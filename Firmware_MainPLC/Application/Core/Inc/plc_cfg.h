/*
 * plc_cfg.h — system configuration from the HVAC application (Shared_Libs/hvac_cfg.h).
 *
 * The file is uploaded over HTTP, checked, stored in W25Q128 (two 16 KB copies in the reserved
 * first megabyte, the newer valid one is used after power-up) and becomes active at once.
 *
 * The PLC then compares the configuration with the modules found by the identification walk
 * (plc_id) at their places "line.rail.place" and sends the modules their settings:
 *   - every module with an ID: present at its place, right type, extra modules;
 *   - locomotives, HUBs, displays: application version from the update roll call (plc_fwupd);
 *   - AI: CFG_STATUS frames per place; channel settings are sent again whenever the module reports
 *     another generation or a channel without confirmation (after power-up it runs on defaults);
 *   - modules without ID support (Doors PLC, HMI): not checked.
 * A summary of the first problem goes to the displays every HVAC_ID_CHAIN_MS (HVAC_CAN_ID_CHAIN).
 */
#ifndef PLC_CFG_H
#define PLC_CFG_H

#include <stdint.h>
#include "hvac_cfg.h"
#include "plc_id.h"

#define PLC_FW_VERSION          0x0104u /* major << 8 | minor */

#define PLC_CFG_MAX_EXTRA       16u

/* Upload result */
#define PLC_CFG_OK              0u
#define PLC_CFG_E_FLASH         1u /* W25Q128 missing or erase/program failed */
#define PLC_CFG_E_SIZE          2u
#define PLC_CFG_E_FORMAT        3u /* magic, format or table contents */
#define PLC_CFG_E_CRC           4u
#define PLC_CFG_E_BUSY          5u

/* State of one configured module */
#define PLC_CFG_MOD_UNCHECKED   0u /* this module type cannot be checked */
#define PLC_CFG_MOD_OK          1u
#define PLC_CFG_MOD_MISSING     2u /* nothing answers at this place */
#define PLC_CFG_MOD_ERROR       3u /* answers, but rejected settings or did not confirm them */
#define PLC_CFG_MOD_APPLYING    4u /* walk in progress or settings not confirmed yet */
#define PLC_CFG_MOD_ON_BUS      5u /* answers, place or settings not checked (e.g. display board without ID) */
#define PLC_CFG_MOD_WRONG_TYPE  6u /* another module type at this place */

/* PlcCfgModState.err */
#define PLC_CFG_ERR_NONE        0u
#define PLC_CFG_ERR_NO_CONFIRM  0x80u /* settings sent several times, module reports another generation */
#define PLC_CFG_ERR_IN_BOOT     0x81u /* a module of this type stays in its bootloader */
/* 1..3: HVAC_CFG_E_* reported by the module */

/* Overall state */
#define PLC_CFG_ST_NONE         0u /* no configuration */
#define PLC_CFG_ST_OK           1u
#define PLC_CFG_ST_APPLYING     2u
#define PLC_CFG_ST_ERRORS       3u

typedef struct
{
  uint8_t  state;     /* PLC_CFG_MOD_* */
  uint8_t  err;       /* PLC_CFG_ERR_* or HVAC_CFG_E_* */
  uint8_t  ok_mask;   /* channels confirmed */
  uint8_t  bad_mask;  /* channels rejected */
  uint16_t version;   /* application version of the module that answers, 0 = unknown */
  uint8_t  found_cat; /* type of the module found at this place, 0 = none */
  uint32_t tag;       /* node tag of the module found at this place, 0 = none */
} PlcCfgModState;

typedef struct
{
  uint8_t  present;
  uint8_t  gen;
  uint8_t  state;     /* PLC_CFG_ST_* */
  uint8_t  flash_ok;
  uint32_t size;
  uint32_t crc;
  char     project[HVAC_CFGF_PROJECT_LEN + 1u];
  uint16_t module_count;
  uint16_t channel_count;
  uint32_t applied_ms;     /* uptime when the state last became OK, 0 = never */
  uint32_t resends;        /* channel settings sent again (module restarted or lost them) */
  uint8_t  extra_loco;     /* locomotives found that are not in the configuration */
  uint8_t  extra_ai;
  uint8_t  extra_count;    /* entries for PlcCfg_GetExtra */
  uint8_t  problems;       /* missing + wrong type + settings errors + extra */
} PlcCfgSummary;

/* Creates the mutex. Call before the scheduler starts. */
void PlcCfg_Setup(void);

/* From the CAN1 RX interrupt: CFG_STATUS frame. */
void PlcCfg_OnStatusIsr(const uint8_t d[8]);

/* Periodic work from the CAN task: loads the stored file once, checks modules, sends settings. */
void PlcCfg_Poll(void);

/* 1 when the stored configuration was loaded (or there is none). */
uint8_t PlcCfg_Ready(void);
/* For the identification walk: k-th configured head rail of the line (ascending), 0 = none. */
uint8_t PlcCfg_HeadRail(uint8_t line, uint8_t k);
/* Highest rail number in the configuration, 0 = none. */
uint8_t PlcCfg_MaxRail(void);

/* Upload from the web task: whole file in RAM, then checked and stored. */
uint8_t PlcCfg_UploadBegin(uint32_t size, uint32_t crc);
void PlcCfg_UploadWrite(const uint8_t *p, uint32_t n);
uint8_t PlcCfg_UploadEnd(uint8_t *gen_out);

void PlcCfg_GetSummary(PlcCfgSummary *out);
/* Module i of the active configuration; returns 0 if there is no such module. */
uint8_t PlcCfg_GetModule(uint16_t i, HvacCfgModule *m, PlcCfgModState *st);
/* Extra module i: found but not configured (rail, place set) or outside the chain (HVAC_ID_NONE). */
uint8_t PlcCfg_GetExtra(uint8_t i, PlcIdFound *out);

#endif /* PLC_CFG_H */
