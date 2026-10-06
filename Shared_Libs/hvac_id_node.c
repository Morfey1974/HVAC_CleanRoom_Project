/*
 * hvac_id_node.c — module side of the identification walk (hvac_id.h).
 * Needs HAL_GetTick (main.h of the module project).
 */
#include "hvac_id.h"
#include "main.h"

static const HvacIdNodeCfg *s_cfg;
static uint32_t s_tag;
static volatile uint8_t s_line = HVAC_ID_NONE;
static volatile uint8_t s_rail = HVAC_ID_NONE;
static volatile uint8_t s_place = HVAC_ID_NONE;
static volatile uint8_t s_answer;
static volatile uint8_t s_out_req;
static volatile uint8_t s_out_pending;
static uint32_t s_last_unassigned;

static void HvacId_SendHere(void)
{
  uint8_t d[8];

  hvac_id_put_tag(d, s_tag);
  d[3] = s_cfg->cat;
  d[4] = s_line;
  d[5] = s_rail;
  d[6] = s_place;
  d[7] = s_cfg->board_rev;
  s_cfg->send(HVAC_CAN_ID_ID_HERE, d);
}

void HvacId_Init(const HvacIdNodeCfg *cfg, uint32_t tag)
{
  s_cfg = cfg;
  s_tag = tag;
  s_last_unassigned = HAL_GetTick();
  s_out_req = HVAC_ID_OUT_NONE;
  s_out_pending = 1u;
}

void HvacId_OnCmd(const uint8_t d[8])
{
  if (s_cfg == 0) return;

  /* Quiet while a walk runs: an unsolicited "no ID" would look like a QUERY answer. */
  s_last_unassigned = HAL_GetTick();

  switch (d[0])
  {
    case HVAC_ID_CMD_RESET:
      s_place = HVAC_ID_NONE;
      s_rail = HVAC_ID_NONE;
      s_line = HVAC_ID_NONE;
      s_out_req = HVAC_ID_OUT_NONE;
      s_out_pending = 1u;
      break;
    case HVAC_ID_CMD_QUERY:
      if (s_place == HVAC_ID_NONE && s_cfg->input_active()) s_answer = 1u;
      break;
    case HVAC_ID_CMD_ASSIGN:
      if (hvac_id_get_tag(&d[1]) == s_tag && d[6] != HVAC_ID_NONE)
      {
        s_line = d[4];
        s_rail = d[5];
        s_place = d[6];
        s_answer = 1u;
      }
      break;
    case HVAC_ID_CMD_PASS:
      if (s_place != HVAC_ID_NONE && d[1] == s_line && d[2] == s_rail && d[3] == s_place)
      {
        s_out_req = d[4];
        s_out_pending = 1u;
      }
      break;
    case HVAC_ID_CMD_DONE:
      s_out_req = HVAC_ID_OUT_NONE;
      s_out_pending = 1u;
      break;
    default:
      break;
  }
}

void HvacId_Poll(void)
{
  uint32_t now;

  if (s_cfg == 0) return;
  now = HAL_GetTick();

  if (s_out_pending)
  {
    s_out_pending = 0u;
    s_cfg->set_output(s_out_req);
  }
  if (s_answer)
  {
    s_answer = 0u;
    HvacId_SendHere();
  }
  if (s_place != HVAC_ID_NONE)
  {
    s_last_unassigned = now;
  }
  else if ((now - s_last_unassigned) >= HVAC_ID_UNASSIGNED_MS)
  {
    s_last_unassigned = now;
    HvacId_SendHere();
  }
}

uint8_t HvacId_Get(uint8_t *line, uint8_t *rail, uint8_t *place)
{
  uint8_t p = s_place;

  if (p == HVAC_ID_NONE) return 0u;
  *line = s_line;
  *rail = s_rail;
  *place = p;
  return 1u;
}
