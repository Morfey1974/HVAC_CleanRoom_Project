/*
 * boot_core.c — HVAC CAN bootloader core for STM32H7 (single bank, 128 KB sectors, 32-byte flash words).
 */
#include "boot_core.h"

#include <string.h>
#include "boot_config.h"
#include "fwupd/fwupd_proto.h"

#define BOOT_WINDOW_MS        200u /* listen for CONNECT before starting a valid application */
#define BOOT_MAX_FAILS        3u
#define BOOT_META_MAGIC       0x4154454Du /* "META" */
#define BOOT_ERASED32         0xFFFFFFFFu
#define BOOT_FW               32u         /* flash word, bytes */

typedef struct
{
  uint32_t magic;
  uint32_t size;
  uint32_t crc;
  uint32_t version;
  uint32_t check;     /* ~(magic ^ size ^ crc ^ version) */
  uint32_t pad[3];
} BootMeta;           /* one flash word */

extern FwupdShared _fwupd_shared;

static FDCAN_HandleTypeDef *s_can;
static uint32_t s_tag;
static uint8_t s_state;
static uint8_t s_fails;
static uint16_t s_app_version;

static uint32_t s_erased_size;
static uint32_t s_blk_off;
static uint32_t s_blk_len;
static uint32_t s_blk_fill;
static uint16_t s_blk_crc;
static uint8_t s_blk_active;
static uint8_t s_blk[FWUPD_BLOCK_MAX] __attribute__((aligned(32)));
static uint8_t s_first_fw[BOOT_FW] __attribute__((aligned(32)));
static uint8_t s_first_fw_ok;

/* ---------- CAN ---------- */

static void Boot_Send(uint32_t type, const uint8_t d[8])
{
  FDCAN_TxHeaderTypeDef h = {0};
  uint32_t t0 = HAL_GetTick();

  h.Identifier = FWUPD_ID(type, s_tag);
  h.IdType = FDCAN_EXTENDED_ID;
  h.TxFrameType = FDCAN_DATA_FRAME;
  h.DataLength = FDCAN_DLC_BYTES_8;
  h.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  h.BitRateSwitch = FDCAN_BRS_OFF;
  h.FDFormat = FDCAN_CLASSIC_CAN;
  h.TxEventFifoControl = FDCAN_NO_TX_EVENTS;

  while (HAL_FDCAN_GetTxFifoFreeLevel(s_can) == 0u)
  {
    if ((HAL_GetTick() - t0) > 50u) return;
  }
  HAL_FDCAN_AddMessageToTxFifoQ(s_can, &h, (uint8_t *)d);
}

static void Boot_Announce(void)
{
  uint8_t d[8];

  d[0] = BOOT_MODULE_TYPE;
  d[1] = BOOT_BOARD_REV;
  d[2] = s_state;
  d[3] = BOOT_VERSION;
  d[4] = (uint8_t)s_app_version;
  d[5] = (uint8_t)(s_app_version >> 8);
  d[6] = s_fails;
  d[7] = 0u;
  Boot_Send(FWUPD_MSG_ANNOUNCE, d);
}

static void Boot_Ack(uint8_t cmd, uint8_t err, uint32_t param)
{
  uint8_t d[8] = {0};

  d[0] = cmd;
  d[1] = err;
  fwupd_put32(&d[2], param);
  Boot_Send(FWUPD_MSG_ACK, d);
}

