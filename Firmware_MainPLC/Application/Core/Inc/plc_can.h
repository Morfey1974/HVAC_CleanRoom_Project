/*
 * plc_can.h — Main PLC CAN gateway.
 *
 *   CAN1 (FDCAN1) — ID line 1: locomotives and their rails.
 *   CAN2 (FDCAN2) — ID line 2: HUBs of displays.
 *   CAN3 (FDCAN3) — ID line 3: Phoenix DIN-rail bus (locomotives, HUBs) and the doors master.
 *
 * AI measurement frames (HVAC_CAN_ID_AI_MEAS) received on CAN1 or CAN3 are forwarded
 * unchanged to CAN2 and CAN3 (not back to the bus they came from). If no AI frame arrives
 * for HVAC_AI_LINK_TIMEOUT_MS, the PLC sends NO_LINK frames itself.
 * Every HVAC_DOORS_PERIOD_MS the PLC sends HVAC_CAN_ID_DOORS on CAN2 and CAN3; the state
 * comes from the doors master on CAN3 (hvac_can.h, DM_*) while it talks, otherwise from Modbus.
 * Module configuration frames (hvac_cfg.h) go on the bus of the module's line, see plc_cfg.
 */
#ifndef PLC_CAN_H
#define PLC_CAN_H

#include <stdint.h>
#include "hvac_can.h"

typedef struct
{
  HvacAiMeas meas;       /* last AI frame (or NO_LINK frame sent by the PLC) */
  uint8_t  loco_link;    /* 1: AI frame within HVAC_AI_LINK_TIMEOUT_MS */
  uint8_t  init_ok;      /* 1: CAN1 and CAN2 started */
  uint8_t  loco_bus_off; /* bus-off events counter, CAN1 */
  uint8_t  hub_bus_off;  /* bus-off events counter, CAN2 */
  uint8_t  doors_src;    /* PLC_DOORS_SRC_* of the last doors frame */
  uint32_t age_ms;       /* time since last AI frame */
  uint32_t rx_loco;      /* AI frames received on CAN1 or CAN3 */
  uint32_t rx_hub;       /* any frames received on CAN2 */
  uint32_t tx_hub;       /* frames sent on CAN2 */
  uint32_t tx_hub_err;   /* CAN2 TX FIFO full or HAL error */
  uint32_t rx_drop;      /* AI frames lost: gateway queue full */
} PlcAiSnapshot;

#define PLC_DOORS_SRC_NONE    0u
#define PLC_DOORS_SRC_MODBUS  1u
#define PLC_DOORS_SRC_CAN3    2u

/* Doors master state (frames DM_STATE / DM_ACK on CAN3). */
typedef struct
{
  uint8_t  link;         /* 1: DM_STATE within HVAC_DM_TIMEOUT_MS */
  uint8_t  closed;
  uint8_t  locked;
  uint8_t  fault;
  uint8_t  status;       /* HVAC_DM_ST_* */
  uint8_t  counter;
  uint8_t  doors;
  uint16_t version;
  uint32_t age_ms;       /* since the last DM_STATE, 0xFFFFFFFF: never */
  uint32_t rx_state;
  uint8_t  ack_valid;
  uint8_t  ack_seq;
  uint8_t  ack_cmd;
  uint8_t  ack_res;      /* HVAC_DM_RES_* */
  uint32_t ack_age_ms;
  uint8_t  cmd_seq;      /* sequence number of the last command sent */
  uint8_t  cmd_code;
  uint32_t tx_plc;       /* DM_PLC frames sent */
} PlcDoorsMaster;

/* Received standard frame on CAN3, for the bench check page. */
typedef struct
{
  uint32_t age_ms;
  uint16_t id;
  uint8_t  dlc;
  uint8_t  d[8];
} PlcCan3Frame;

#define PLC_CAN3_LOG_LEN  16u

typedef struct
{
  uint8_t  ok;           /* 1: FDCAN3 started */
  uint8_t  bus_off;      /* bus-off events counter */
  uint8_t  err_passive;  /* 1: error-passive now (no other node acknowledges) */
  uint8_t  tec;          /* transmit error counter */
  uint8_t  rec;          /* receive error counter */
  uint32_t rx;           /* standard frames received */
  uint32_t rx_ext;       /* extended frames received (firmware update) */
  uint32_t tx;           /* frames queued */
  uint32_t tx_err;       /* TX FIFO full or HAL error */
  PlcDoorsMaster dm;
} PlcCan3Snapshot;

/* Body of canTask, never returns. */
void PlcCan_Task(void);

/* Thread-safe copy of the gateway state, for the web page. */
void PlcCan_GetSnapshot(PlcAiSnapshot *out);

/* CAN3 state and up to max last received standard frames (newest first); returns their count. */
uint8_t PlcCan_GetCan3(PlcCan3Snapshot *out, PlcCan3Frame *frames, uint8_t max);

#define PLC_CAN_BUS_LOCO  1u /* CAN1: ID line 1 */
#define PLC_CAN_BUS_HUB   2u /* CAN2: ID line 2 */
#define PLC_CAN_BUS_BUS   3u /* CAN3: ID line 3 */
#define PLC_CAN_BUSES     3u

/* Bus of an ID line (1..3); anything else -> CAN1. */
static inline uint8_t PlcCan_LineBus(uint8_t line)
{
  return (line >= 1u && line <= PLC_CAN_BUSES) ? line : PLC_CAN_BUS_LOCO;
}

/* Queues one CAN1 frame from any thread (shared with plc_fwupd); returns 1 if queued. */
uint8_t PlcCan_SendLoco(uint32_t id, uint8_t is_ext, const uint8_t d[8]);
/* Same for any bus (PLC_CAN_BUS_*), DLC 8. */
uint8_t PlcCan_Send(uint8_t bus, uint32_t id, uint8_t is_ext, const uint8_t d[8]);
/* Standard frame with any DLC (0..8) on CAN3, for the bench check. */
uint8_t PlcCan_SendCan3Raw(uint16_t id, uint8_t dlc, const uint8_t *d);
/* Command to the doors master (HVAC_DM_CMD_*); *seq = its sequence number. */
uint8_t PlcCan_DoorsCmd(uint8_t cmd, uint8_t mask, uint8_t *seq);

/* 1: CAN2 frames are acknowledged by someone (not error-passive, not bus-off). */
uint8_t PlcCan_HubBusOk(void);

#endif /* PLC_CAN_H */
