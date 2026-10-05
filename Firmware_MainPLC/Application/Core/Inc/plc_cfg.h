/*
 * plc_cfg.h — system configuration from the HVAC application (Shared_Libs/hvac_cfg.h).
 *
 * The file is uploaded over HTTP, checked, stored in W25Q128 (two 16 KB copies in the reserved
 * first megabyte, the newer valid one is used after power-up) and becomes active at once.
 *
 * The PLC then compares the configuration with what answers on the buses and sends the modules
 * their settings:
 *   - locomotives: roll call of the update protocol (plc_fwupd);
 *   - AI: CFG_STATUS frames; channel settings are sent again whenever the module reports another
 *     generation or a channel without confirmation (after power-up the module runs on defaults);
 *   - display hub: indirectly, CAN2 frames are acknowledged (no error-passive / bus-off);
 *   - other module types: not checked yet.
 * Until positions on the rail are assigned by the PLC, one AI module per system is supported.
 */
#ifndef PLC_CFG_H
#define PLC_CFG_H

#include <stdint.h>
#include "hvac_cfg.h"

#define PLC_FW_VERSION          0x0101u /* major << 8 | minor */

/* Upload result */
#define PLC_CFG_OK              0u
#define PLC_CFG_E_FLASH         1u /* W25Q128 missing or erase/program failed */
#define PLC_CFG_E_SIZE          2u
#define PLC_CFG_E_FORMAT        3u /* magic, format or table contents */
#define PLC_CFG_E_CRC           4u
#define PLC_CFG_E_BUSY          5u

/* State of one configured module */
#define PLC_CFG_MOD_UNCHECKED   0u /* this module type cannot be checked yet */
#define PLC_CFG_MOD_OK          1u
#define PLC_CFG_MOD_MISSING     2u
#define PLC_CFG_MOD_ERROR       3u /* answers, but rejected settings or did not confirm them */
#define PLC_CFG_MOD_APPLYING    4u
#define PLC_CFG_MOD_ON_BUS      5u /* indirect: the bus acknowledges frames */

/* PlcCfgModState.err */
#define PLC_CFG_ERR_NONE        0u
#define PLC_CFG_ERR_NO_CONFIRM  0x80u /* settings sent several times, module reports another generation */
#define PLC_CFG_ERR_IN_BOOT     0x81u /* module stays in its bootloader */
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
  uint8_t  extra_loco;     /* answering locomotives that are not in the configuration */
  uint8_t  extra_ai;
} PlcCfgSummary;

/* Creates the mutex. Call before the scheduler starts. */
void PlcCfg_Setup(void);

/* From the CAN1 RX interrupt: CFG_STATUS frame. */
void PlcCfg_OnStatusIsr(const uint8_t d[8]);

/* Periodic work from the CAN task: loads the stored file once, checks modules, sends settings. */
void PlcCfg_Poll(void);

/* Upload from the web task: whole file in RAM, then checked and stored. */
uint8_t PlcCfg_UploadBegin(uint32_t size, uint32_t crc);
void PlcCfg_UploadWrite(const uint8_t *p, uint32_t n);
uint8_t PlcCfg_UploadEnd(uint8_t *gen_out);

void PlcCfg_GetSummary(PlcCfgSummary *out);
/* Module i of the active configuration; returns 0 if there is no such module. */
uint8_t PlcCfg_GetModule(uint16_t i, HvacCfgModule *m, PlcCfgModState *st);

#endif /* PLC_CFG_H */