static void Boot_CanStart(void)
{
  HAL_FDCAN_ConfigGlobalFilter(s_can, FDCAN_REJECT, FDCAN_ACCEPT_IN_RX_FIFO0,
                               FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
  HAL_FDCAN_Start(s_can);
}

static void Boot_CanRecoverBusOff(void)
{
  FDCAN_ProtocolStatusTypeDef ps;

  if (HAL_FDCAN_GetProtocolStatus(s_can, &ps) == HAL_OK && ps.BusOff)
  {
    CLEAR_BIT(s_can->Instance->CCCR, FDCAN_CCCR_INIT);
  }
}

/* ---------- flash ---------- */

static uint8_t Boot_EraseSectors(uint32_t addr, uint32_t size)
{
  FLASH_EraseInitTypeDef e = {0};
  uint32_t sector_err = 0u;
  HAL_StatusTypeDef st;

  if (size == 0u) return 1u;
  e.TypeErase = FLASH_TYPEERASE_SECTORS;
  e.Banks = FLASH_BANK_1;
  e.Sector = (addr - FLASH_BANK1_BASE) / FLASH_SECTOR_SIZE;
  e.NbSectors = (size + FLASH_SECTOR_SIZE - 1u) / FLASH_SECTOR_SIZE;
#if defined(FLASH_CR_PSIZE)
  e.VoltageRange = FLASH_VOLTAGE_RANGE_3;
#endif

  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG_BANK1(FLASH_FLAG_ALL_ERRORS_BANK1);
  st = HAL_FLASHEx_Erase(&e, &sector_err);
  HAL_FLASH_Lock();
  return (st == HAL_OK) ? 1u : 0u;
}

/* addr is 32-byte aligned; a word that already holds these bytes (block resent) is skipped. */
static uint8_t Boot_ProgramFw(uint32_t addr, const uint8_t *src)
{
  HAL_StatusTypeDef st;

  if (memcmp((const void *)addr, src, BOOT_FW) == 0) return 1u;
  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG_BANK1(FLASH_FLAG_ALL_ERRORS_BANK1);
  st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, addr, (uint32_t)src);
  HAL_FLASH_Lock();
  return (st == HAL_OK && memcmp((const void *)addr, src, BOOT_FW) == 0) ? 1u : 0u;
}

/* ---------- image checks ---------- */

static const FwupdHeader *Boot_Header(void)
{
  return (const FwupdHeader *)(BOOT_APP_START + FWUPD_HEADER_OFFSET);
}

static uint8_t Boot_HeaderOk(const FwupdHeader *h)
{
  return (h->magic == FWUPD_HEADER_MAGIC && h->module_type == BOOT_MODULE_TYPE &&
          h->board_rev == BOOT_BOARD_REV) ? 1u : 0u;
}

static uint8_t Boot_VectorsOk(uint32_t sp, uint32_t pc)
{
  uint8_t sp_ok = ((sp > D1_DTCMRAM_BASE && sp <= (uint32_t)&_fwupd_shared) ||
                   (sp > D1_AXISRAM_BASE && sp <= D1_AXISRAM_BASE + 0x50000u)) ? 1u : 0u;

  return (sp_ok && (sp & 3u) == 0u && pc >= BOOT_APP_START && pc < BOOT_APP_END) ? 1u : 0u;
}

static uint32_t Boot_MetaCheck(const BootMeta *m)
{
  return ~(m->magic ^ m->size ^ m->crc ^ m->version);
}

static uint8_t Boot_WriteMeta(uint32_t size, uint32_t crc, uint32_t version)
{
  BootMeta m __attribute__((aligned(32)));

  memset(&m, 0xFF, sizeof(m));
  m.magic = BOOT_META_MAGIC;
  m.size = size;
  m.crc = crc;
  m.version = version;
  m.check = Boot_MetaCheck(&m);
  return Boot_ProgramFw(BOOT_META_ADDR, (const uint8_t *)&m);
}

/* Valid = vectors and header fit this module, metadata present and CRC matches.
 * Metadata erased and application flashed by ST-LINK: CRC is computed and metadata written once.
 * During an update the first flash word is programmed only after VERIFY, so a half-written
 * image never has valid vectors and is never adopted. */
