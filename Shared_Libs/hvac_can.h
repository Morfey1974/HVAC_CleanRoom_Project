/*
 * hvac_can.h — CAN frames shared by AI module, Locomotive, Main PLC, HUB and display boards.
 *
 * All buses: classic CAN, 11-bit IDs, 50 kbit/s.
 *
 * Path of the AI measurement frame:
 *   AI CAN2 -> Locomotive CAN1 -> Locomotive CAN2 -> Main PLC CAN1 -> Main PLC CAN2
 *   -> HUB CAN2 -> HUB CAN1 -> Display CAN1
 */
#ifndef HVAC_CAN_H
#define HVAC_CAN_H

#include <stdint.h>

/* AI measurement (two 4-20 mA channels of a T+RH transmitter), DLC 8.
 *   [0..1] humidity,    int16 little-endian, 0.01 %RH
 *   [2..3] temperature, int16 little-endian, 0.01 degC
 *   [4]    status, HVAC_AI_ST_*
 *   [5]    frame counter, incremented by the AI module
 *   [6..7] reserved, 0
 */
#define HVAC_CAN_ID_AI_MEAS       0x301u
#define HVAC_CAN_AI_MEAS_DLC      8u

#define HVAC_AI_ST_HUM_FAULT      0x01u /* humidity loop below 3.6 mA (open) or above 21 mA */
#define HVAC_AI_ST_TEMP_FAULT     0x02u /* temperature loop below 3.6 mA (open) or above 21 mA */
#define HVAC_AI_ST_ADC_FAULT      0x04u /* ADS1220 did not answer */
#define HVAC_AI_ST_NO_LINK        0x80u /* set by the locomotive (AI silent) or the Main PLC (locomotive silent) */

#define HVAC_AI_LINK_TIMEOUT_MS   2000u /* upstream silent this long -> NO_LINK frames */
#define HVAC_AI_NO_LINK_PERIOD_MS 500u
#define HVAC_DISPLAY_TIMEOUT_MS   3000u /* display: no AI frame this long -> show fault */

typedef struct
{
  float humidity_pct;
  float temperature_c;
  uint8_t status;
  uint8_t counter;
} HvacAiMeas;

static inline int16_t hvac_can_to_centi(float v)
{
  float c = v * 100.0f;
  if (c > 32767.0f) c = 32767.0f;
  if (c < -32768.0f) c = -32768.0f;
  return (int16_t)((c >= 0.0f) ? (c + 0.5f) : (c - 0.5f));
}

static inline void hvac_can_ai_encode(const HvacAiMeas *m, uint8_t d[8])
{
  int16_t h = hvac_can_to_centi(m->humidity_pct);
  int16_t t = hvac_can_to_centi(m->temperature_c);
  d[0] = (uint8_t)((uint16_t)h & 0xFFu);
  d[1] = (uint8_t)((uint16_t)h >> 8);
  d[2] = (uint8_t)((uint16_t)t & 0xFFu);
  d[3] = (uint8_t)((uint16_t)t >> 8);
  d[4] = m->status;
  d[5] = m->counter;
  d[6] = 0u;
  d[7] = 0u;
}

static inline void hvac_can_ai_decode(const uint8_t d[8], HvacAiMeas *m)
{
  int16_t h = (int16_t)((uint16_t)d[0] | ((uint16_t)d[1] << 8));
  int16_t t = (int16_t)((uint16_t)d[2] | ((uint16_t)d[3] << 8));
  m->humidity_pct = (float)h / 100.0f;
  m->temperature_c = (float)t / 100.0f;
  m->status = d[4];
  m->counter = d[5];
}

/* Doors state, read by the Main PLC from the DCM door controller over Modbus RTU
 * and sent by the Main PLC on CAN2 -> HUB -> Display. DLC 4.
 *   [0] closed mask: bit N = door N+1 (D-03..D-07), 1 = closed, 0 = open
 *   [1] status, HVAC_DOORS_ST_*
 *   [2] frame counter, incremented by the Main PLC
 *   [3] reserved, 0
 */
