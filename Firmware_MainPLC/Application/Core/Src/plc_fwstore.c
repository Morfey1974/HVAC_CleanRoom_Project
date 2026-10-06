/*
 * plc_fwstore.c — module firmware images in W25Q128.
 */
#include "plc_fwstore.h"

#include <stddef.h>
#include <string.h>
#include "cmsis_os2.h"
#include "plc_w25q.h"
#include "fwupd/fwupd_proto.h"

#define FWSTORE_TYPE_SIZE      0x100000u
#define FWSTORE_SLOT_SIZE      0x80000u
#define FWSTORE_DESC_SIZE      4096u
#define FWSTORE_MAGIC          0x54535746u /* "FWST" */
#define FWSTORE_HEAD_LEN       512u        /* buffered until the image header is known */

typedef struct
{
  uint32_t magic;
  uint32_t seq;
  uint8_t  role;
  uint8_t  module_type;
  uint8_t  board_rev;
  uint8_t  reserved0;
  uint16_t version;
  uint16_t reserved1;
  uint32_t size;
  uint32_t crc;
  uint32_t check;        /* CRC32 of the fields above */
} FwStoreDesc;

static FwStoreSlot s_slots[FWSTORE_TYPES][2];
static uint32_t s_seq;
static uint8_t s_flash_ok;
static osMutexId_t s_mtx;

static struct
{
  uint8_t  active;
  uint8_t  slot_idx;
  uint8_t  type;
  uint32_t size;
  uint32_t crc_expected;
  uint32_t crc;
  uint32_t received;
  uint8_t  head[FWSTORE_HEAD_LEN];
} s_up;

static uint32_t FwStore_SlotBase(uint8_t type, uint8_t idx)
{
  return (uint32_t)type * FWSTORE_TYPE_SIZE + (uint32_t)idx * FWSTORE_SLOT_SIZE;
}

static uint32_t FwStore_DescCheck(const FwStoreDesc *d)
{
  return fwupd_crc32((const uint8_t *)d, offsetof(FwStoreDesc, check));
}

static void FwStore_Lock(void)   { if (s_mtx != NULL) osMutexAcquire(s_mtx, osWaitForever); }
static void FwStore_Unlock(void) { if (s_mtx != NULL) osMutexRelease(s_mtx); }

static void FwStore_LoadSlot(uint8_t type, uint8_t idx)
{
  FwStoreDesc d;
  FwStoreSlot *s = &s_slots[type][idx];
  uint32_t base = FwStore_SlotBase(type, idx);

  memset(s, 0, sizeof(*s));
  s->addr = base + FWSTORE_DESC_SIZE;
  if (!W25q_Read(base, &d, sizeof(d))) return;
  if (d.magic != FWSTORE_MAGIC || d.check != FwStore_DescCheck(&d)) return;
  if (d.module_type != type || d.size == 0u || d.size > FWSTORE_IMAGE_MAX) return;
  if (d.role < FWSTORE_ROLE_CURRENT || d.role > FWSTORE_ROLE_BACKUP) return;

  s->role = d.role;
  s->module_type = d.module_type;
  s->board_rev = d.board_rev;
  s->version = d.version;
  s->size = d.size;
  s->crc = d.crc;
  s->seq = d.seq;
  if (d.seq > s_seq) s_seq = d.seq;
}

static uint8_t FwStore_WriteDesc(uint8_t type, uint8_t idx, const FwStoreSlot *v)
{
  FwStoreDesc d;
  uint32_t base = FwStore_SlotBase(type, idx);

  memset(&d, 0xFF, sizeof(d));
  d.magic = FWSTORE_MAGIC;
  d.seq = ++s_seq;
  d.role = v->role;
  d.module_type = type;
  d.board_rev = v->board_rev;
  d.version = v->version;
  d.size = v->size;
  d.crc = v->crc;
  d.check = FwStore_DescCheck(&d);

  if (!W25q_Erase(base, FWSTORE_DESC_SIZE) || !W25q_Write(base, &d, sizeof(d))) return 0u;
  s_slots[type][idx] = *v;
  s_slots[type][idx].module_type = type;
  s_slots[type][idx].seq = d.seq;
  s_slots[type][idx].addr = base + FWSTORE_DESC_SIZE;
  return 1u;
}

/* Two CURRENT slots after a power loss inside Promote: the newer one wins. */
static void FwStore_Resolve(uint8_t type)
{
  FwStoreSlot *a = &s_slots[type][0];
  FwStoreSlot *b = &s_slots[type][1];

  if (a->role == FWSTORE_ROLE_CURRENT && b->role == FWSTORE_ROLE_CURRENT)
  {
    if (a->seq > b->seq) b->role = FWSTORE_ROLE_BACKUP; else a->role = FWSTORE_ROLE_BACKUP;
  }
}