static uint8_t Boot_AppValid(void)
{
  const uint32_t *vec = (const uint32_t *)BOOT_APP_START;
  const FwupdHeader *h = Boot_Header();
  const BootMeta *m = (const BootMeta *)BOOT_META_ADDR;
  uint32_t size;

  s_app_version = 0u;
  if (!Boot_VectorsOk(vec[0], vec[1]) || !Boot_HeaderOk(h)) return 0u;

  if (m->magic == BOOT_META_MAGIC && m->check == Boot_MetaCheck(m))
  {
    if (m->size < FWUPD_HEADER_OFFSET + sizeof(FwupdHeader) || m->size > (BOOT_APP_END - BOOT_APP_START)) return 0u;
    if (fwupd_crc32((const uint8_t *)BOOT_APP_START, m->size) != m->crc) return 0u;
  }
  else if (m->magic == BOOT_ERASED32)
  {
    if (h->image_end <= BOOT_APP_START + FWUPD_HEADER_OFFSET || h->image_end > BOOT_APP_END) return 0u;
    size = h->image_end - BOOT_APP_START;
    if (!Boot_WriteMeta(size, fwupd_crc32((const uint8_t *)BOOT_APP_START, size), h->fw_version)) return 0u;
  }
  else
  {
    return 0u;
  }

  s_app_version = h->fw_version;
  return 1u;
}

/* ---------- shared RAM ---------- */

static void Boot_SharedWrite(uint32_t request, uint32_t fails)
{
  FwupdShared *sh = &_fwupd_shared;

  sh->magic = FWUPD_SHARED_MAGIC;
  sh->request = request;
  sh->boot_fails = fails;
  sh->check = fwupd_shared_check(sh);
}

/* ---------- start application ---------- */

static void Boot_StartApp(void)
{
  const uint32_t *vec = (const uint32_t *)BOOT_APP_START;
  void (*entry)(void) = (void (*)(void))vec[1];

  Boot_SharedWrite(FWUPD_REQ_NONE, (uint32_t)s_fails + 1u);

  HAL_FDCAN_Stop(s_can);
  HAL_FDCAN_DeInit(s_can);
  HAL_RCC_DeInit();
  HAL_DeInit();

  __disable_irq();
  SysTick->CTRL = 0u;
  SysTick->LOAD = 0u;
  SysTick->VAL = 0u;
  for (uint32_t i = 0u; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFFu;
    NVIC->ICPR[i] = 0xFFFFFFFFu;
  }

  SCB->VTOR = BOOT_APP_VTOR;
  __DSB();
  __ISB();
  __set_MSP(vec[0]);
  __enable_irq();
  entry();
  for (;;) {}
}

/* ---------- commands ---------- */

static void Boot_OnErase(const uint8_t *d)
{
  uint32_t size = fwupd_get32(d);

  s_blk_active = 0u;
  s_first_fw_ok = 0u;
  s_erased_size = 0u;
  if (size < FWUPD_HEADER_OFFSET + sizeof(FwupdHeader) || size > (BOOT_APP_END - BOOT_APP_START))
  {
    Boot_Ack(FWUPD_MSG_ERASE, FWUPD_ERR_RANGE, size);
    return;
  }

  s_state = FWUPD_ST_BOOT_UPDATING;
  s_app_version = 0u;
  if (!Boot_EraseSectors(BOOT_META_ADDR, FLASH_SECTOR_SIZE) || !Boot_EraseSectors(BOOT_APP_START, size))
  {
    Boot_Ack(FWUPD_MSG_ERASE, FWUPD_ERR_FLASH, size);
    return;
  }
  s_erased_size = (size + FLASH_SECTOR_SIZE - 1u) / FLASH_SECTOR_SIZE * FLASH_SECTOR_SIZE;
  Boot_Ack(FWUPD_MSG_ERASE, FWUPD_ERR_OK, size);
}

static void Boot_OnBlock(const uint8_t *d)
{
  uint32_t off = fwupd_get32(&d[0]);
  uint32_t len = fwupd_get16(&d[4]);

  s_blk_active = 0u;
  if (s_erased_size == 0u)
  {
    Boot_Ack(FWUPD_MSG_BLOCK, FWUPD_ERR_STATE, off);
    return;
  }
  /* Blocks start on a flash word; only the last block of the image may be shorter than a flash word multiple. */
  if ((off % BOOT_FW) || (len & 7u) || len == 0u || len > FWUPD_BLOCK_MAX || off + len > s_erased_size)
  {
    Boot_Ack(FWUPD_MSG_BLOCK, FWUPD_ERR_RANGE, off);
    return;
  }
  s_blk_off = off;
  s_blk_len = len;
  s_blk_crc = fwupd_get16(&d[6]);
  s_blk_fill = 0u;
  s_blk_active = 1u;
}

