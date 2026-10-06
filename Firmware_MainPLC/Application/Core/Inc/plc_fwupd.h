/*
 * plc_fwupd.h — Main PLC firmware update master on CAN1 (locomotives, rail modules) and CAN2 (HUBs, displays).
 *
 * Concept: Docs/Firmware_Update_Concept.md. Protocol: Shared_Libs/fwupd/fwupd_proto.h.
 * Images: plc_fwstore.h (W25Q128), uploaded from the HVAC application over Ethernet.
 *
 * Roll call: nodes announce themselves (application every 2 s, bootloader every 1 s), the PLC
 * broadcasts DISCOVER at start and every PLC_FWUPD_DISCOVER_MS.
 *
 * Automatic update with the CURRENT image of the node's type (only after PLC_FWUPD_SAME_ANN
 * identical announces in a row, board matches):
 *   - bootloader reports "no application" or "application failed" with another version;
 *   - bootloader waits (stopped by CONNECT) — started if the version matches, otherwise updated;
 *   - node first seen after PLC_FWUPD_LEARN_MS runs another version (module replaced).
 * A running known node with another version is only flagged (version_mismatch).
 * A silent node is never updated.
 *
 * Update run (command from the HVAC application): all linked nodes of one type get the NEW
 * image (or CURRENT if there is no NEW), one by one. If every node succeeded, NEW becomes CURRENT.
 * Rollback run: the same with the BACKUP image; if every node succeeded, BACKUP and CURRENT swap.
 */
#ifndef PLC_FWUPD_H
#define PLC_FWUPD_H

#include <stdint.h>

#define PLC_FWUPD_MAX_NODES     24u
#define PLC_FWUPD_LOG_LEN       32u

#define PLC_FWUPD_RES_NONE      0u
#define PLC_FWUPD_RES_OK        1u
#define PLC_FWUPD_RES_FAILED    2u /* all attempts failed, no more automatic attempts */

#define PLC_FWUPD_STEP_IDLE     0u
#define PLC_FWUPD_STEP_CONNECT  1u
#define PLC_FWUPD_STEP_ERASE    2u
#define PLC_FWUPD_STEP_BLOCKS   3u
#define PLC_FWUPD_STEP_VERIFY   4u
#define PLC_FWUPD_STEP_START    5u
#define PLC_FWUPD_STEP_CHECK    6u /* waiting for the new version in roll call */

#define PLC_FWUPD_E_TIMEOUT     0x80u
#define PLC_FWUPD_E_TX          0x81u
#define PLC_FWUPD_E_VERSION     0x82u /* started, but announces another version */
#define PLC_FWUPD_E_IMAGE       0x83u /* image could not be read from the store */

/* Update run modes */
#define PLC_FWUPD_MODE_DIFFERENT 1u /* nodes whose version differs from the image */
#define PLC_FWUPD_MODE_ALL       2u /* every node of the type */

/* PlcFwupd_RequestRun result */
#define PLC_FWUPD_REQ_OK        0u
#define PLC_FWUPD_REQ_BUSY      1u
#define PLC_FWUPD_REQ_NO_IMAGE  2u
#define PLC_FWUPD_REQ_ARG       3u
#define PLC_FWUPD_REQ_NO_BACKUP 4u

/* Log events */
#define PLC_FWUPD_EV_NODE_NEW    1u
#define PLC_FWUPD_EV_NODE_LOST   2u
#define PLC_FWUPD_EV_NODE_STATE  3u /* ver_from = old state, ver_to = new state */
#define PLC_FWUPD_EV_UPD_START   4u
#define PLC_FWUPD_EV_UPD_OK      5u
#define PLC_FWUPD_EV_UPD_FAIL    6u /* err, step */
#define PLC_FWUPD_EV_STARTED     7u /* stopped node started without update */
#define PLC_FWUPD_EV_RUN_START   8u
#define PLC_FWUPD_EV_RUN_END     9u /* err = failed nodes */
#define PLC_FWUPD_EV_PROMOTE     10u
#define PLC_FWUPD_EV_UPLOAD_OK   11u
#define PLC_FWUPD_EV_UPLOAD_FAIL 12u /* err = FWSTORE_E_* */
#define PLC_FWUPD_EV_RUN_CANCEL  13u
/* Configuration (plc_cfg), ver_to = generation */
#define PLC_FWUPD_EV_CFG_SAVED   14u /* err = PLC_CFG_E_* if the upload failed */
#define PLC_FWUPD_EV_CFG_OK      15u /* every checkable module confirmed the configuration */
#define PLC_FWUPD_EV_CFG_ERROR   16u /* module_type = catalogue type, err = PLC_CFG_ERR_* / HVAC_CFG_E_* */
#define PLC_FWUPD_EV_CFG_RESEND  17u /* module reported another generation, settings sent again */
#define PLC_FWUPD_EV_ROLLBACK    18u /* BACKUP became CURRENT after a rollback run */
#define PLC_FWUPD_EV_ID_WALK     19u /* identification walk (plc_id): ver_from = modules found, err = problems */

