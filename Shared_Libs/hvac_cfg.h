/*
 * hvac_cfg.h — module configuration: CAN frames (Main PLC <-> modules) and the configuration
 * file the HVAC application uploads to the Main PLC over Ethernet.
 *
 * The Main PLC is the owner of the configuration: it keeps the file in W25Q128 and sends every
 * module its settings again whenever the module reports another generation (e.g. after power-up,
 * modules keep nothing in flash).
 *
 * CAN (classic, 11-bit IDs, 50 kbit/s), path through the locomotive in both directions:
 *   Main PLC CAN1 -> Locomotive CAN2 -> Locomotive CAN1 -> AI CAN2   (CFG_SET)
 *   AI CAN2 -> Locomotive CAN1 -> Locomotive CAN2 -> Main PLC CAN1   (CFG_STATUS)
 */
#ifndef HVAC_CFG_H
#define HVAC_CFG_H

#include <stdint.h>

/* Channel setting, PLC -> module, DLC 8.
 *   [0]    target place on the rail (1..31), HVAC_CFG_PLACE_ANY = every module of the bus
 *   [1]    channel, 1..N
 *   [2]    signal, HVAC_SIG_*
 *   [3]    configuration generation (1..255)
 *   [4..5] value at the bottom of the signal, int16 little-endian, 0.1 units
 *   [6..7] value at the top of the signal,    int16 little-endian, 0.1 units
 * min == max == 0: no sensor range, the module reports the signal itself (mA / V).
 */
#define HVAC_CAN_ID_CFG_SET       0x330u

/* Module configuration status, module -> PLC, DLC 8, every HVAC_CFG_STATUS_MS and after each CFG_SET.
 *   [0]    module type, FWUPD_TYPE_* (fwupd_proto.h)
 *   [1]    place on the rail, HVAC_CFG_PLACE_ANY if unknown
 *   [2..3] application version, major << 8 | minor
 *   [4]    configuration generation of the last CFG_SET, 0 = defaults since power-up
 *   [5]    channels accepted, bit N-1 = channel N
 *   [6]    channels rejected, bit N-1 = channel N
 *   [7]    last error, HVAC_CFG_E_*
 */
#define HVAC_CAN_ID_CFG_STATUS    0x331u

#define HVAC_CFG_STATUS_MS        1000u
#define HVAC_CFG_PLACE_ANY        0xFFu

#define HVAC_SIG_OFF              0u /* channel not used: no value, fault bit set */
#define HVAC_SIG_4_20MA           1u
#define HVAC_SIG_0_20MA           2u
#define HVAC_SIG_0_10V            3u
#define HVAC_SIG_2_10V            4u
#define HVAC_SIG_0_5V             5u
#define HVAC_SIG_PT100            6u
#define HVAC_SIG_PT1000           7u
#define HVAC_SIG_NTC10K           8u

#define HVAC_CFG_E_OK             0u
#define HVAC_CFG_E_CHANNEL        1u /* no such channel */
#define HVAC_CFG_E_SIGNAL         2u /* the module hardware does not support this signal */
#define HVAC_CFG_E_RANGE          3u /* min >= max, or outside what the measurement frame can carry */

/* ---------- configuration file (HTTP POST /api/cfg on the Main PLC) ----------
 *
 * Little-endian: header, then module_count modules, then channel_count channels.
 * Module types are the catalogue codes of the HVAC application (library "type code").
 */
#define HVAC_CFGF_MAGIC           0x47464348u /* "HCFG" */
#define HVAC_CFGF_FORMAT          1u
#define HVAC_CFGF_MAX_SIZE        8192u
#define HVAC_CFGF_MAX_MODULES     64u
#define HVAC_CFGF_MAX_CHANNELS    256u
#define HVAC_CFGF_PROJECT_LEN     24u

#define HVAC_CAT_PLC              0x01u
#define HVAC_CAT_DOORS_PLC        0x02u
#define HVAC_CAT_LOCOMOTIVE       0x10u
#define HVAC_CAT_AI               0x20u
#define HVAC_CAT_AO               0x30u
#define HVAC_CAT_DI               0x40u
#define HVAC_CAT_DO               0x50u
#define HVAC_CAT_RELAY            0x60u
#define HVAC_CAT_HUB_DISPLAYS     0x70u
#define HVAC_CAT_TFT43            0x71u
#define HVAC_CAT_HMI              0x80u

typedef struct __attribute__((packed))
{
  uint32_t magic;          /* HVAC_CFGF_MAGIC */
  uint16_t format;         /* HVAC_CFGF_FORMAT */
  uint16_t module_count;
  uint16_t channel_count;
  uint16_t reserved0;
  char     project[HVAC_CFGF_PROJECT_LEN]; /* project number, ASCII, zero padded */
  uint32_t reserved1[2];
} HvacCfgHeader;           /* 44 bytes */

typedef struct __attribute__((packed))
{
  uint8_t type;            /* HVAC_CAT_* */
  uint8_t line;
  uint8_t rail;
  uint8_t place;
  uint8_t channels;        /* channel count of the module */
  uint8_t reserved[3];
} HvacCfgModule;           /* 8 bytes */

typedef struct __attribute__((packed))
{
  uint8_t module;          /* index in the module table */
  uint8_t channel;         /* 1..N */
  uint8_t signal;          /* HVAC_SIG_* */
  uint8_t quantity;        /* HVAC_QTY_*, for the display */
  int16_t min_x10;
  int16_t max_x10;
} HvacCfgChannel;          /* 8 bytes */

/* The HVAC application writes these sizes; keep both sides equal. */
_Static_assert(sizeof(HvacCfgHeader) == 44u, "HvacCfgHeader size");
_Static_assert(sizeof(HvacCfgModule) == 8u, "HvacCfgModule size");
_Static_assert(sizeof(HvacCfgChannel) == 8u, "HvacCfgChannel size");

#define HVAC_QTY_NONE             0u
#define HVAC_QTY_TEMPERATURE      1u
#define HVAC_QTY_HUMIDITY         2u
#define HVAC_QTY_PRESSURE         3u
#define HVAC_QTY_FLOW             4u
#define HVAC_QTY_OTHER            5u

static inline void hvac_cfg_put16(uint8_t *d, int16_t v)
{
  d[0] = (uint8_t)((uint16_t)v & 0xFFu);
  d[1] = (uint8_t)((uint16_t)v >> 8);
}

static inline int16_t hvac_cfg_get16(const uint8_t *d)
{
  return (int16_t)((uint16_t)d[0] | ((uint16_t)d[1] << 8));
}

#endif /* HVAC_CFG_H */