static void Boot_OnData(const uint8_t *d)
{
  uint32_t padded;

  if (!s_blk_active) return; /* stray DATA after a failed BLOCK: the block ACK reports it */

  memcpy(&s_blk[s_blk_fill], d, 8u);
  s_blk_fill += 8u;
  if (s_blk_fill < s_blk_len) return;

  s_blk_active = 0u;
  if (fwupd_crc16(s_blk, s_blk_len) != s_blk_crc)
  {
    Boot_Ack(FWUPD_MSG_BLOCK, FWUPD_ERR_CRC, s_blk_off);
    return;
  }

  padded = (s_blk_len + BOOT_FW - 1u) / BOOT_FW * BOOT_FW;
  memset(&s_blk[s_blk_len], 0xFF, padded - s_blk_len);

  for (uint32_t i = 0u; i < padded; i += BOOT_FW)
  {
    uint32_t addr = BOOT_APP_START + s_blk_off + i;

    if (addr == BOOT_APP_START)
    {
      memcpy(s_first_fw, &s_blk[i], BOOT_FW);
      s_first_fw_ok = 1u;
      continue;
    }
    if (!Boot_ProgramFw(addr, &s_blk[i]))
    {
      Boot_Ack(FWUPD_MSG_BLOCK, FWUPD_ERR_FLASH, s_blk_off);
      return;
    }
  }
  Boot_Ack(FWUPD_MSG_BLOCK, FWUPD_ERR_OK, s_blk_off);
}

static void Boot_OnVerify(const uint8_t *d)
{
  uint32_t size = fwupd_get32(&d[0]);
  uint32_t crc = fwupd_get32(&d[4]);
  const FwupdHeader *h = Boot_Header();
  const uint32_t *v = (const uint32_t *)s_first_fw;
  uint32_t calc;

  if (s_erased_size == 0u || !s_first_fw_ok || size < FWUPD_HEADER_OFFSET + sizeof(FwupdHeader) ||
      size > s_erased_size)
  {
    Boot_Ack(FWUPD_MSG_VERIFY, FWUPD_ERR_STATE, size);
    return;
  }

  calc = fwupd_crc32_update(0u, s_first_fw, BOOT_FW);
  calc = fwupd_crc32_update(calc, (const uint8_t *)(BOOT_APP_START + BOOT_FW), size - BOOT_FW);
  if (calc != crc)
  {
    Boot_Ack(FWUPD_MSG_VERIFY, FWUPD_ERR_CRC, calc);
    return;
  }
  if (!Boot_HeaderOk(h) || !Boot_VectorsOk(v[0], v[1]))
  {
    Boot_Ack(FWUPD_MSG_VERIFY, FWUPD_ERR_HEADER, size);
    return;
  }
  if (!Boot_ProgramFw(BOOT_APP_START, s_first_fw) || !Boot_WriteMeta(size, crc, h->fw_version))
  {
    Boot_Ack(FWUPD_MSG_VERIFY, FWUPD_ERR_FLASH, size);
    return;
  }

  s_erased_size = 0u;
  s_fails = 0u;
  s_app_version = h->fw_version;
  s_state = FWUPD_ST_BOOT_WAIT;
  Boot_Ack(FWUPD_MSG_VERIFY, FWUPD_ERR_OK, size);
}