typedef struct
{
  uint32_t tag;              /* node tag (24 bits), 0 = free slot */
  uint8_t  module_type;
  uint8_t  board_rev;
  uint8_t  state;            /* FWUPD_ST_* */
  uint8_t  boot_version;
  uint16_t app_version;
  uint8_t  boot_fails;
  uint8_t  link;             /* 1: announce within PLC_FWUPD_LINK_MS */
  uint8_t  same_ann;         /* identical announces in a row */
  uint8_t  version_mismatch; /* running, version differs from the CURRENT image */
  uint8_t  result;           /* PLC_FWUPD_RES_* of the last update */
  uint8_t  attempts;         /* update attempts in the last run */
  uint8_t  last_err;         /* FWUPD_ERR_* or PLC_FWUPD_E_* of the last failure */
  uint8_t  last_step;        /* PLC_FWUPD_STEP_* where it failed */
  uint8_t  learned;          /* 1: seen during the learning window after PLC start */
  uint8_t  bus;              /* 1 = CAN1, 2 = CAN2 */
  uint32_t last_seen;        /* tick */
} PlcFwupdNode;

typedef struct
{
  uint32_t id;               /* 1, 2, ... since PLC start */
  uint32_t t_ms;             /* PLC uptime */
  uint32_t tag;
  uint8_t  module_type;
  uint8_t  event;            /* PLC_FWUPD_EV_* */
  uint8_t  err;
  uint8_t  step;
  uint16_t ver_from;
  uint16_t ver_to;
} PlcFwupdLog;

typedef struct
{
  PlcFwupdNode nodes[PLC_FWUPD_MAX_NODES];
  uint8_t  store_ok;         /* W25Q128 answers */
  uint8_t  run_active;
  uint8_t  run_type;
  uint8_t  run_mode;
  uint16_t run_version;
  uint8_t  run_done;         /* nodes processed in this run */
  uint8_t  run_failed;
  uint8_t  run_rollback;     /* 1: the run sends the BACKUP image */
  uint8_t  busy;             /* a node is being updated */
  uint32_t busy_tag;
  uint8_t  step;             /* PLC_FWUPD_STEP_* */
  uint8_t  progress;         /* 0..100 % of blocks */
  uint32_t block_retries;    /* total resent blocks */
  uint32_t updates_ok;
  uint32_t updates_failed;
  uint32_t rx_frames;
  uint32_t rx_drop;
} PlcFwupdStatus;

extern volatile PlcFwupdStatus g_plc_fwupd;
extern volatile uint32_t g_plc_fwupd_block_delay_ms; /* test only: pause after each block, 0 = off */

/* Creates the queue, the store mutexes and fwupdTask. Call before the scheduler starts. */
void PlcFwupd_Start(void);

/* From the CAN RX interrupt: extended data frame, bus 1 = CAN1, 2 = CAN2. */
void PlcFwupd_OnRxIsr(uint8_t bus, uint32_t id, const uint8_t data[8]);

/* Update run for one module type (PLC_FWUPD_MODE_*). */
uint8_t PlcFwupd_RequestRun(uint8_t module_type, uint8_t mode);
/* Rollback run: nodes of the type whose version differs from the BACKUP image get it. */
uint8_t PlcFwupd_RequestRollback(uint8_t module_type);
/* Stops the run after the node in progress. */
void PlcFwupd_Cancel(void);

/* Adds a log entry (any thread). */
void PlcFwupd_Log(uint8_t event, uint32_t tag, uint8_t module_type, uint8_t err, uint8_t step,
                  uint16_t ver_from, uint16_t ver_to);
/* Copies log entries with id > after_id, oldest first; returns the count. */
uint32_t PlcFwupd_GetLog(uint32_t after_id, PlcFwupdLog *out, uint32_t max);

#endif /* PLC_FWUPD_H */
