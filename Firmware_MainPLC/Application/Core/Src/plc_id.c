/*
 * plc_id.c — identification walk over the MODUL_ID lines (see plc_id.h).
 */
#include "plc_id.h"

#include <string.h>
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "hvac_cfg.h"
#include "plc_can.h"
#include "plc_cfg.h"
#include "plc_fwupd.h"

#define ID_QUEUE_LEN        16u
#define ID_FIRST_WALK_MS    3000u  /* modules finish their start-up */
#define ID_AUTO_GAP_MS      5000u  /* minimum pause between walks requested by modules */
#define ID_OUT_SETTLE_MS    50u    /* output change -> optocoupler -> module main loop */
#define ID_CMD_TRIES        3u
#define ID_ASSIGN_TRIES     2u
#define ID_MAX_HEADS        30u
#define ID_MAX_PLACES       31u
#define ID_LINES            2u

typedef struct
{
  uint8_t bus;
  uint8_t d[8];
} IdFrame;

static osMessageQueueId_t s_q;
static volatile uint8_t s_req;
static volatile uint8_t s_busy;
static uint16_t s_walks;
static uint32_t s_done_ms;
static uint32_t s_last_walk;

static PlcIdFound s_found[PLC_ID_MAX_FOUND];
static uint8_t s_found_n;
static PlcIdFound s_stray[PLC_ID_MAX_STRAY];
static uint8_t s_stray_n;

