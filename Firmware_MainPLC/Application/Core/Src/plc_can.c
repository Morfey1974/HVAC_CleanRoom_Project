/*
 * plc_can.c — Main PLC CAN gateway: CAN1 / CAN3 -> CAN2 / CAN3, doors master on CAN3.
 */
#include "plc_can.h"

#include <string.h>
#include "fdcan.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "plc_modbus.h"
#include "plc_fwupd.h"
#include "plc_cfg.h"
#include "plc_id.h"

#define PLC_CAN_LOCO        (&hfdcan1)
#define PLC_CAN_HUB         (&hfdcan2)
#define PLC_CAN_BUS3        (&hfdcan3)

#define PLC_CAN_QUEUE_LEN   16u
#define PLC_CAN_POLL_MS     100u

typedef struct
{
  uint8_t bus;
  uint8_t data[8];
} PlcCanFrame;

typedef struct
{
  uint32_t t;
  uint16_t id;
  uint8_t  dlc;
  uint8_t  d[8];
} Can3Rx;

static osMessageQueueId_t s_loco_q;
static volatile uint32_t s_rx_hub;
static volatile uint32_t s_rx_drop;
static PlcAiSnapshot s_snap;

static PlcCan3Snapshot s_c3;
static Can3Rx s_c3_log[PLC_CAN3_LOG_LEN];
static uint8_t s_c3_head;
static uint8_t s_c3_n;
static uint32_t s_dm_state_t;
static uint32_t s_dm_ack_t;
static uint8_t s_dm_seq;

static const uint8_t s_dlc_len[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 8, 8, 8, 8, 8, 8, 8};

