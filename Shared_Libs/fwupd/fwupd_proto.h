/*
 * fwupd_proto.h — HVAC firmware update protocol over CAN (bootloaders, applications, Main PLC).
 *
 * Concept: Docs/Firmware_Update_Concept.md.
 *
 * Classic CAN, 29-bit extended IDs, 8-byte frames:
 *   ID[28:24] message type (FWUPD_MSG_*)
 *   ID[23:0]  node tag = low 24 bits of CRC32 over the 96-bit chip UID; FWUPD_NODE_ALL = broadcast.
 * Extended IDs never collide with the 11-bit application frames (hvac_can.h).
 *
 * Update sequence (Main PLC -> node, node answers FWUPD_MSG_ACK unless noted):
 *   DISCOVER (broadcast)  -> every node answers ANNOUNCE
 *   CONNECT               -> application reboots into the bootloader, bootloader answers ANNOUNCE (state BOOT_WAIT)
 *   ERASE                 -> erases the application area and boot metadata
 *   BLOCK + 32 x DATA     -> one block of up to 256 bytes at an offset, programmed after its CRC16 matches
 *   VERIFY                -> CRC32 of the whole image; on success the first double word and the metadata are written
 *   START                 -> bootloader starts the application
 *
 * The first double word of the image (initial SP + reset vector) is programmed last, so an interrupted
 * update never leaves a startable application.
 */
#ifndef FWUPD_PROTO_H
#define FWUPD_PROTO_H

#include <stdint.h>

#define FWUPD_ID_IS_EXT            1u
#define FWUPD_NODE_ALL             0xFFFFFFu
#define FWUPD_NODE_MASK            0xFFFFFFu

#define FWUPD_ID(type, node)       ((((uint32_t)(type) & 0x1Fu) << 24) | ((uint32_t)(node) & FWUPD_NODE_MASK))
#define FWUPD_ID_TYPE(id)          (((uint32_t)(id) >> 24) & 0x1Fu)
#define FWUPD_ID_NODE(id)          ((uint32_t)(id) & FWUPD_NODE_MASK)

/* Main PLC -> node */
#define FWUPD_MSG_DISCOVER         0x01u /* no data */
#define FWUPD_MSG_CONNECT          0x02u /* [0..3] FWUPD_CONNECT_MAGIC, [4..7] FWUPD_CONNECT_KEY(node tag) */
#define FWUPD_MSG_ERASE            0x03u /* [0..3] image size, bytes */
#define FWUPD_MSG_BLOCK            0x04u /* [0..3] offset from app start, [4..5] length (8..256, multiple of 8), [6..7] CRC16 of the block */
#define FWUPD_MSG_DATA             0x05u /* 8 bytes of the current block */
#define FWUPD_MSG_VERIFY           0x06u /* [0..3] image size, [4..7] CRC32 of the image */
#define FWUPD_MSG_START            0x07u /* no data */

/* node -> Main PLC */
#define FWUPD_MSG_ANNOUNCE         0x10u /* FwupdAnnounce */
#define FWUPD_MSG_ACK              0x11u /* [0] message type acknowledged, [1] FWUPD_ERR_*, [2..5] parameter (offset, size) */

#define FWUPD_CONNECT_MAGIC        0x21445055u /* "UPD!" */
#define FWUPD_CONNECT_KEY(tag)     (~(uint32_t)(tag)) /* repeats the addressed node tag inside the payload */

#define FWUPD_BLOCK_MAX            256u
#define FWUPD_ANNOUNCE_APP_MS      2000u /* application roll call period */
#define FWUPD_ANNOUNCE_BOOT_MS     1000u /* bootloader roll call period */

/* Module types (the same value is compiled into the bootloader and into the application header). */
#define FWUPD_TYPE_MAIN_PLC        0x01u
#define FWUPD_TYPE_LOCOMOTIVE      0x02u
#define FWUPD_TYPE_HUB_DISPLAYS    0x03u
#define FWUPD_TYPE_DISPLAY_TFT43   0x04u
#define FWUPD_TYPE_AI              0x05u
#define FWUPD_TYPE_AO              0x06u
#define FWUPD_TYPE_DI              0x07u
#define FWUPD_TYPE_DO              0x08u

/* Node state in ANNOUNCE */
#define FWUPD_ST_APP_RUNNING       0x00u /* application answers */
#define FWUPD_ST_BOOT_EMPTY        0x01u /* bootloader: no valid application */
#define FWUPD_ST_BOOT_WAIT         0x02u /* bootloader: stays on request (CONNECT), application is valid */
#define FWUPD_ST_BOOT_APP_FAILED   0x03u /* bootloader: application did not confirm start several times */
#define FWUPD_ST_BOOT_UPDATING     0x04u /* bootloader: erase/program in progress */