static uint32_t FwStore_CrcFlash(uint32_t addr, uint32_t size)
{
  static uint8_t buf[256];
  uint32_t crc = 0u;

  for (uint32_t off = 0u; off < size; off += sizeof(buf))
  {
    uint32_t n = (size - off > sizeof(buf)) ? sizeof(buf) : (size - off);
    if (!W25q_Read(addr + off, buf, n)) return ~crc;
    crc = fwupd_crc32_update(crc, buf, n);
  }
  return crc;
}

void FwStore_Setup(void)
{
  s_mtx = osMutexNew(NULL);
  W25q_Setup();
}

uint8_t FwStore_Init(void)
{
  memset(s_slots, 0, sizeof(s_slots));
  s_seq = 0u;
  s_flash_ok = W25q_Probe();
  if (!s_flash_ok) return 0u;

  for (uint8_t t = 1u; t < FWSTORE_TYPES; t++)
  {
    FwStore_LoadSlot(t, 0u);
    FwStore_LoadSlot(t, 1u);
    FwStore_Resolve(t);
  }
  return 1u;
}

uint8_t FwStore_FlashOk(void)
{
  return s_flash_ok;
}

void FwStore_GetSlot(uint8_t type, uint8_t idx, FwStoreSlot *out)
{
  memset(out, 0, sizeof(*out));
  if (type >= FWSTORE_TYPES || idx > 1u) return;
  FwStore_Lock();
  *out = s_slots[type][idx];
  FwStore_Unlock();
}

uint8_t FwStore_Find(uint8_t type, uint8_t role, FwStoreSlot *out)
{
  uint8_t found = 0u;

  if (type == 0u || type >= FWSTORE_TYPES) return 0u;
  FwStore_Lock();
  for (uint8_t i = 0u; i < 2u && !found; i++)
  {
    if (s_slots[type][i].role == role)
    {
      *out = s_slots[type][i];
      found = 1u;
    }
  }
  FwStore_Unlock();
  return found;
}

uint8_t FwStore_ReadImage(const FwStoreSlot *s, uint32_t off, void *buf, uint32_t len)
{
  if (off + len > ((s->size + 7u) & ~7u)) return 0u;
  return W25q_Read(s->addr + off, buf, len);
}

/* ---------- upload ---------- */

uint8_t FwStore_UploadActive(void)
{
  return s_up.active;
}

void FwStore_UploadAbort(void)
{
  s_up.active = 0u;
}

uint8_t FwStore_UploadBegin(uint32_t size, uint32_t crc)
{
  if (!s_flash_ok) return FWSTORE_E_FLASH;
  if (s_up.active) return FWSTORE_E_BUSY;
  if (size < FWUPD_HEADER_OFFSET + sizeof(FwupdHeader) || size > FWSTORE_IMAGE_MAX || (size & 7u)) return FWSTORE_E_SIZE;

  memset(&s_up, 0, sizeof(s_up));
  s_up.size = size;
  s_up.crc_expected = crc;
  s_up.active = 1u;
  return FWSTORE_OK;
}

/* Header known: choose the slot that is not CURRENT, erase it, program the buffered head. */
static uint8_t FwStore_UploadStart(void)
{
  const FwupdHeader *h = (const FwupdHeader *)&s_up.head[FWUPD_HEADER_OFFSET];
  uint32_t head_len = (s_up.size < FWSTORE_HEAD_LEN) ? s_up.size : FWSTORE_HEAD_LEN;
  uint8_t idx;
  uint8_t ok;

  if (h->magic != FWUPD_HEADER_MAGIC || h->module_type == 0u || h->module_type >= FWSTORE_TYPES) return FWSTORE_E_HEADER;

  FwStore_Lock();
  s_up.type = h->module_type;
  idx = (s_slots[s_up.type][0].role == FWSTORE_ROLE_CURRENT) ? 1u : 0u;
  if (s_slots[s_up.type][idx].role == FWSTORE_ROLE_CURRENT) idx ^= 1u;
  s_up.slot_idx = idx;
  memset(&s_slots[s_up.type][idx], 0, sizeof(FwStoreSlot));
  s_slots[s_up.type][idx].addr = FwStore_SlotBase(s_up.type, idx) + FWSTORE_DESC_SIZE;
  ok = W25q_Erase(FwStore_SlotBase(s_up.type, idx), FWSTORE_DESC_SIZE + s_up.size) &&
       W25q_Write(s_slots[s_up.type][idx].addr, s_up.head, head_len);
  FwStore_Unlock();
  return ok ? FWSTORE_OK : FWSTORE_E_FLASH;
}