static void Boot_Handle(uint32_t id, const uint8_t *d, uint32_t len)
{
  uint32_t node = FWUPD_ID_NODE(id);
  uint32_t type = FWUPD_ID_TYPE(id);

  if (node == FWUPD_NODE_ALL)
  {
    if (type == FWUPD_MSG_DISCOVER) Boot_Announce();
    return;
  }
  if (node != s_tag || len < 8u) return;

  switch (type)
  {
    case FWUPD_MSG_DISCOVER:
      Boot_Announce();
      break;
    case FWUPD_MSG_CONNECT:
      if (fwupd_get32(&d[0]) == FWUPD_CONNECT_MAGIC && fwupd_get32(&d[4]) == FWUPD_CONNECT_KEY(s_tag))
      {
        if (s_state != FWUPD_ST_BOOT_EMPTY && s_state != FWUPD_ST_BOOT_APP_FAILED) s_state = FWUPD_ST_BOOT_WAIT;
        Boot_Announce();
      }
      break;
    case FWUPD_MSG_ERASE:
      Boot_OnErase(d);
      break;
    case FWUPD_MSG_BLOCK:
      Boot_OnBlock(d);
      break;
    case FWUPD_MSG_DATA:
      Boot_OnData(d);
      break;
    case FWUPD_MSG_VERIFY:
      Boot_OnVerify(d);
      break;
    case FWUPD_MSG_START:
      if (Boot_AppValid())
      {
        Boot_Ack(FWUPD_MSG_START, FWUPD_ERR_OK, s_app_version);
        HAL_Delay(5);
        s_fails = 0u;
        Boot_StartApp();
      }
      Boot_Ack(FWUPD_MSG_START, FWUPD_ERR_STATE, 0u);
      break;
    default:
      break;
  }
}

/* ---------- main ---------- */

void Boot_Run(FDCAN_HandleTypeDef *hfdcan)
{
  const uint32_t uid[3] = { HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2() };
  FwupdShared *sh = &_fwupd_shared;
  uint8_t stay = 0u;
  uint32_t t_start;
  uint32_t t_announce;

  s_can = hfdcan;
  s_tag = fwupd_node_tag(uid);

  if (sh->magic == FWUPD_SHARED_MAGIC && sh->check == fwupd_shared_check(sh))
  {
    stay = (sh->request == FWUPD_REQ_STAY_IN_BOOT) ? 1u : 0u;
    s_fails = (sh->boot_fails > 255u) ? 255u : (uint8_t)sh->boot_fails;
  }
  Boot_SharedWrite(FWUPD_REQ_NONE, s_fails);

  if (!Boot_AppValid())             s_state = FWUPD_ST_BOOT_EMPTY;
  else if (stay)                    s_state = FWUPD_ST_BOOT_WAIT;
  else if (s_fails >= BOOT_MAX_FAILS) s_state = FWUPD_ST_BOOT_APP_FAILED;
  else                              s_state = FWUPD_ST_APP_RUNNING; /* start window */

  Boot_CanStart();
  t_start = HAL_GetTick();
  t_announce = t_start - FWUPD_ANNOUNCE_BOOT_MS;

  for (;;)
  {
    FDCAN_RxHeaderTypeDef rh;
    uint8_t d[8];
    uint32_t now;

    while (HAL_FDCAN_GetRxFifoFillLevel(s_can, FDCAN_RX_FIFO0) > 0u)
    {
      if (HAL_FDCAN_GetRxMessage(s_can, FDCAN_RX_FIFO0, &rh, d) != HAL_OK) break;
      if (rh.IdType != FDCAN_EXTENDED_ID || rh.RxFrameType != FDCAN_DATA_FRAME) continue;
      if (s_state == FWUPD_ST_APP_RUNNING && FWUPD_ID_TYPE(rh.Identifier) != FWUPD_MSG_CONNECT) continue;
      Boot_Handle(rh.Identifier, d, (rh.DataLength == FDCAN_DLC_BYTES_8) ? 8u : 0u);
    }

    now = HAL_GetTick();
    if (s_state == FWUPD_ST_APP_RUNNING)
    {
      if ((now - t_start) >= BOOT_WINDOW_MS) Boot_StartApp();
      continue;
    }

    if ((now - t_announce) >= FWUPD_ANNOUNCE_BOOT_MS)
    {
      t_announce = now;
      Boot_Announce();
    }
    Boot_CanRecoverBusOff();
  }
}
