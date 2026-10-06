/*
 * plc_id.h — identification walk: the Main PLC gives every module its place ID "line.rail.place"
 * over the MODUL_ID optocoupler lines (protocol: Shared_Libs/hvac_id.h).
 *
 *   line 1: MCU_MODUL_ID_OUT_AI      -> locomotives on CAN1, each walks its rail (RAIL output);
 *   line 2: MCU_MODUL_ID_OUT_DISPLAY -> HUBs of displays on CAN2, each polls its 9 ports.
 * An ID output is active high (SET = the next module's ID input is active).
 *
 * Rail numbers come from the configuration: the k-th head (locomotive / HUB) found on a line gets
 * the k-th rail of that line's heads in the configuration (ascending); heads beyond the
 * configuration get the next free rail numbers. Places: rail modules count from the locomotive
 * (1..31), displays get their HUB port (1..9).
 *
 * The walk runs after power-up, on request (HVAC application, new configuration) and when a module
 * reports that it has no ID (restarted, or connected later).
 */
#ifndef PLC_ID_H
#define PLC_ID_H

#include <stdint.h>
#include "hvac_id.h"

#define PLC_ID_MAX_FOUND   48u
#define PLC_ID_MAX_STRAY   8u

typedef struct
{
  uint32_t tag;     /* fwupd node tag, same as in plc_fwupd */
  uint8_t  cat;     /* HVAC_CAT_* */
  uint8_t  line;
  uint8_t  rail;
  uint8_t  place;
  uint8_t  board;
} PlcIdFound;

typedef struct
{
  uint8_t  busy;        /* walk in progress */
  uint8_t  count;       /* modules with an ID after the last walk */
  uint8_t  stray;       /* modules without ID that answer outside the chain (wiring, broken chain) */
  uint16_t walks;       /* completed walks since PLC start, 0 = none yet */
  uint32_t done_ms;     /* uptime at the end of the last walk */
} PlcIdSummary;

/* Creates the queue and idTask. Call before the scheduler starts. */
void PlcId_Start(void);

/* From the CAN RX interrupt: ID_HERE frame, bus = PLC_CAN_BUS_*. */
void PlcId_OnHereIsr(uint8_t bus, const uint8_t d[8]);

/* Asks for a new walk (any thread). */
void PlcId_RequestWalk(void);

void PlcId_GetSummary(PlcIdSummary *out);
/* Copies the found modules; returns the count. */
uint8_t PlcId_GetFound(PlcIdFound *out, uint8_t max);
/* Copies modules without ID outside the chain (line, rail, place = HVAC_ID_NONE); returns the count. */
uint8_t PlcId_GetStray(PlcIdFound *out, uint8_t max);

#endif /* PLC_ID_H */
