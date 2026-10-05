/*
 * plc_can.c — Main PLC CAN gateway: Locomotive (CAN1) -> HUB (CAN2).
 */
#include "plc_can.h"

#include <string.h>
#include "fdcan.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "plc_modbus.h"
#include "plc_fwupd.h"

#define PLC_CAN_LOCO        (&hfdcan1)
#define PLC_CAN_HUB         (&hfdcan2)

#define PLC_CAN_QUEUE_LEN   16u
#define PLC_CAN_POLL_MS     100u

typedef struct
{
  uint8_t data[8];
} PlcCanFrame;

static osMessageQueueId_t s_loco_q;
static volatile uint32_t s_rx_hub;
static volatile uint32_t s_rx_drop;
static PlcAiSnapshot s_snap;

static HAL_StatusTypeDef PlcCan_Start(void)
{
  FDCAN_FilterTypeDef f = {0};

  f.IdType = FDCAN_STANDARD_ID;
  f.FilterIndex = 0;
  f.FilterType = FDCAN_FILTER_DUAL;
  f.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  f.FilterID1 = HVAC_CAN_ID_AI_MEAS;
  f.FilterID2 = HVAC_CAN_ID_AI_MEAS;
  if (HAL_FDCAN_ConfigFilter(PLC_CAN_LOCO, &f) != HAL_OK) return HAL_ERROR;
  /* Extended IDs on CAN1 = firmware update protocol (plc_fwupd). */
  if (HAL_FDCAN_ConfigGlobalFilter(PLC_CAN_LOCO, FDCAN_REJECT, FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK) return HAL_ERROR;

  /* HUB side: nothing is processed yet, frames are only counted. */
  if (HAL_FDCAN_ConfigGlobalFilter(PLC_CAN_HUB, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT,
                                   FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK) return HAL_ERROR;

  if (HAL_FDCAN_ActivateNotification(PLC_CAN_LOCO, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) return HAL_ERROR;
  if (HAL_FDCAN_ActivateNotification(PLC_CAN_HUB, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) return HAL_ERROR;

  if (HAL_FDCAN_Start(PLC_CAN_LOCO) != HAL_OK) return HAL_ERROR;
  if (HAL_FDCAN_Start(PLC_CAN_HUB) != HAL_OK) return HAL_ERROR;
  return HAL_OK;
}

static uint8_t PlcCan_SendHubId(uint32_t id, uint32_t dlc, const uint8_t *d)
{
  FDCAN_TxHeaderTypeDef h = {0};

  h.Identifier = id;
  h.IdType = FDCAN_STANDARD_ID;
  h.TxFrameType = FDCAN_DATA_FRAME;
  h.DataLength = dlc;
  h.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  h.BitRateSwitch = FDCAN_BRS_OFF;
  h.FDFormat = FDCAN_CLASSIC_CAN;
  h.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  h.MessageMarker = 0;

  if (HAL_FDCAN_GetTxFifoFreeLevel(PLC_CAN_HUB) == 0u) return 0u;
  return (HAL_FDCAN_AddMessageToTxFifoQ(PLC_CAN_HUB, &h, (uint8_t *)d) == HAL_OK) ? 1u : 0u;
}

static uint8_t PlcCan_SendHub(const uint8_t d[8])
{
  return PlcCan_SendHubId(HVAC_CAN_ID_AI_MEAS, FDCAN_DLC_BYTES_8, d);
}

static uint8_t PlcCan_SendDoors(uint8_t counter)
{
  PlcDoorsSnapshot s;
  uint8_t d[HVAC_CAN_DOORS_DLC] = {0};

  PlcModbus_GetDoors(&s);
  d[0] = s.link ? (uint8_t)(s.closed_mask & HVAC_DOORS_MASK) : 0u;
  d[1] = s.link ? 0u : HVAC_DOORS_ST_NO_LINK;
  d[2] = counter;
  return PlcCan_SendHubId(HVAC_CAN_ID_DOORS, FDCAN_DLC_BYTES_4, d);
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

void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  FDCAN_RxHeaderTypeDef rh;
  PlcCanFrame f;

  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0u) return;

  while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0u)
  {
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rh, f.data) != HAL_OK) break;

    if (hfdcan == PLC_CAN_LOCO)
    {
      if (rh.IdType == FDCAN_EXTENDED_ID)
      {
        if (rh.RxFrameType == FDCAN_DATA_FRAME && rh.DataLength == FDCAN_DLC_BYTES_8)
        {
          PlcFwupd_OnRxIsr(rh.Identifier, f.data);
        }
      }
      else if (rh.IdType == FDCAN_STANDARD_ID && rh.Identifier == HVAC_CAN_ID_AI_MEAS &&
          rh.DataLength == FDCAN_DLC_BYTES_8)
      {
        if (s_loco_q == NULL || osMessageQueuePut(s_loco_q, &f, 0u, 0u) != osOK) s_rx_drop++;
      }
    }
    else if (hfdcan == PLC_CAN_HUB)
    {
      s_rx_hub++;
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

void PlcCan_Task(void)
{
  uint32_t last_rx;
  uint32_t last_no_link_tx;
  uint32_t last_doors_tx;
  uint8_t no_link_counter = 0u;
  uint8_t doors_counter = 0u;

  s_loco_q = osMessageQueueNew(PLC_CAN_QUEUE_LEN, sizeof(PlcCanFrame), NULL);

  /* Display starts with "no link" until the first Locomotive frame. */
  memset(&s_snap, 0, sizeof(s_snap));
  s_snap.meas.status = HVAC_AI_ST_NO_LINK;
  s_snap.init_ok = (s_loco_q != NULL && PlcCan_Start() == HAL_OK) ? 1u : 0u;

  last_rx = osKernelGetTickCount() - HVAC_AI_LINK_TIMEOUT_MS;
  last_no_link_tx = osKernelGetTickCount() - HVAC_AI_NO_LINK_PERIOD_MS;
  last_doors_tx = osKernelGetTickCount();

  for (;;)
  {
    PlcCanFrame f;
    uint32_t now;

    if (s_loco_q != NULL && osMessageQueueGet(s_loco_q, &f, NULL, PLC_CAN_POLL_MS) == osOK)
    {
      HvacAiMeas m;
      uint8_t sent;

      hvac_can_ai_decode(f.data, &m);
      sent = PlcCan_SendHub(f.data);
      last_rx = osKernelGetTickCount();

      taskENTER_CRITICAL();
      s_snap.meas = m;
      s_snap.rx_loco++;
      if (sent) s_snap.tx_hub++; else s_snap.tx_hub_err++;
      taskEXIT_CRITICAL();
    }
    else if (s_loco_q == NULL)
    {
      osDelay(PLC_CAN_POLL_MS);
    }

    now = osKernelGetTickCount();

    if ((now - last_rx) >= HVAC_AI_LINK_TIMEOUT_MS &&
        (now - last_no_link_tx) >= HVAC_AI_NO_LINK_PERIOD_MS)
    {
      HvacAiMeas m = {0};
      uint8_t d[8];
      uint8_t sent;

      m.status = HVAC_AI_ST_NO_LINK;
      m.counter = no_link_counter++;
      hvac_can_ai_encode(&m, d);
      sent = PlcCan_SendHub(d);
      last_no_link_tx = now;

      taskENTER_CRITICAL();
      s_snap.meas = m;
      if (sent) s_snap.tx_hub++; else s_snap.tx_hub_err++;
      taskEXIT_CRITICAL();
    }

    if (s_snap.init_ok && (now - last_doors_tx) >= HVAC_DOORS_PERIOD_MS)
    {
      uint8_t sent = PlcCan_SendDoors(doors_counter++);
      last_doors_tx = now;

      taskENTER_CRITICAL();
      if (sent) s_snap.tx_hub++; else s_snap.tx_hub_err++;
      taskEXIT_CRITICAL();
    }

    {
      uint8_t loco_off = PlcCan_RecoverBusOff(PLC_CAN_LOCO);
      uint8_t hub_off = PlcCan_RecoverBusOff(PLC_CAN_HUB);

      taskENTER_CRITICAL();
      s_snap.age_ms = now - last_rx;
      s_snap.loco_link = (s_snap.age_ms < HVAC_AI_LINK_TIMEOUT_MS) ? 1u : 0u;
      s_snap.loco_bus_off += loco_off;
      s_snap.hub_bus_off += hub_off;
      taskEXIT_CRITICAL();
    }
  }
}