static const osThreadAttr_t s_task_attr = {
  .name = "idTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

/* ---------- outputs and frames ---------- */

static void Id_LineOut(uint8_t line, uint8_t on)
{
  GPIO_PinState s = on ? GPIO_PIN_SET : GPIO_PIN_RESET;

  if (line == 1u) HAL_GPIO_WritePin(MCU_MODUL_ID_OUT_AI_GPIO_Port, MCU_MODUL_ID_OUT_AI_Pin, s);
  else            HAL_GPIO_WritePin(MCU_MODUL_ID_OUT_DISPLAY_GPIO_Port, MCU_MODUL_ID_OUT_DISPLAY_Pin, s);
}

static void Id_Cmd(uint8_t bus, const uint8_t d[8])
{
  for (uint32_t t = 0u; t < ID_CMD_TRIES; t++)
  {
    if (PlcCan_Send(bus, HVAC_CAN_ID_ID_CMD, 0u, d)) return;
    osDelay(2);
  }
}

static void Id_CmdAll(uint8_t cmd)
{
  uint8_t d[8] = {0};

  d[0] = cmd;
  Id_Cmd(PLC_CAN_BUS_LOCO, d);
  Id_Cmd(PLC_CAN_BUS_HUB, d);
}

static void Id_Flush(void)
{
  IdFrame f;

  while (osMessageQueueGet(s_q, &f, NULL, 0u) == osOK) { }
}

/* Waits for ID_HERE on the bus: tag == 0 -> any module without ID, otherwise this tag with this place. */
static uint8_t Id_Wait(uint8_t bus, uint32_t tag, uint8_t place, uint8_t out[8])
{
  uint32_t t0 = osKernelGetTickCount();

  for (;;)
  {
    uint32_t el = osKernelGetTickCount() - t0;
    IdFrame f;

    if (el >= HVAC_ID_ANSWER_MS) return 0u;
    if (osMessageQueueGet(s_q, &f, NULL, HVAC_ID_ANSWER_MS - el) != osOK) return 0u;
    if (f.bus != bus) continue;
    if (tag == 0u ? (f.d[6] == HVAC_ID_NONE) : (hvac_id_get_tag(f.d) == tag && f.d[6] == place))
    {
      memcpy(out, f.d, 8u);
      return 1u;
    }
  }
}

static uint8_t Id_Query(uint8_t bus, uint8_t here[8])
{
  uint8_t d[8] = {0};

  Id_Flush();
  d[0] = HVAC_ID_CMD_QUERY;
  Id_Cmd(bus, d);
  return Id_Wait(bus, 0u, 0u, here);
}

static uint8_t Id_Assign(uint8_t bus, uint32_t tag, uint8_t line, uint8_t rail, uint8_t place)
{
  uint8_t d[8] = {0};
  uint8_t here[8];

  d[0] = HVAC_ID_CMD_ASSIGN;
  hvac_id_put_tag(&d[1], tag);
  d[4] = line;
  d[5] = rail;
  d[6] = place;
  for (uint32_t t = 0u; t < ID_ASSIGN_TRIES; t++)
  {
    Id_Cmd(bus, d);
    if (Id_Wait(bus, tag, place, here)) return 1u;
  }
  return 0u;
}

static void Id_Pass(uint8_t bus, uint8_t line, uint8_t rail, uint8_t place, uint8_t out)
{
  uint8_t d[8] = {0};

  d[0] = HVAC_ID_CMD_PASS;
  d[1] = line;
  d[2] = rail;
  d[3] = place;
  d[4] = out;
  Id_Cmd(bus, d);
  osDelay(ID_OUT_SETTLE_MS);
}

static void Id_Add(const uint8_t here[8], uint8_t line, uint8_t rail, uint8_t place)
{
  PlcIdFound e;

  if (s_found_n >= PLC_ID_MAX_FOUND) return;
  e.tag = hvac_id_get_tag(here);
  e.cat = here[3];
  e.board = here[7];
  e.line = line;
  e.rail = rail;
  e.place = place;
  taskENTER_CRITICAL();
  s_found[s_found_n++] = e;
  taskEXIT_CRITICAL();
}

/* ---------- walk ---------- */

static void Id_WalkRail(uint8_t bus, uint8_t line, uint8_t rail)
{
  uint8_t here[8];

  Id_Pass(bus, line, rail, 0u, HVAC_ID_OUT_RAIL);
  for (uint8_t place = 1u; place <= ID_MAX_PLACES; place++)
  {
    if (!Id_Query(bus, here)) break;
    if (!Id_Assign(bus, hvac_id_get_tag(here), line, rail, place)) break;
    Id_Add(here, line, rail, place);
    Id_Pass(bus, line, rail, place, HVAC_ID_OUT_CHAIN);
  }
}

static void Id_WalkPorts(uint8_t bus, uint8_t line, uint8_t rail)
{
  uint8_t here[8];

  for (uint8_t port = 1u; port <= HVAC_ID_PORTS; port++)
  {
    Id_Pass(bus, line, rail, 0u, HVAC_ID_OUT_PORT(port));
    if (!Id_Query(bus, here)) continue;
    if (Id_Assign(bus, hvac_id_get_tag(here), line, rail, port)) Id_Add(here, line, rail, port);
  }
}

static void Id_Walk(void)
{
  uint8_t next_free = (uint8_t)(PlcCfg_MaxRail() + 1u);

  s_busy = 1u;
  taskENTER_CRITICAL();
  s_found_n = 0u;
  taskEXIT_CRITICAL();

  Id_LineOut(1u, 0u);
  Id_LineOut(2u, 0u);
  Id_CmdAll(HVAC_ID_CMD_RESET);
  osDelay(ID_OUT_SETTLE_MS);

  for (uint8_t line = 1u; line <= ID_LINES; line++)
  {
    uint8_t bus = (line == 1u) ? PLC_CAN_BUS_LOCO : PLC_CAN_BUS_HUB;
    uint8_t here[8];

    Id_LineOut(line, 1u);
    osDelay(ID_OUT_SETTLE_MS);
    for (uint8_t k = 0u; k < ID_MAX_HEADS; k++)
    {
      uint8_t rail;
      uint8_t cat;

      if (!Id_Query(bus, here)) break;
      cat = here[3];
      rail = PlcCfg_HeadRail(line, k);
      if (rail == 0u) rail = next_free++;
      if (!Id_Assign(bus, hvac_id_get_tag(here), line, rail, 0u)) break;
      Id_Add(here, line, rail, 0u);
      if (k == 0u) Id_LineOut(line, 0u);

      if (cat == HVAC_CAT_LOCOMOTIVE) Id_WalkRail(bus, line, rail);
      else if (cat == HVAC_CAT_HUB_DISPLAYS) Id_WalkPorts(bus, line, rail);
      Id_Pass(bus, line, rail, 0u, HVAC_ID_OUT_CHAIN);
    }
    Id_LineOut(line, 0u);
  }

  Id_CmdAll(HVAC_ID_CMD_DONE);
  Id_Flush();
  s_walks++;
  s_done_ms = osKernelGetTickCount();
  s_last_walk = s_done_ms;
  s_busy = 0u;
  PlcFwupd_Log(PLC_FWUPD_EV_ID_WALK, 0u, 0u, 0u, 0u, s_found_n, s_walks);
}

/* ---------- modules without ID outside a walk ---------- */

static uint8_t Id_IsFound(uint32_t tag)
{
  for (uint8_t i = 0u; i < s_found_n; i++) if (s_found[i].tag == tag) return 1u;
  return 0u;
}

static void Id_OnUnassigned(const IdFrame *f)
{
  uint32_t tag = hvac_id_get_tag(f->d);

  /* A module that had an ID restarted: walk again. */
  if (Id_IsFound(tag)) { s_req = 1u; return; }

  for (uint8_t i = 0u; i < s_stray_n; i++) if (s_stray[i].tag == tag) return;

  /* New module: one walk; if it still has no ID afterwards it is outside the chain. */
  if (s_stray_n < PLC_ID_MAX_STRAY)
  {
    PlcIdFound e;

    e.tag = tag;
    e.cat = f->d[3];
    e.board = f->d[7];
    e.line = (f->bus == PLC_CAN_BUS_HUB) ? 2u : 1u;
    e.rail = HVAC_ID_NONE;
    e.place = HVAC_ID_NONE;
    taskENTER_CRITICAL();
    s_stray[s_stray_n++] = e;
    taskEXIT_CRITICAL();
  }
  s_req = 1u;
}

static void PlcId_Task(void *arg)
{
  uint8_t manual = 0u;

  (void)arg;
  while (osKernelGetTickCount() < ID_FIRST_WALK_MS || !PlcCfg_Ready()) osDelay(100);
  s_req = 1u;
  manual = 1u;

  for (;;)
  {
    IdFrame f;
    uint32_t now;

    if (osMessageQueueGet(s_q, &f, NULL, 100u) == osOK && f.d[6] == HVAC_ID_NONE) Id_OnUnassigned(&f);

    now = osKernelGetTickCount();
    if (s_req == 2u) manual = 1u;
    if (s_req != 0u && (manual || (now - s_last_walk) >= ID_AUTO_GAP_MS))
    {
      if (s_req == 2u)
      {
        taskENTER_CRITICAL();
        s_stray_n = 0u;
        taskEXIT_CRITICAL();
      }
      s_req = 0u;
      manual = 0u;
      Id_Walk();
    }
  }
}

/* ---------- API ---------- */

void PlcId_Start(void)
{
  s_q = osMessageQueueNew(ID_QUEUE_LEN, sizeof(IdFrame), NULL);
  if (s_q != NULL) osThreadNew(PlcId_Task, NULL, &s_task_attr);
}

void PlcId_OnHereIsr(uint8_t bus, const uint8_t d[8])
{
  IdFrame f;

  if (s_q == NULL) return;
  f.bus = bus;
  memcpy(f.d, d, 8u);
  (void)osMessageQueuePut(s_q, &f, 0u, 0u);
}

void PlcId_RequestWalk(void)
{
  s_req = 2u;
}

void PlcId_GetSummary(PlcIdSummary *out)
{
  taskENTER_CRITICAL();
  out->busy = s_busy;
  out->count = s_found_n;
  out->walks = s_walks;
  out->done_ms = s_done_ms;
  out->stray = 0u;
  for (uint8_t i = 0u; i < s_stray_n; i++) if (!Id_IsFound(s_stray[i].tag)) out->stray++;
  taskEXIT_CRITICAL();
}

uint8_t PlcId_GetFound(PlcIdFound *out, uint8_t max)
{
  uint8_t n;

  taskENTER_CRITICAL();
  n = (s_found_n < max) ? s_found_n : max;
  memcpy(out, s_found, (uint32_t)n * sizeof(PlcIdFound));
  taskEXIT_CRITICAL();
  return n;
}

uint8_t PlcId_GetStray(PlcIdFound *out, uint8_t max)
{
  uint8_t n = 0u;

  taskENTER_CRITICAL();
  for (uint8_t i = 0u; i < s_stray_n && n < max; i++)
  {
    if (!Id_IsFound(s_stray[i].tag)) out[n++] = s_stray[i];
  }
  taskEXIT_CRITICAL();
  return n;
}
