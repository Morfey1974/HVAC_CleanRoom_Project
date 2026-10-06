/*
 * hvac_id.h — module identification: the Main PLC walks the MODUL_ID lines and gives every module
 * its place ID "line.rail.place" (Docs/Web_Interface_Plan.md, 8.3.3 / 8.3.4).
 *
 * Every module has an ID input (optocoupler) and one or more ID outputs:
 *   Main PLC     line 1 output (to the first locomotive), line 2 output (to the first HUB of displays);
 *   locomotive   CHAIN output (next locomotive on the cable), RAIL output (first module of its rail);
 *   rail module  CHAIN output (next module of the rail);
 *   HUB          CHAIN output (next HUB on the cable), one PORT output per display port (1..9);
 *   display      input only.
 * A module answers QUERY only while its ID input is active and it has no ID yet, so exactly one
 * module answers at a time. IDs live in RAM: after a restart the module asks for a new walk.
 *
 * Walk (Main PLC): RESET -> raise the line output -> QUERY -> ASSIGN -> PASS (module raises the
 * next output) -> QUERY ... ; no answer within HVAC_ID_ANSWER_MS = end of that chain -> DONE.
 *
 * CAN (classic, 11-bit IDs), forwarded unchanged by the locomotive (CAN2 <-> CAN1) and the HUB.
 */
#ifndef HVAC_ID_H
#define HVAC_ID_H

#include <stdint.h>

/* PLC -> modules, DLC 8, [0] = HVAC_ID_CMD_*:
 *   RESET   forget the ID, drop all ID outputs
 *   QUERY   a module with active input and no ID answers ID_HERE
 *   ASSIGN  [1..3] node tag (fwupd_node_tag, little-endian), [4] line, [5] rail, [6] place
 *   PASS    [1] line, [2] rail, [3] place, [4] output HVAC_ID_OUT_*: the module with this ID raises
 *           this output and drops its other outputs
 *   DONE    drop all ID outputs, keep the ID */
#define HVAC_CAN_ID_ID_CMD        0x350u

/* Module -> PLC, DLC 8: answer to QUERY and ASSIGN, and every HVAC_ID_UNASSIGNED_MS while the module
 * has no ID (asks the PLC for a walk).
 *   [0..2] node tag, little-endian
 *   [3]    catalogue type, HVAC_CAT_* (hvac_cfg.h)
 *   [4]    line, [5] rail, [6] place; HVAC_ID_NONE in [6] = no ID
 *   [7]    board revision */
#define HVAC_CAN_ID_ID_HERE       0x351u

/* Chain check summary, PLC -> HUB -> displays, DLC 8, every HVAC_ID_CHAIN_MS:
 *   [0]    HVAC_CHAIN_ST_*
 *   [1]    number of problems
 *   [2]    first problem HVAC_CHAIN_P_*, [3] line, [4] rail, [5] place,
 *   [6]    expected catalogue type, [7] found catalogue type (0 = none) */
#define HVAC_CAN_ID_CHAIN         0x352u
#define HVAC_ID_CHAIN_MS          1000u

#define HVAC_ID_CMD_RESET         1u
#define HVAC_ID_CMD_QUERY         2u
#define HVAC_ID_CMD_ASSIGN        3u
#define HVAC_ID_CMD_PASS          4u
#define HVAC_ID_CMD_DONE          5u

#define HVAC_ID_OUT_NONE          0x00u
#define HVAC_ID_OUT_CHAIN         0x01u
#define HVAC_ID_OUT_RAIL          0x02u
#define HVAC_ID_OUT_PORT(n)       (0x10u + (n)) /* HUB display port 1..9 */
#define HVAC_ID_PORTS             9u

#define HVAC_ID_NONE              0xFFu
#define HVAC_ID_ANSWER_MS         200u
#define HVAC_ID_SETTLE_MS         20u   /* optocoupler delay after an output changes */
#define HVAC_ID_UNASSIGNED_MS     5000u

#define HVAC_CHAIN_ST_NONE        0u /* no configuration or not walked yet */
#define HVAC_CHAIN_ST_OK          1u
#define HVAC_CHAIN_ST_BUSY        2u /* walk or configuration in progress */
#define HVAC_CHAIN_ST_ERROR       3u

#define HVAC_CHAIN_P_NONE         0u
#define HVAC_CHAIN_P_MISSING      1u /* configured module not found at its place */
#define HVAC_CHAIN_P_WRONG_TYPE   2u /* another module type at this place */
#define HVAC_CHAIN_P_EXTRA        3u /* module not in the configuration */
#define HVAC_CHAIN_P_CONFIG       4u /* module rejected or did not confirm its settings */

static inline void hvac_id_put_tag(uint8_t *d, uint32_t tag)
{
  d[0] = (uint8_t)tag; d[1] = (uint8_t)(tag >> 8); d[2] = (uint8_t)(tag >> 16);
}

static inline uint32_t hvac_id_get_tag(const uint8_t *d)
{
  return (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16);
}

/* ---------- module side (hvac_id_node.c) ---------- */

typedef struct
{
  uint8_t  cat;                               /* HVAC_CAT_* */
  uint8_t  board_rev;
  uint8_t  (*input_active)(void);             /* ID input raised by the previous module */
  void     (*set_output)(uint8_t out);        /* HVAC_ID_OUT_*: raise only this one, NONE drops all */
  void     (*send)(uint32_t std_id, const uint8_t d[8]); /* main-loop context */
} HvacIdNodeCfg;

void HvacId_Init(const HvacIdNodeCfg *cfg, uint32_t tag);
/* HVAC_CAN_ID_ID_CMD frame, interrupt context is fine. */
void HvacId_OnCmd(const uint8_t d[8]);
/* Main loop: answers, outputs, periodic "no ID" announce. */
void HvacId_Poll(void);
/* 1 and the place ID if assigned. */
uint8_t HvacId_Get(uint8_t *line, uint8_t *rail, uint8_t *place);

#endif /* HVAC_ID_H */
