/*
 * plc_fwstore.h — module firmware images in W25Q128 (Docs/Firmware_Update_Concept.md, section 5).
 *
 * Every module type (FWUPD_TYPE_*, 1..15) owns 1 MB at type * 1 MB with two 512 KB slots.
 * Slot = 4 KB descriptor sector + image. A slot is CURRENT (copy used for new and replaced
 * modules), NEW (uploaded, waits for an update run) or BACKUP (previous CURRENT, for rollback).
 * An upload always goes to the slot that is not CURRENT, so the current copy is never touched.
 * The first 1 MB (type 0) is reserved.
 */
#ifndef PLC_FWSTORE_H
#define PLC_FWSTORE_H

#include <stdint.h>

#define FWSTORE_TYPES          16u
#define FWSTORE_IMAGE_MAX      (512u * 1024u - 4096u)

#define FWSTORE_ROLE_EMPTY     0u
#define FWSTORE_ROLE_CURRENT   1u
#define FWSTORE_ROLE_NEW       2u
#define FWSTORE_ROLE_BACKUP    3u

/* Upload result */
#define FWSTORE_OK             0u
#define FWSTORE_E_FLASH        1u /* W25Q128 missing or erase/program failed */
#define FWSTORE_E_SIZE         2u
#define FWSTORE_E_HEADER       3u /* no HVFW header or unknown module type */
#define FWSTORE_E_CRC          4u /* CRC from the client or read-back mismatch */
#define FWSTORE_E_BUSY         5u /* update run in progress or another upload */
#define FWSTORE_E_STATE        6u

typedef struct
{
  uint8_t  role;          /* FWSTORE_ROLE_* */
  uint8_t  module_type;
  uint8_t  board_rev;
  uint16_t version;
  uint32_t size;
  uint32_t crc;
  uint32_t seq;           /* grows with every descriptor write */
  uint32_t addr;          /* flash address of the image */
} FwStoreSlot;

/* Creates the mutexes. Call before the scheduler starts. */
void FwStore_Setup(void);
/* Probes the flash and loads all descriptors. Call once from a thread. */
uint8_t FwStore_Init(void);
uint8_t FwStore_FlashOk(void);

/* Copy of a slot descriptor; idx 0/1. */
void FwStore_GetSlot(uint8_t type, uint8_t idx, FwStoreSlot *out);
/* Finds the slot of a role for a type; returns 0 if none. */
uint8_t FwStore_Find(uint8_t type, uint8_t role, FwStoreSlot *out);

uint8_t FwStore_ReadImage(const FwStoreSlot *s, uint32_t off, void *buf, uint32_t len);

/* Streaming upload of a .bin; size and CRC32 come from the client (crc 0 = not checked). */
uint8_t FwStore_UploadBegin(uint32_t size, uint32_t crc);
uint8_t FwStore_UploadWrite(const uint8_t *data, uint32_t len);
uint8_t FwStore_UploadEnd(FwStoreSlot *out);
void FwStore_UploadAbort(void);
uint8_t FwStore_UploadActive(void);

/* NEW -> CURRENT, old CURRENT -> BACKUP. */
uint8_t FwStore_Promote(uint8_t type);
/* BACKUP -> CURRENT, old CURRENT -> BACKUP. */
uint8_t FwStore_Rollback(uint8_t type);

#endif /* PLC_FWSTORE_H */
