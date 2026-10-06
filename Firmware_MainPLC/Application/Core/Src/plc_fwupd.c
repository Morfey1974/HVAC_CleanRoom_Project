/*
 * plc_fwupd.c — Main PLC firmware update master on CAN1 and CAN2.
 */
#include "plc_fwupd.h"

#include <string.h>
#include "fdcan.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "fwupd/fwupd_proto.h"
#include "plc_fwstore.h"
#include "plc_can.h"

#define PLC_FWUPD_CAN            (&hfdcan1)
#define PLC_FWUPD_QUEUE_LEN      48u

#define PLC_FWUPD_DISCOVER_MS    10000u
#define PLC_FWUPD_LINK_MS        6000u
#define PLC_FWUPD_LEARN_MS       15000u /* nodes seen this long after PLC start are "known" */
#define PLC_FWUPD_SAME_ANN       3u

#define PLC_FWUPD_ATTEMPTS       3u
#define PLC_FWUPD_CONNECT_TRIES  5u
#define PLC_FWUPD_BLOCK_RETRIES  5u
#define PLC_FWUPD_VERIFY_TRIES   3u

#define PLC_FWUPD_T_CONNECT_MS   1500u
#define PLC_FWUPD_T_ERASE_MS     30000u /* display (STM32H7): up to 6 sectors of 128 KB */
#define PLC_FWUPD_T_BLOCK_MS     800u
#define PLC_FWUPD_T_VERIFY_MS    3000u
#define PLC_FWUPD_T_START_MS     1000u
#define PLC_FWUPD_T_CHECK_MS     8000u
#define PLC_FWUPD_T_TX_MS        100u

typedef struct
{
  uint32_t id;
  uint8_t  d[8];
  uint8_t  bus;
} FwFrame;

volatile PlcFwupdStatus g_plc_fwupd;
volatile uint32_t g_plc_fwupd_block_delay_ms;

static osMessageQueueId_t s_q;
static uint8_t s_last_ann[PLC_FWUPD_MAX_NODES][8];
static uint8_t s_run_done[PLC_FWUPD_MAX_NODES];
static uint8_t s_blk[FWUPD_BLOCK_MAX] __attribute__((aligned(8)));

static volatile uint8_t s_run_req_type;
static volatile uint8_t s_run_req_mode;
static volatile uint8_t s_run_req_rollback;
static volatile uint8_t s_run_cancel;
static FwStoreSlot s_run_img;

static PlcFwupdLog s_log[PLC_FWUPD_LOG_LEN];
static uint32_t s_log_next = 1u;