/* ACK status */
#define FWUPD_ERR_OK               0x00u
#define FWUPD_ERR_STATE            0x01u /* command not allowed now (e.g. DATA without BLOCK) */
#define FWUPD_ERR_RANGE            0x02u /* offset/size outside the application area */
#define FWUPD_ERR_CRC              0x03u /* block CRC16 or image CRC32 mismatch */
#define FWUPD_ERR_FLASH            0x04u /* erase/program failed */
#define FWUPD_ERR_HEADER           0x05u /* image header: wrong magic, module type or board */

/* ANNOUNCE payload, 8 bytes */
typedef struct
{
  uint8_t  module_type;  /* FWUPD_TYPE_* */
  uint8_t  board_rev;
  uint8_t  state;        /* FWUPD_ST_* */
  uint8_t  boot_version;
  uint16_t app_version;  /* 0 if no valid application; major << 8 | minor */
  uint8_t  boot_fails;   /* unconfirmed application starts in a row */
  uint8_t  reserved;
} FwupdAnnounce;

/* Application image header, placed by the linker at FWUPD_HEADER_OFFSET from the application start. */
#define FWUPD_HEADER_OFFSET        0x100u
#define FWUPD_HEADER_MAGIC         0x57465648u /* "HVFW" */

typedef struct
{
  uint32_t magic;        /* FWUPD_HEADER_MAGIC */
  uint8_t  header_ver;   /* 1 */
  uint8_t  module_type;  /* FWUPD_TYPE_* */
  uint8_t  board_rev;
  uint8_t  reserved0;
  uint16_t fw_version;   /* major << 8 | minor */
  uint16_t reserved1;
  uint32_t image_end;    /* absolute address of the first byte after the image (linker symbol) */
  uint32_t reserved2[3];
} FwupdHeader;

/* Shared RAM word block (not initialised by startup code), between bootloader and application. */
#define FWUPD_SHARED_MAGIC         0xB007C0DEu
#define FWUPD_REQ_NONE             0u
#define FWUPD_REQ_STAY_IN_BOOT     1u

typedef struct
{
  uint32_t magic;        /* FWUPD_SHARED_MAGIC when the rest is valid */
  uint32_t request;      /* FWUPD_REQ_* set by the application before reset */
  uint32_t boot_fails;   /* incremented by the bootloader before each start, cleared by the application */
  uint32_t check;        /* ~(magic ^ request ^ boot_fails) */
} FwupdShared;

static inline uint32_t fwupd_crc32_update(uint32_t crc, const uint8_t *p, uint32_t n)
{
  static const uint32_t t[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
  };
  crc = ~crc;
  while (n-- > 0u)
  {
    crc ^= *p++;
    crc = (crc >> 4) ^ t[crc & 0x0Fu];
    crc = (crc >> 4) ^ t[crc & 0x0Fu];
  }
  return ~crc;
}

/* Standard CRC-32 (IEEE 802.3, as zlib/Python binascii.crc32). */
static inline uint32_t fwupd_crc32(const uint8_t *p, uint32_t n)
{
  return fwupd_crc32_update(0u, p, n);
}

/* CRC-16/CCITT-FALSE, for blocks. */
static inline uint16_t fwupd_crc16(const uint8_t *p, uint32_t n)
{
  uint16_t crc = 0xFFFFu;
  while (n-- > 0u)
  {
    crc ^= (uint16_t)((uint16_t)*p++ << 8);
    for (uint32_t i = 0u; i < 8u; i++)
    {
      crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

static inline uint32_t fwupd_node_tag(const uint32_t uid[3])
{
  uint32_t tag = fwupd_crc32((const uint8_t *)uid, 12u) & FWUPD_NODE_MASK;
  return (tag == FWUPD_NODE_ALL) ? 0x000001u : tag;
}

static inline void fwupd_put32(uint8_t *d, uint32_t v)
{
  d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); d[2] = (uint8_t)(v >> 16); d[3] = (uint8_t)(v >> 24);
}

static inline uint32_t fwupd_get32(const uint8_t *d)
{
  return (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24);
}

static inline uint16_t fwupd_get16(const uint8_t *d)
{
  return (uint16_t)((uint16_t)d[0] | ((uint16_t)d[1] << 8));
}

static inline uint32_t fwupd_shared_check(const FwupdShared *s)
{
  return ~(s->magic ^ s->request ^ s->boot_fails);
}

#endif /* FWUPD_PROTO_H */
