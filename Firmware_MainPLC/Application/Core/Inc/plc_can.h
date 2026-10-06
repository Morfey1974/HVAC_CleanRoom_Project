/*
 * plc_can.h — Main PLC CAN gateway.
 *
 *   CAN1 (FDCAN1) — bus to the Locomotive module.
 *   CAN2 (FDCAN2) — bus to the HUB of displays.
 *
 * AI measurement frames (HVAC_CAN_ID_AI_MEAS) received from the Locomotive are
 * forwarded unchanged to the HUB. If the Locomotive is silent for
 * HVAC_AI_LINK_TIMEOUT_MS, the PLC sends NO_LINK frames to the HUB itself.
 * Every HVAC_DOORS_PERIOD_MS the PLC also sends HVAC_CAN_ID_DOORS to the HUB.
 * Module configuration frames (hvac_cfg.h) go through CAN1, see plc_cfg.
 */
#ifndef PLC_CAN_H
#define PLC_CAN_H

#include <stdint.h>
#include "hvac_can.h"

typedef struct
{
  HvacAiMeas meas;       /* last AI frame (or NO_LINK frame sent by the PLC) */
  uint8_t  loco_link;    /* 1: Locomotive frame within HVAC_AI_LINK_TIMEOUT_MS */
  uint8_t  init_ok;      /* 1: both FDCAN started */
  uint8_t  loco_bus_off; /* bus-off events counter, CAN1 */
  uint8_t  hub_bus_off;  /* bus-off events counter, CAN2 */
  uint32_t age_ms;       /* time since last Locomotive frame */
  uint32_t rx_loco;      /* AI frames received on CAN1 */
  uint32_t rx_hub;       /* any frames received on CAN2 */
  uint32_t tx_hub;       /* frames sent on CAN2 */
  uint32_t tx_hub_err;   /* CAN2 TX FIFO full or HAL error */
  uint32_t rx_drop;      /* CAN1 frames lost: gateway queue full */
} PlcAiSnapshot;

/* Body of canTask, never returns. */
void PlcCan_Task(void);

/* Thread-safe copy of the gateway state, for the web page. */
void PlcCan_GetSnapshot(PlcAiSnapshot *out);

#define PLC_CAN_BUS_LOCO  1u /* CAN1: locomotives and their rails, ID line 1 */
#define PLC_CAN_BUS_HUB   2u /* CAN2: HUBs of displays, ID line 2 */

/* Queues one CAN1 frame from any thread (shared with plc_fwupd); returns 1 if queued. */
uint8_t PlcCan_SendLoco(uint32_t id, uint8_t is_ext, const uint8_t d[8]);
/* Same for either bus (PLC_CAN_BUS_*), DLC 8. */
uint8_t PlcCan_Send(uint8_t bus, uint32_t id, uint8_t is_ext, const uint8_t d[8]);

/* 1: CAN2 frames are acknowledged by someone (not error-passive, not bus-off). */
uint8_t PlcCan_HubBusOk(void);

#endif /* PLC_CAN_H */