#define HVAC_CAN_ID_DOORS         0x310u
#define HVAC_CAN_DOORS_DLC        4u
#define HVAC_DOORS_COUNT          5u
#define HVAC_DOORS_MASK           0x1Fu
#define HVAC_DOORS_ST_NO_LINK     0x80u /* DCM did not answer 3 polls in a row */
#define HVAC_DOORS_PERIOD_MS      500u
#define HVAC_DOORS_TIMEOUT_MS     3000u /* display: no doors frame this long -> no link */

/* Doors master (DCM door controller with a CAN port) <-> Main PLC, on PLC CAN3. DLC 8 for all.
 * While DM_STATE frames arrive, the PLC takes the doors state from them instead of Modbus.
 *
 * HVAC_CAN_ID_DM_STATE, master -> PLC, every HVAC_DM_PERIOD_MS and at once on a change:
 *   [0] closed mask, bit N = door N+1, 1 = closed (bits 0..4 go to the displays, see HVAC_CAN_ID_DOORS)
 *   [1] locked mask, 1 = lock engaged
 *   [2] fault mask,  1 = door fault (sensor, drive)
 *   [3] status, HVAC_DM_ST_*
 *   [4] frame counter, incremented by the master
 *   [5] number of doors served, 1..8
 *   [6..7] master firmware version, little-endian, 0xMMmm
 *
 * HVAC_CAN_ID_DM_CMD, PLC -> master:
 *   [0] command, HVAC_DM_CMD_*
 *   [1] door mask the command applies to, 0xFF = all
 *   [2] sequence number, copied into the answer
 *   [3..7] reserved, 0
 *
 * HVAC_CAN_ID_DM_ACK, master -> PLC, answer to every DM_CMD:
 *   [0] sequence number of the command
 *   [1] command
 *   [2] result, HVAC_DM_RES_*
 *   [3..7] reserved, 0
 *
 * HVAC_CAN_ID_DM_PLC, PLC -> master, every HVAC_DM_PLC_PERIOD_MS:
 *   [0] frame counter, incremented by the PLC
 *   [1] PLC flags, HVAC_DM_PLC_*
 *   [2..3] PLC firmware version, little-endian
 *   [4..7] reserved, 0
 */
#define HVAC_CAN_ID_DM_STATE      0x320u
#define HVAC_CAN_ID_DM_CMD        0x321u
#define HVAC_CAN_ID_DM_ACK        0x322u
#define HVAC_CAN_ID_DM_PLC        0x323u

#define HVAC_DM_ST_LOCAL          0x01u /* master in local (manual) mode, PLC commands are refused */
#define HVAC_DM_ST_ALARM          0x02u /* door open too long or emergency release */
#define HVAC_DM_ST_INTERLOCK      0x04u /* a door is open, the others are held */

#define HVAC_DM_CMD_PING          0x00u /* link test: the master only answers */
#define HVAC_DM_CMD_LOCK          0x01u
#define HVAC_DM_CMD_UNLOCK        0x02u
#define HVAC_DM_CMD_OPEN          0x03u /* open the doors of the mask once */
#define HVAC_DM_CMD_ALARM_RESET   0x04u

#define HVAC_DM_RES_OK            0x00u
#define HVAC_DM_RES_BUSY          0x01u
#define HVAC_DM_RES_REFUSED       0x02u /* local mode or interlock */
#define HVAC_DM_RES_UNKNOWN       0x03u /* command not supported */

#define HVAC_DM_PLC_CFG_OK        0x01u /* PLC has a valid configuration */

#define HVAC_DM_PERIOD_MS         500u
#define HVAC_DM_TIMEOUT_MS        2000u /* PLC: no DM_STATE this long -> back to Modbus */
#define HVAC_DM_PLC_PERIOD_MS     1000u

#endif /* HVAC_CAN_H */