static HAL_StatusTypeDef PlcCan_Start(void)
{
  FDCAN_FilterTypeDef f = {0};

  /* One standard filter (CubeMX: 1 on FDCAN1): AI_MEAS .. ID_HERE, the ISR picks the IDs it needs. */
  f.IdType = FDCAN_STANDARD_ID;
  f.FilterIndex = 0;
  f.FilterType = FDCAN_FILTER_RANGE;
  f.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  f.FilterID1 = HVAC_CAN_ID_AI_MEAS;
  f.FilterID2 = HVAC_CAN_ID_ID_HERE;
  if (HAL_FDCAN_ConfigFilter(PLC_CAN_LOCO, &f) != HAL_OK) return HAL_ERROR;
  /* Extended IDs = firmware update protocol (plc_fwupd) on every bus. */
  if (HAL_FDCAN_ConfigGlobalFilter(PLC_CAN_LOCO, FDCAN_REJECT, FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK) return HAL_ERROR;
  if (HAL_FDCAN_ConfigGlobalFilter(PLC_CAN_HUB, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK) return HAL_ERROR;

  if (HAL_FDCAN_ActivateNotification(PLC_CAN_LOCO, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) return HAL_ERROR;
  if (HAL_FDCAN_ActivateNotification(PLC_CAN_HUB, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) return HAL_ERROR;

  if (HAL_FDCAN_Start(PLC_CAN_LOCO) != HAL_OK) return HAL_ERROR;
  if (HAL_FDCAN_Start(PLC_CAN_HUB) != HAL_OK) return HAL_ERROR;
  return HAL_OK;
}

/* CAN3 has no standard filters in CubeMX: every frame goes to FIFO0, the ISR sorts them. */
static HAL_StatusTypeDef PlcCan_Start3(void)
{
  if (HAL_FDCAN_ConfigGlobalFilter(PLC_CAN_BUS3, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK) return HAL_ERROR;
  if (HAL_FDCAN_ActivateNotification(PLC_CAN_BUS3, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) return HAL_ERROR;
  return HAL_FDCAN_Start(PLC_CAN_BUS3);
}

static uint8_t PlcCan_Put(FDCAN_HandleTypeDef *can, uint32_t id, uint8_t is_ext, uint32_t dlc, const uint8_t *d)
{
  FDCAN_TxHeaderTypeDef h = {0};
  uint8_t ok = 0u;

  if (HAL_FDCAN_GetState(can) != HAL_FDCAN_STATE_BUSY) return 0u;
  h.Identifier = id;
  h.IdType = is_ext ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
  h.TxFrameType = FDCAN_DATA_FRAME;
  h.DataLength = dlc;
  h.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  h.BitRateSwitch = FDCAN_BRS_OFF;
  h.FDFormat = FDCAN_CLASSIC_CAN;
  h.TxEventFifoControl = FDCAN_NO_TX_EVENTS;

  /* Several tasks write each bus: the free-level check and the FIFO put must not interleave. */
  taskENTER_CRITICAL();
  if (HAL_FDCAN_GetTxFifoFreeLevel(can) > 0u &&
      HAL_FDCAN_AddMessageToTxFifoQ(can, &h, (uint8_t *)d) == HAL_OK) ok = 1u;
  if (can == PLC_CAN_BUS3)
  {
    if (ok) s_c3.tx++; else s_c3.tx_err++;
  }
  taskEXIT_CRITICAL();
  return ok;
}

static FDCAN_HandleTypeDef *PlcCan_Handle(uint8_t bus)
{
  if (bus == PLC_CAN_BUS_HUB) return PLC_CAN_HUB;
  if (bus == PLC_CAN_BUS_BUS) return PLC_CAN_BUS3;
  return PLC_CAN_LOCO;
}

/* Frame for the displays: CAN2 and CAN3 (skip_bus = the bus it came from, 0 = none). */
static void PlcCan_SendDisplays(uint32_t id, uint32_t dlc, const uint8_t *d, uint8_t skip_bus)
{
  uint8_t sent = PlcCan_Put(PLC_CAN_HUB, id, 0u, dlc, d);

  if (s_c3.ok && skip_bus != PLC_CAN_BUS_BUS) (void)PlcCan_Put(PLC_CAN_BUS3, id, 0u, dlc, d);
  taskENTER_CRITICAL();
  if (sent) s_snap.tx_hub++; else s_snap.tx_hub_err++;
  taskEXIT_CRITICAL();
}

static void PlcCan_SendDoors(uint8_t counter)
{
  PlcDoorsSnapshot s;
  uint8_t d[HVAC_CAN_DOORS_DLC] = {0};
  uint8_t src;

  PlcModbus_GetDoors(&s);
  if (s_c3.dm.link)
  {
    src = PLC_DOORS_SRC_CAN3;
    d[0] = (uint8_t)(s_c3.dm.closed & HVAC_DOORS_MASK);
  }
  else if (s.link)
  {
    src = PLC_DOORS_SRC_MODBUS;
    d[0] = (uint8_t)(s.closed_mask & HVAC_DOORS_MASK);
  }
  else
  {
    src = PLC_DOORS_SRC_NONE;
    d[1] = HVAC_DOORS_ST_NO_LINK;
  }
  d[2] = counter;
  s_snap.doors_src = src;
  PlcCan_SendDisplays(HVAC_CAN_ID_DOORS, FDCAN_DLC_BYTES_4, d, 0u);
}

static void PlcCan_SendDmPlc(uint8_t counter)
{
  PlcCfgSummary cs;
  uint8_t d[8] = {0};

  PlcCfg_GetSummary(&cs);
  d[0] = counter;
  d[1] = (cs.state == PLC_CFG_ST_OK) ? HVAC_DM_PLC_CFG_OK : 0u;
  d[2] = (uint8_t)(PLC_FW_VERSION & 0xFFu);
  d[3] = (uint8_t)(PLC_FW_VERSION >> 8);
  if (PlcCan_Put(PLC_CAN_BUS3, HVAC_CAN_ID_DM_PLC, 0u, FDCAN_DLC_BYTES_8, d))
  {
    taskENTER_CRITICAL();
    s_c3.dm.tx_plc++;
    taskEXIT_CRITICAL();
  }
}

/* After bus-off the FDCAN stays in INIT until software clears it. */
static uint8_t PlcCan_RecoverBusOff(FDCAN_HandleTypeDef *h)
{
  FDCAN_ProtocolStatusTypeDef ps;

  if (HAL_FDCAN_GetProtocolStatus(h, &ps) != HAL_OK) return 0u;
  if (ps.BusOff == 0u) return 0u;
  CLEAR_BIT(h->Instance->CCCR, FDCAN_CCCR_INIT);
  return 1u;
}

static void PlcCan_QueueAi(uint8_t bus, const uint8_t data[8])
{
  PlcCanFrame f;

  f.bus = bus;
  memcpy(f.data, data, 8u);
  if (s_loco_q == NULL || osMessageQueuePut(s_loco_q, &f, 0u, 0u) != osOK) s_rx_drop++;
}

/* CAN3 standard frame, from the ISR. */
static void PlcCan_OnCan3Std(const FDCAN_RxHeaderTypeDef *rh, const uint8_t d[8])
{
  uint8_t len = s_dlc_len[rh->DataLength & 0x0Fu];
  uint8_t len8 = (len == 8u) ? 1u : 0u;
  uint32_t now = osKernelGetTickCount();
  Can3Rx *e = &s_c3_log[s_c3_head];

  s_c3.rx++;
  e->t = now;
  e->id = (uint16_t)rh->Identifier;
  e->dlc = len;
  memcpy(e->d, d, 8u);
  s_c3_head = (uint8_t)((s_c3_head + 1u) % PLC_CAN3_LOG_LEN);
  if (s_c3_n < PLC_CAN3_LOG_LEN) s_c3_n++;

  if (!len8) return;
  switch (rh->Identifier)
  {
    case HVAC_CAN_ID_ID_HERE:    PlcId_OnHereIsr(PLC_CAN_BUS_BUS, d); break;
    case HVAC_CAN_ID_AI_MEAS:    PlcCan_QueueAi(PLC_CAN_BUS_BUS, d); break;
    case HVAC_CAN_ID_CFG_STATUS: PlcCfg_OnStatusIsr(d); break;
    case HVAC_CAN_ID_DM_STATE:
      s_c3.dm.closed = d[0];
      s_c3.dm.locked = d[1];
      s_c3.dm.fault = d[2];
      s_c3.dm.status = d[3];
      s_c3.dm.counter = d[4];
      s_c3.dm.doors = d[5];
      s_c3.dm.version = (uint16_t)(d[6] | ((uint16_t)d[7] << 8));
      s_c3.dm.rx_state++;
      s_c3.dm.link = 1u;
      s_dm_state_t = now;
      break;
    case HVAC_CAN_ID_DM_ACK:
      s_c3.dm.ack_seq = d[0];
      s_c3.dm.ack_cmd = d[1];
      s_c3.dm.ack_res = d[2];
      s_c3.dm.ack_valid = 1u;
      s_dm_ack_t = now;
      break;
    default: break;
  }
}

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  FDCAN_RxHeaderTypeDef rh;
  uint8_t d[8];

  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0u) return;

  while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0u)
  {
    uint8_t len8;

    memset(d, 0, sizeof(d));
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rh, d) != HAL_OK) break;
    len8 = (rh.DataLength == FDCAN_DLC_BYTES_8) ? 1u : 0u;

    if (hfdcan == PLC_CAN_LOCO)
    {
      if (rh.IdType == FDCAN_EXTENDED_ID)
      {
        if (rh.RxFrameType == FDCAN_DATA_FRAME && len8) PlcFwupd_OnRxIsr(PLC_CAN_BUS_LOCO, rh.Identifier, d);
      }
      else if (rh.Identifier == HVAC_CAN_ID_ID_HERE && len8)    PlcId_OnHereIsr(PLC_CAN_BUS_LOCO, d);
      else if (rh.Identifier == HVAC_CAN_ID_AI_MEAS && len8)    PlcCan_QueueAi(PLC_CAN_BUS_LOCO, d);
      else if (rh.Identifier == HVAC_CAN_ID_CFG_STATUS && len8) PlcCfg_OnStatusIsr(d);
    }
    else if (hfdcan == PLC_CAN_HUB)
    {
      s_rx_hub++;
      if (rh.IdType == FDCAN_EXTENDED_ID)
      {
        if (rh.RxFrameType == FDCAN_DATA_FRAME && len8) PlcFwupd_OnRxIsr(PLC_CAN_BUS_HUB, rh.Identifier, d);
      }
      else if (rh.Identifier == HVAC_CAN_ID_ID_HERE && len8)    PlcId_OnHereIsr(PLC_CAN_BUS_HUB, d);
      else if (rh.Identifier == HVAC_CAN_ID_CFG_STATUS && len8) PlcCfg_OnStatusIsr(d);
    }
    else if (hfdcan == PLC_CAN_BUS3)
    {
      if (rh.IdType == FDCAN_EXTENDED_ID)
      {
        s_c3.rx_ext++;
        if (rh.RxFrameType == FDCAN_DATA_FRAME && len8) PlcFwupd_OnRxIsr(PLC_CAN_BUS_BUS, rh.Identifier, d);
      }
      else
      {
        PlcCan_OnCan3Std(&rh, d);
      }
    }
  }
}

void PlcCan_GetSnapshot(PlcAiSnapshot *out)
{
  taskENTER_CRITICAL();
  *out = s_snap;
  out->rx_hub = s_rx_hub;
  out->rx_drop = s_rx_drop;
  taskEXIT_CRITICAL();
}

uint8_t PlcCan_GetCan3(PlcCan3Snapshot *out, PlcCan3Frame *frames, uint8_t max)
{
  FDCAN_ProtocolStatusTypeDef ps;
  FDCAN_ErrorCountersTypeDef ec;
  uint32_t now = osKernelGetTickCount();
  uint8_t n = 0u;

  taskENTER_CRITICAL();
  *out = s_c3;
  out->dm.age_ms = (s_c3.dm.rx_state != 0u) ? (now - s_dm_state_t) : 0xFFFFFFFFu;
  out->dm.ack_age_ms = s_c3.dm.ack_valid ? (now - s_dm_ack_t) : 0xFFFFFFFFu;
  for (; n < max && n < s_c3_n; n++)
  {
    const Can3Rx *e = &s_c3_log[(s_c3_head + PLC_CAN3_LOG_LEN - 1u - n) % PLC_CAN3_LOG_LEN];

    frames[n].age_ms = now - e->t;
    frames[n].id = e->id;
    frames[n].dlc = e->dlc;
    memcpy(frames[n].d, e->d, 8u);
  }
  taskEXIT_CRITICAL();

  if (out->ok && HAL_FDCAN_GetProtocolStatus(PLC_CAN_BUS3, &ps) == HAL_OK)
  {
    out->err_passive = (ps.ErrorPassive != 0u || ps.BusOff != 0u) ? 1u : 0u;
  }
  if (out->ok && HAL_FDCAN_GetErrorCounters(PLC_CAN_BUS3, &ec) == HAL_OK)
  {
    out->tec = (uint8_t)ec.TxErrorCnt;
    out->rec = (uint8_t)ec.RxErrorCnt;
  }
  return n;
}

uint8_t PlcCan_SendLoco(uint32_t id, uint8_t is_ext, const uint8_t d[8])
{
  return PlcCan_Put(PLC_CAN_LOCO, id, is_ext, FDCAN_DLC_BYTES_8, d);
}

uint8_t PlcCan_Send(uint8_t bus, uint32_t id, uint8_t is_ext, const uint8_t d[8])
{
  if (bus == PLC_CAN_BUS_BUS && !s_c3.ok) return 0u;
  return PlcCan_Put(PlcCan_Handle(bus), id, is_ext, FDCAN_DLC_BYTES_8, d);
}

uint8_t PlcCan_SendCan3Raw(uint16_t id, uint8_t dlc, const uint8_t *d)
{
  uint8_t buf[8] = {0};

  if (!s_c3.ok || id > 0x7FFu || dlc > 8u) return 0u;
  memcpy(buf, d, dlc);
  /* FDCAN_DLC_BYTES_n == n for classic lengths 0..8. */
  return PlcCan_Put(PLC_CAN_BUS3, id, 0u, dlc, buf);
}

uint8_t PlcCan_DoorsCmd(uint8_t cmd, uint8_t mask, uint8_t *seq)
{
  uint8_t d[8] = {0};
  uint8_t s;

  if (!s_c3.ok) return 0u;
  taskENTER_CRITICAL();
  s = ++s_dm_seq;
  s_c3.dm.cmd_seq = s;
  s_c3.dm.cmd_code = cmd;
  taskEXIT_CRITICAL();
  d[0] = cmd;
  d[1] = mask;
  d[2] = s;
  if (seq != NULL) *seq = s;
  return PlcCan_Put(PLC_CAN_BUS3, HVAC_CAN_ID_DM_CMD, 0u, FDCAN_DLC_BYTES_8, d);
}

uint8_t PlcCan_HubBusOk(void)
{
  FDCAN_ProtocolStatusTypeDef ps;

  if (!s_snap.init_ok || HAL_FDCAN_GetProtocolStatus(PLC_CAN_HUB, &ps) != HAL_OK) return 0u;
  return (ps.ErrorPassive == 0u && ps.BusOff == 0u) ? 1u : 0u;
}

void PlcCan_Task(void)
{
  uint32_t last_rx;
  uint32_t last_no_link_tx;
  uint32_t last_doors_tx;
  uint32_t last_dm_tx;
  uint8_t no_link_counter = 0u;
  uint8_t doors_counter = 0u;
  uint8_t dm_counter = 0u;

  s_loco_q = osMessageQueueNew(PLC_CAN_QUEUE_LEN, sizeof(PlcCanFrame), NULL);

  /* Display starts with "no link" until the first AI frame. */
  memset(&s_snap, 0, sizeof(s_snap));
  s_snap.meas.status = HVAC_AI_ST_NO_LINK;
  s_snap.init_ok = (s_loco_q != NULL && PlcCan_Start() == HAL_OK) ? 1u : 0u;
  s_c3.ok = (PlcCan_Start3() == HAL_OK) ? 1u : 0u;

  last_rx = osKernelGetTickCount() - HVAC_AI_LINK_TIMEOUT_MS;
  last_no_link_tx = osKernelGetTickCount() - HVAC_AI_NO_LINK_PERIOD_MS;
  last_doors_tx = osKernelGetTickCount();
  last_dm_tx = osKernelGetTickCount();

  for (;;)
  {
    PlcCanFrame f;
    uint32_t now;

    if (s_loco_q != NULL && osMessageQueueGet(s_loco_q, &f, NULL, PLC_CAN_POLL_MS) == osOK)
    {
      HvacAiMeas m;

      hvac_can_ai_decode(f.data, &m);
      PlcCan_SendDisplays(HVAC_CAN_ID_AI_MEAS, FDCAN_DLC_BYTES_8, f.data, f.bus);
      last_rx = osKernelGetTickCount();

      taskENTER_CRITICAL();
      s_snap.meas = m;
      s_snap.rx_loco++;
      taskEXIT_CRITICAL();
    }
    else if (s_loco_q == NULL)
    {
      osDelay(PLC_CAN_POLL_MS);
    }

    now = osKernelGetTickCount();

    if (s_snap.init_ok) PlcCfg_Poll();

    if ((now - last_rx) >= HVAC_AI_LINK_TIMEOUT_MS &&
        (now - last_no_link_tx) >= HVAC_AI_NO_LINK_PERIOD_MS)
    {
      HvacAiMeas m = {0};
      uint8_t d[8];

      m.status = HVAC_AI_ST_NO_LINK;
      m.counter = no_link_counter++;
      hvac_can_ai_encode(&m, d);
      PlcCan_SendDisplays(HVAC_CAN_ID_AI_MEAS, FDCAN_DLC_BYTES_8, d, 0u);
      last_no_link_tx = now;

      taskENTER_CRITICAL();
      s_snap.meas = m;
      taskEXIT_CRITICAL();
    }

    taskENTER_CRITICAL();
    if (s_c3.dm.link && (now - s_dm_state_t) >= HVAC_DM_TIMEOUT_MS) s_c3.dm.link = 0u;
    taskEXIT_CRITICAL();

    if (s_snap.init_ok && (now - last_doors_tx) >= HVAC_DOORS_PERIOD_MS)
    {
      PlcCan_SendDoors(doors_counter++);
      last_doors_tx = now;
    }

    if (s_c3.ok && (now - last_dm_tx) >= HVAC_DM_PLC_PERIOD_MS)
    {
      PlcCan_SendDmPlc(dm_counter++);
      last_dm_tx = now;
    }

    {
      uint8_t loco_off = PlcCan_RecoverBusOff(PLC_CAN_LOCO);
      uint8_t hub_off = PlcCan_RecoverBusOff(PLC_CAN_HUB);
      uint8_t bus3_off = s_c3.ok ? PlcCan_RecoverBusOff(PLC_CAN_BUS3) : 0u;

      taskENTER_CRITICAL();
      s_snap.age_ms = now - last_rx;
      s_snap.loco_link = (s_snap.age_ms < HVAC_AI_LINK_TIMEOUT_MS) ? 1u : 0u;
      s_snap.loco_bus_off += loco_off;
      s_snap.hub_bus_off += hub_off;
      s_c3.bus_off += bus3_off;
      taskEXIT_CRITICAL();
    }
  }
}