static const osThreadAttr_t s_task_attr = {
  .name = "fwupdTask",
  .stack_size = 768 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

/* ---------- log ---------- */

void PlcFwupd_Log(uint8_t event, uint32_t tag, uint8_t module_type, uint8_t err, uint8_t step,
                  uint16_t ver_from, uint16_t ver_to)
{
  PlcFwupdLog e;

  e.t_ms = osKernelGetTickCount();
  e.tag = tag;
  e.module_type = module_type;
  e.event = event;
  e.err = err;
  e.step = step;
  e.ver_from = ver_from;
  e.ver_to = ver_to;

  taskENTER_CRITICAL();
  e.id = s_log_next++;
  s_log[e.id % PLC_FWUPD_LOG_LEN] = e;
  taskEXIT_CRITICAL();
}

uint32_t PlcFwupd_GetLog(uint32_t after_id, PlcFwupdLog *out, uint32_t max)
{
  uint32_t n = 0u;
  uint32_t first;
  uint32_t next;

  taskENTER_CRITICAL();
  next = s_log_next;
  first = (next > PLC_FWUPD_LOG_LEN) ? (next - PLC_FWUPD_LOG_LEN) : 1u;
  if (after_id + 1u > first) first = after_id + 1u;
  for (uint32_t id = first; id < next && n < max; id++) out[n++] = s_log[id % PLC_FWUPD_LOG_LEN];
  taskEXIT_CRITICAL();
  return n;
}

/* ---------- CAN ---------- */

void PlcFwupd_OnRxIsr(uint8_t bus, uint32_t id, const uint8_t data[8])
{
  FwFrame f;

  g_plc_fwupd.rx_frames++;
  if (s_q == NULL) return;
  f.id = id;
  f.bus = bus;
  memcpy(f.d, data, 8u);
  if (osMessageQueuePut(s_q, &f, 0u, 0u) != osOK) g_plc_fwupd.rx_drop++;
}

static uint8_t Fw_SendBus(uint8_t bus, uint32_t id, const uint8_t d[8])
{
  uint32_t t0 = osKernelGetTickCount();

  while (!PlcCan_Send(bus, id, 1u, d))
  {
    if ((osKernelGetTickCount() - t0) >= PLC_FWUPD_T_TX_MS) return 0u;
    osDelay(1);
  }
  return 1u;
}

static int Fw_NodeIndex(uint32_t tag, uint8_t create);

/* To one node: on the bus it announced on. To all nodes: on every bus. */
static uint8_t Fw_Send(uint32_t type, uint32_t tag, const uint8_t d[8])
{
  int i;

  if (HAL_FDCAN_GetState(PLC_FWUPD_CAN) != HAL_FDCAN_STATE_BUSY) return 0u;
  if (tag == FWUPD_NODE_ALL)
  {
    uint8_t ok = 0u;

    for (uint8_t bus = 1u; bus <= PLC_CAN_BUSES; bus++) ok |= Fw_SendBus(bus, FWUPD_ID(type, tag), d);
    return ok;
  }
  i = Fw_NodeIndex(tag, 0u);
  return Fw_SendBus((i >= 0) ? PlcCan_LineBus(g_plc_fwupd.nodes[i].bus) : PLC_CAN_BUS_LOCO, FWUPD_ID(type, tag), d);
}

/* ---------- node table ---------- */

static int Fw_NodeIndex(uint32_t tag, uint8_t create)
{
  int free_i = -1;

  for (int i = 0; i < (int)PLC_FWUPD_MAX_NODES; i++)
  {
    if (g_plc_fwupd.nodes[i].tag == tag) return i;
    if (free_i < 0 && g_plc_fwupd.nodes[i].tag == 0u) free_i = i;
  }
  if (!create || free_i < 0) return -1;

  memset((void *)&g_plc_fwupd.nodes[free_i], 0, sizeof(PlcFwupdNode));
  memset(s_last_ann[free_i], 0, 8u);
  s_run_done[free_i] = 0u;
  g_plc_fwupd.nodes[free_i].tag = tag;
  g_plc_fwupd.nodes[free_i].learned = (osKernelGetTickCount() < PLC_FWUPD_LEARN_MS) ? 1u : 0u;
  return free_i;
}

static void Fw_OnAnnounce(uint32_t tag, uint8_t bus, const uint8_t d[8])
{
  int i = Fw_NodeIndex(tag, 1u);
  volatile PlcFwupdNode *n;
  FwStoreSlot cur;
  uint8_t is_new;
  uint8_t old_state;

  if (i < 0) return;
  n = &g_plc_fwupd.nodes[i];
  is_new = (n->last_seen == 0u) ? 1u : 0u;
  old_state = n->state;

  if (n->link && memcmp(s_last_ann[i], d, 8u) == 0)
  {
    if (n->same_ann < 255u) n->same_ann++;
  }
  else
  {
    n->same_ann = 1u;
    memcpy(s_last_ann[i], d, 8u);
  }

  n->module_type = d[0];
  n->board_rev = d[1];
  n->state = d[2];
  n->boot_version = d[3];
  n->app_version = fwupd_get16(&d[4]);
  n->boot_fails = d[6];
  n->link = 1u;
  n->bus = bus;
  n->last_seen = osKernelGetTickCount();
  n->version_mismatch = (n->state == FWUPD_ST_APP_RUNNING && FwStore_Find(n->module_type, FWSTORE_ROLE_CURRENT, &cur) &&
                         n->app_version != cur.version) ? 1u : 0u;

  if (is_new)
  {
    PlcFwupd_Log(PLC_FWUPD_EV_NODE_NEW, tag, n->module_type, n->state, 0u, n->app_version, n->app_version);
  }
  else if (old_state != n->state && !g_plc_fwupd.busy)
  {
    PlcFwupd_Log(PLC_FWUPD_EV_NODE_STATE, tag, n->module_type, 0u, 0u, old_state, n->state);
  }
}

static void Fw_Record(const FwFrame *f)
{
  if (FWUPD_ID_TYPE(f->id) == FWUPD_MSG_ANNOUNCE && FWUPD_ID_NODE(f->id) != FWUPD_NODE_ALL)
  {
    Fw_OnAnnounce(FWUPD_ID_NODE(f->id), f->bus, f->d);
  }
}

static void Fw_UpdateLinks(uint32_t now)
{
  for (uint32_t i = 0u; i < PLC_FWUPD_MAX_NODES; i++)
  {
    volatile PlcFwupdNode *n = &g_plc_fwupd.nodes[i];

    if (n->tag != 0u && n->link && (now - n->last_seen) >= PLC_FWUPD_LINK_MS)
    {
      n->link = 0u;
      n->same_ann = 0u;
      PlcFwupd_Log(PLC_FWUPD_EV_NODE_LOST, n->tag, n->module_type, 0u, 0u, n->app_version, n->app_version);
    }
  }
}

/* ---------- waiting for answers ---------- */

/* Waits for a frame of this type from the node; announces are always recorded. */
static uint8_t Fw_Wait(uint32_t tag, uint32_t type, uint32_t timeout_ms, uint8_t out[8])
{
  uint32_t t0 = osKernelGetTickCount();

  for (;;)
  {
    uint32_t el = osKernelGetTickCount() - t0;
    FwFrame f;

    if (el >= timeout_ms) return 0u;
    if (osMessageQueueGet(s_q, &f, NULL, timeout_ms - el) != osOK) return 0u;
    Fw_Record(&f);
    if (FWUPD_ID_NODE(f.id) == tag && FWUPD_ID_TYPE(f.id) == type)
    {
      memcpy(out, f.d, 8u);
      return 1u;
    }
  }
}

static uint8_t Fw_WaitAck(uint32_t tag, uint8_t cmd, uint8_t check_param, uint32_t param,
                          uint32_t timeout_ms, uint8_t *err)
{
  uint32_t t0 = osKernelGetTickCount();
  uint8_t d[8];

  for (;;)
  {
    uint32_t el = osKernelGetTickCount() - t0;

    if (el >= timeout_ms) return 0u;
    if (!Fw_Wait(tag, FWUPD_MSG_ACK, timeout_ms - el, d)) return 0u;
    if (d[0] != cmd) continue;
    if (check_param && fwupd_get32(&d[2]) != param) continue; /* late ACK of an earlier block */
    *err = d[1];
    return 1u;
  }
}

/* Waits for an announce whose state is accepted by want_boot: 1 = any bootloader state, 0 = application. */
static uint8_t Fw_WaitState(uint32_t tag, uint8_t want_boot, uint32_t timeout_ms, uint8_t out[8])
{
  uint32_t t0 = osKernelGetTickCount();

  for (;;)
  {
    uint32_t el = osKernelGetTickCount() - t0;

    if (el >= timeout_ms) return 0u;
    if (!Fw_Wait(tag, FWUPD_MSG_ANNOUNCE, timeout_ms - el, out)) return 0u;
    if (want_boot ? (out[2] != FWUPD_ST_APP_RUNNING) : (out[2] == FWUPD_ST_APP_RUNNING)) return 1u;
  }
}

/* ---------- update steps ---------- */

static uint8_t Fw_Connect(uint32_t tag, uint8_t *err)
{
  uint8_t d[8];

  for (uint32_t t = 0u; t < PLC_FWUPD_CONNECT_TRIES; t++)
  {
    fwupd_put32(&d[0], FWUPD_CONNECT_MAGIC);
    fwupd_put32(&d[4], FWUPD_CONNECT_KEY(tag));
    if (!Fw_Send(FWUPD_MSG_CONNECT, tag, d)) { *err = PLC_FWUPD_E_TX; continue; }
    if (Fw_WaitState(tag, 1u, PLC_FWUPD_T_CONNECT_MS, d)) return 1u;
    *err = PLC_FWUPD_E_TIMEOUT;
  }
  return 0u;
}

static uint8_t Fw_Erase(uint32_t tag, uint32_t size, uint8_t *err)
{
  uint8_t d[8] = {0};
  uint8_t e;

  fwupd_put32(&d[0], size);
  if (!Fw_Send(FWUPD_MSG_ERASE, tag, d)) { *err = PLC_FWUPD_E_TX; return 0u; }
  if (!Fw_WaitAck(tag, FWUPD_MSG_ERASE, 1u, size, PLC_FWUPD_T_ERASE_MS, &e)) { *err = PLC_FWUPD_E_TIMEOUT; return 0u; }
  if (e != FWUPD_ERR_OK) { *err = e; return 0u; }
  return 1u;
}

static uint8_t Fw_SendBlock(uint32_t tag, uint32_t off, uint32_t len)
{
  uint8_t d[8];
  uint16_t crc = fwupd_crc16(s_blk, len);

  fwupd_put32(&d[0], off);
  d[4] = (uint8_t)len;
  d[5] = (uint8_t)(len >> 8);
  d[6] = (uint8_t)crc;
  d[7] = (uint8_t)(crc >> 8);
  if (!Fw_Send(FWUPD_MSG_BLOCK, tag, d)) return 0u;
  for (uint32_t i = 0u; i < len; i += 8u)
  {
    if (!Fw_Send(FWUPD_MSG_DATA, tag, &s_blk[i])) return 0u;
  }
  return 1u;
}

static uint8_t Fw_Blocks(uint32_t tag, const FwStoreSlot *img, uint8_t *err)
{
  uint32_t size = img->size;

  for (uint32_t off = 0u; off < size; )
  {
    uint32_t len = (size - off > FWUPD_BLOCK_MAX) ? FWUPD_BLOCK_MAX : (size - off);
    uint32_t r;

    if (!FwStore_ReadImage(img, off, s_blk, len)) { *err = PLC_FWUPD_E_IMAGE; return 0u; }

    for (r = 0u; r <= PLC_FWUPD_BLOCK_RETRIES; r++)
    {
      uint8_t e;

      if (r > 0u) g_plc_fwupd.block_retries++;
      if (!Fw_SendBlock(tag, off, len)) { *err = PLC_FWUPD_E_TX; continue; }
      if (!Fw_WaitAck(tag, FWUPD_MSG_BLOCK, 1u, off, PLC_FWUPD_T_BLOCK_MS, &e)) { *err = PLC_FWUPD_E_TIMEOUT; continue; }
      if (e == FWUPD_ERR_OK) break;
      *err = e;
      if (e != FWUPD_ERR_CRC) return 0u; /* flash, range or lost state: restart the whole update */
    }
    if (r > PLC_FWUPD_BLOCK_RETRIES) return 0u;

    off += len;
    g_plc_fwupd.progress = (uint8_t)((off * 100u) / size);
    if (g_plc_fwupd_block_delay_ms != 0u) osDelay(g_plc_fwupd_block_delay_ms);
  }
  return 1u;
}

static uint8_t Fw_Verify(uint32_t tag, uint32_t size, uint32_t crc, uint8_t *err)
{
  uint8_t d[8];

  for (uint32_t t = 0u; t < PLC_FWUPD_VERIFY_TRIES; t++)
  {
    uint8_t e;

    fwupd_put32(&d[0], size);
    fwupd_put32(&d[4], crc);
    if (!Fw_Send(FWUPD_MSG_VERIFY, tag, d)) { *err = PLC_FWUPD_E_TX; continue; }
    if (!Fw_WaitAck(tag, FWUPD_MSG_VERIFY, 0u, 0u, PLC_FWUPD_T_VERIFY_MS, &e)) { *err = PLC_FWUPD_E_TIMEOUT; continue; }
    if (e == FWUPD_ERR_OK) return 1u;
    /* Repeated VERIFY after a lost OK: the node already finished; START checks the image itself. */
    if (e == FWUPD_ERR_STATE && t > 0u) return 1u;
    *err = e;
    return 0u;
  }
  return 0u;
}

static uint8_t Fw_StartAndCheck(uint32_t tag, uint16_t version, uint8_t *err)
{
  uint8_t d[8] = {0};
  uint8_t e;

  g_plc_fwupd.step = PLC_FWUPD_STEP_START;
  if (!Fw_Send(FWUPD_MSG_START, tag, d)) { *err = PLC_FWUPD_E_TX; return 0u; }
  /* A lost START ACK is fine: the roll call below shows whether the application runs. */
  if (Fw_WaitAck(tag, FWUPD_MSG_START, 0u, 0u, PLC_FWUPD_T_START_MS, &e) && e != FWUPD_ERR_OK)
  {
    *err = e;
    return 0u;
  }

  g_plc_fwupd.step = PLC_FWUPD_STEP_CHECK;
  if (!Fw_WaitState(tag, 0u, PLC_FWUPD_T_CHECK_MS, d)) { *err = PLC_FWUPD_E_TIMEOUT; return 0u; }
  if (fwupd_get16(&d[4]) != version) { *err = PLC_FWUPD_E_VERSION; return 0u; }
  return 1u;
}

static uint8_t Fw_UpdateOnce(uint32_t tag, const FwStoreSlot *img, uint8_t *err)
{
  g_plc_fwupd.progress = 0u;
  g_plc_fwupd.step = PLC_FWUPD_STEP_CONNECT;
  if (!Fw_Connect(tag, err)) return 0u;
  g_plc_fwupd.step = PLC_FWUPD_STEP_ERASE;
  if (!Fw_Erase(tag, img->size, err)) return 0u;
  g_plc_fwupd.step = PLC_FWUPD_STEP_BLOCKS;
  if (!Fw_Blocks(tag, img, err)) return 0u;
  g_plc_fwupd.step = PLC_FWUPD_STEP_VERIFY;
  if (!Fw_Verify(tag, img->size, img->crc, err)) return 0u;
  return Fw_StartAndCheck(tag, img->version, err);
}

static uint8_t Fw_Run(int i, const FwStoreSlot *img, uint8_t start_only)
{
  volatile PlcFwupdNode *n = &g_plc_fwupd.nodes[i];
  uint32_t tag = n->tag;
  uint16_t ver_from = n->app_version;
  uint8_t ok = 0u;
  uint8_t err = 0u;

  g_plc_fwupd.busy = 1u;
  g_plc_fwupd.busy_tag = tag;
  n->attempts = 0u;
  if (!start_only) PlcFwupd_Log(PLC_FWUPD_EV_UPD_START, tag, n->module_type, 0u, 0u, ver_from, img->version);

  for (uint32_t a = 0u; a < PLC_FWUPD_ATTEMPTS && !ok; a++)
  {
    n->attempts++;
    ok = start_only ? Fw_StartAndCheck(tag, img->version, &err) : Fw_UpdateOnce(tag, img, &err);
    if (ok) break;
    n->last_err = err;
    n->last_step = g_plc_fwupd.step;
    if (start_only) PlcFwupd_Log(PLC_FWUPD_EV_UPD_START, tag, n->module_type, 0u, 0u, ver_from, img->version);
    start_only = 0u; /* a stopped node that does not start gets a full update */
  }

  n->result = ok ? PLC_FWUPD_RES_OK : PLC_FWUPD_RES_FAILED;
  if (ok)
  {
    g_plc_fwupd.updates_ok++;
    PlcFwupd_Log(start_only ? PLC_FWUPD_EV_STARTED : PLC_FWUPD_EV_UPD_OK, tag, n->module_type, 0u, 0u,
                 ver_from, img->version);
  }
  else
  {
    g_plc_fwupd.updates_failed++;
    PlcFwupd_Log(PLC_FWUPD_EV_UPD_FAIL, tag, n->module_type, n->last_err, n->last_step, ver_from, img->version);
  }
  n->same_ann = 0u; /* next decision only after fresh announces */
  g_plc_fwupd.step = PLC_FWUPD_STEP_IDLE;
  g_plc_fwupd.busy = 0u;
  g_plc_fwupd.busy_tag = 0u;
  return ok;
}

/* ---------- automatic decisions ---------- */

/* Returns the node index to serve with the CURRENT image of its type. */
static int Fw_PickAuto(FwStoreSlot *img, uint8_t *start_only)
{
  for (int i = 0; i < (int)PLC_FWUPD_MAX_NODES; i++)
  {
    volatile PlcFwupdNode *n = &g_plc_fwupd.nodes[i];
    uint8_t same_ver;

    if (n->tag == 0u || !n->link || n->same_ann < PLC_FWUPD_SAME_ANN) continue;
    if (n->result == PLC_FWUPD_RES_FAILED) continue;
    if (!FwStore_Find(n->module_type, FWSTORE_ROLE_CURRENT, img) || img->board_rev != n->board_rev) continue;

    same_ver = (n->app_version == img->version) ? 1u : 0u;
    *start_only = 0u;
    switch (n->state)
    {
      case FWUPD_ST_BOOT_EMPTY:
        return i;
      case FWUPD_ST_BOOT_APP_FAILED:
        if (!same_ver) return i; /* the same version would fail again: stays in the bootloader */
        break;
      case FWUPD_ST_BOOT_WAIT:
        *start_only = same_ver;
        return i;
      case FWUPD_ST_APP_RUNNING:
        if (!n->learned && !same_ver && n->result == PLC_FWUPD_RES_NONE) return i; /* module replaced */
        break;
      default:
        break;
    }
  }
  return -1;
}

/* ---------- update run ---------- */

uint8_t PlcFwupd_RequestRun(uint8_t module_type, uint8_t mode)
{
  FwStoreSlot img;

  if (module_type == 0u || module_type >= FWSTORE_TYPES) return PLC_FWUPD_REQ_ARG;
  if (mode != PLC_FWUPD_MODE_DIFFERENT && mode != PLC_FWUPD_MODE_ALL) return PLC_FWUPD_REQ_ARG;
  if (g_plc_fwupd.run_active || s_run_req_type != 0u || FwStore_UploadActive()) return PLC_FWUPD_REQ_BUSY;
  if (!FwStore_Find(module_type, FWSTORE_ROLE_NEW, &img) && !FwStore_Find(module_type, FWSTORE_ROLE_CURRENT, &img))
  {
    return PLC_FWUPD_REQ_NO_IMAGE;
  }
  s_run_req_rollback = 0u;
  s_run_req_mode = mode;
  s_run_req_type = module_type;
  return PLC_FWUPD_REQ_OK;
}

uint8_t PlcFwupd_RequestRollback(uint8_t module_type)
{
  FwStoreSlot img;

  if (module_type == 0u || module_type >= FWSTORE_TYPES) return PLC_FWUPD_REQ_ARG;
  if (g_plc_fwupd.run_active || s_run_req_type != 0u || FwStore_UploadActive()) return PLC_FWUPD_REQ_BUSY;
  if (!FwStore_Find(module_type, FWSTORE_ROLE_BACKUP, &img)) return PLC_FWUPD_REQ_NO_BACKUP;
  s_run_req_rollback = 1u;
  s_run_req_mode = PLC_FWUPD_MODE_DIFFERENT;
  s_run_req_type = module_type;
  return PLC_FWUPD_REQ_OK;
}

void PlcFwupd_Cancel(void)
{
  if (g_plc_fwupd.run_active) s_run_cancel = 1u;
}

static void Fw_RunBegin(void)
{
  uint8_t type = s_run_req_type;
  uint8_t found = s_run_req_rollback
                ? FwStore_Find(type, FWSTORE_ROLE_BACKUP, &s_run_img)
                : (FwStore_Find(type, FWSTORE_ROLE_NEW, &s_run_img) || FwStore_Find(type, FWSTORE_ROLE_CURRENT, &s_run_img));

  if (!found)
  {
    s_run_req_type = 0u;
    return;
  }
  g_plc_fwupd.run_rollback = s_run_req_rollback;
  memset(s_run_done, 0, sizeof(s_run_done));
  s_run_cancel = 0u;
  g_plc_fwupd.run_type = type;
  g_plc_fwupd.run_mode = s_run_req_mode;
  g_plc_fwupd.run_version = s_run_img.version;
  g_plc_fwupd.run_done = 0u;
  g_plc_fwupd.run_failed = 0u;
  g_plc_fwupd.run_active = 1u;
  s_run_req_type = 0u;
  PlcFwupd_Log(PLC_FWUPD_EV_RUN_START, 0u, type, s_run_req_mode, s_run_img.role, 0u, s_run_img.version);
}

static int Fw_PickRun(uint8_t *start_only)
{
  for (int i = 0; i < (int)PLC_FWUPD_MAX_NODES; i++)
  {
    volatile PlcFwupdNode *n = &g_plc_fwupd.nodes[i];

    if (n->tag == 0u || !n->link || s_run_done[i]) continue;
    if (n->module_type != g_plc_fwupd.run_type || n->board_rev != s_run_img.board_rev) continue;
    if (g_plc_fwupd.run_mode == PLC_FWUPD_MODE_DIFFERENT && n->state == FWUPD_ST_APP_RUNNING &&
        n->app_version == s_run_img.version) continue;
    *start_only = (n->state == FWUPD_ST_BOOT_WAIT && n->app_version == s_run_img.version) ? 1u : 0u;
    return i;
  }
  return -1;
}

static void Fw_RunStep(void)
{
  uint8_t start_only;
  int i;

  if (s_run_cancel)
  {
    PlcFwupd_Log(PLC_FWUPD_EV_RUN_CANCEL, 0u, g_plc_fwupd.run_type, g_plc_fwupd.run_failed, 0u, 0u, s_run_img.version);
    g_plc_fwupd.run_active = 0u;
    return;
  }

  i = Fw_PickRun(&start_only);
  if (i >= 0)
  {
    s_run_done[i] = 1u;
    g_plc_fwupd.nodes[i].result = PLC_FWUPD_RES_NONE;
    if (!Fw_Run(i, &s_run_img, start_only)) g_plc_fwupd.run_failed++;
    g_plc_fwupd.run_done++;
    return;
  }

  if (g_plc_fwupd.run_failed == 0u && s_run_img.role == FWSTORE_ROLE_NEW)
  {
    if (FwStore_Promote(g_plc_fwupd.run_type))
    {
      PlcFwupd_Log(PLC_FWUPD_EV_PROMOTE, 0u, g_plc_fwupd.run_type, 0u, 0u, 0u, s_run_img.version);
    }
  }
  else if (g_plc_fwupd.run_failed == 0u && s_run_img.role == FWSTORE_ROLE_BACKUP)
  {
    if (FwStore_Rollback(g_plc_fwupd.run_type))
    {
      PlcFwupd_Log(PLC_FWUPD_EV_ROLLBACK, 0u, g_plc_fwupd.run_type, 0u, 0u, 0u, s_run_img.version);
    }
  }
  PlcFwupd_Log(PLC_FWUPD_EV_RUN_END, 0u, g_plc_fwupd.run_type, g_plc_fwupd.run_failed, g_plc_fwupd.run_done,
               0u, s_run_img.version);
  g_plc_fwupd.run_active = 0u;
}

/* ---------- task ---------- */

static void PlcFwupd_Task(void *arg)
{
  uint32_t t_disc;
  uint8_t zero[8] = {0};

  (void)arg;
  g_plc_fwupd.store_ok = FwStore_Init();

  while (HAL_FDCAN_GetState(PLC_FWUPD_CAN) != HAL_FDCAN_STATE_BUSY) osDelay(100);
  t_disc = osKernelGetTickCount() - PLC_FWUPD_DISCOVER_MS;

  for (;;)
  {
    FwFrame f;
    FwStoreSlot img;
    uint32_t now;
    uint8_t start_only;
    int i;

    if (osMessageQueueGet(s_q, &f, NULL, 100u) == osOK) Fw_Record(&f);

    now = osKernelGetTickCount();
    if ((now - t_disc) >= PLC_FWUPD_DISCOVER_MS)
    {
      t_disc = now;
      (void)Fw_Send(FWUPD_MSG_DISCOVER, FWUPD_NODE_ALL, zero);
    }
    Fw_UpdateLinks(now);

    if (!g_plc_fwupd.run_active && s_run_req_type != 0u) Fw_RunBegin();
    if (g_plc_fwupd.run_active)
    {
      Fw_RunStep();
      continue;
    }
    if (FwStore_UploadActive()) continue;

    i = Fw_PickAuto(&img, &start_only);
    if (i >= 0) (void)Fw_Run(i, &img, start_only);
  }
}

void PlcFwupd_Start(void)
{
  memset((void *)&g_plc_fwupd, 0, sizeof(g_plc_fwupd));
  FwStore_Setup();
  s_q = osMessageQueueNew(PLC_FWUPD_QUEUE_LEN, sizeof(FwFrame), NULL);
  if (s_q != NULL) osThreadNew(PlcFwupd_Task, NULL, &s_task_attr);
}