uint8_t FwStore_UploadWrite(const uint8_t *data, uint32_t len)
{
  uint8_t r;

  if (!s_up.active) return FWSTORE_E_STATE;
  if (s_up.received + len > s_up.size) { s_up.active = 0u; return FWSTORE_E_SIZE; }

  s_up.crc = fwupd_crc32_update(s_up.crc, data, len);

  if (s_up.received < FWSTORE_HEAD_LEN)
  {
    uint32_t head_len = (s_up.size < FWSTORE_HEAD_LEN) ? s_up.size : FWSTORE_HEAD_LEN;
    uint32_t n = head_len - s_up.received;

    if (n > len) n = len;
    memcpy(&s_up.head[s_up.received], data, n);
    s_up.received += n;
    data += n;
    len -= n;
    if (s_up.received < head_len) return FWSTORE_OK;

    r = FwStore_UploadStart();
    if (r != FWSTORE_OK) { s_up.active = 0u; return r; }
  }

  if (len > 0u)
  {
    if (!W25q_Write(s_slots[s_up.type][s_up.slot_idx].addr + s_up.received, data, len))
    {
      s_up.active = 0u;
      return FWSTORE_E_FLASH;
    }
    s_up.received += len;
  }
  return FWSTORE_OK;
}

uint8_t FwStore_UploadEnd(FwStoreSlot *out)
{
  const FwupdHeader *h = (const FwupdHeader *)&s_up.head[FWUPD_HEADER_OFFSET];
  FwStoreSlot v;
  uint8_t ok;

  if (!s_up.active) return FWSTORE_E_STATE;
  s_up.active = 0u;
  if (s_up.received != s_up.size) return FWSTORE_E_SIZE;
  if (s_up.crc_expected != 0u && s_up.crc != s_up.crc_expected) return FWSTORE_E_CRC;

  memset(&v, 0, sizeof(v));
  v.role = FWSTORE_ROLE_NEW;
  v.board_rev = h->board_rev;
  v.version = h->fw_version;
  v.size = s_up.size;
  v.crc = s_up.crc;

  if (FwStore_CrcFlash(FwStore_SlotBase(s_up.type, s_up.slot_idx) + FWSTORE_DESC_SIZE, s_up.size) != s_up.crc)
  {
    return FWSTORE_E_CRC;
  }

  FwStore_Lock();
  ok = FwStore_WriteDesc(s_up.type, s_up.slot_idx, &v);
  if (ok && out != NULL) *out = s_slots[s_up.type][s_up.slot_idx];
  FwStore_Unlock();
  return ok ? FWSTORE_OK : FWSTORE_E_FLASH;
}

/* ---------- roles ---------- */

uint8_t FwStore_Promote(uint8_t type)
{
  uint8_t ok = 1u;
  int n = -1;
  int c = -1;

  if (type == 0u || type >= FWSTORE_TYPES) return 0u;
  FwStore_Lock();
  for (int i = 0; i < 2; i++)
  {
    if (s_slots[type][i].role == FWSTORE_ROLE_NEW) n = i;
    if (s_slots[type][i].role == FWSTORE_ROLE_CURRENT) c = i;
  }
  if (n < 0)
  {
    ok = 0u;
  }
  else
  {
    FwStoreSlot v = s_slots[type][n];

    v.role = FWSTORE_ROLE_CURRENT;
    ok = FwStore_WriteDesc(type, (uint8_t)n, &v);
    if (ok && c >= 0)
    {
      v = s_slots[type][c];
      v.role = FWSTORE_ROLE_BACKUP;
      ok = FwStore_WriteDesc(type, (uint8_t)c, &v);
    }
  }
  FwStore_Unlock();
  return ok;
}

uint8_t FwStore_Rollback(uint8_t type)
{
  uint8_t ok;
  int b = -1;
  int c = -1;

  if (type == 0u || type >= FWSTORE_TYPES) return 0u;
  FwStore_Lock();
  for (int i = 0; i < 2; i++)
  {
    if (s_slots[type][i].role == FWSTORE_ROLE_BACKUP) b = i;
    if (s_slots[type][i].role == FWSTORE_ROLE_CURRENT) c = i;
  }
  ok = (b >= 0) ? 1u : 0u;
  if (ok)
  {
    FwStoreSlot v = s_slots[type][b];

    /* Power loss between the two writes leaves two CURRENT slots; FwStore_Resolve keeps the newer one. */
    v.role = FWSTORE_ROLE_CURRENT;
    ok = FwStore_WriteDesc(type, (uint8_t)b, &v);
    if (ok && c >= 0)
    {
      v = s_slots[type][c];
      v.role = FWSTORE_ROLE_BACKUP;
      ok = FwStore_WriteDesc(type, (uint8_t)c, &v);
    }
  }
  FwStore_Unlock();
  return ok;
}
