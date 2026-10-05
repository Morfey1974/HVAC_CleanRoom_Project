/*
 * plc_w25q.c — W25Q128 SPI NOR flash on OCTOSPI1.
 */
#include "plc_w25q.h"

#include "octospi.h"
#include "cmsis_os2.h"

#define W25Q_CMD_WREN        0x06u
#define W25Q_CMD_RDSR1       0x05u
#define W25Q_CMD_READ        0x03u
#define W25Q_CMD_PP          0x02u
#define W25Q_CMD_SE4K        0x20u
#define W25Q_CMD_BE64K       0xD8u
#define W25Q_CMD_JEDEC       0x9Fu
#define W25Q_CMD_RESET_EN    0x66u
#define W25Q_CMD_RESET       0x99u
#define W25Q_CMD_RDSR2       0x35u
#define W25Q_CMD_WRSR2       0x31u

#define W25Q_SR2_QE          0x02u

#define W25Q_SR1_BUSY        0x01u
#define W25Q_SR1_WEL         0x02u

#define W25Q_T_CMD_MS        100u
#define W25Q_T_PP_MS         10u
#define W25Q_T_SE_MS         600u
#define W25Q_T_BE_MS         2500u

static osMutexId_t s_mtx;
static uint32_t s_jedec;

static void W25q_Cmd(OSPI_RegularCmdTypeDef *c, uint8_t ins, uint8_t with_addr, uint32_t addr, uint32_t ndata)
{
  c->OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  c->FlashId = HAL_OSPI_FLASH_ID_1;
  c->Instruction = ins;
  c->InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;
  c->InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  c->InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  c->Address = addr;
  c->AddressMode = with_addr ? HAL_OSPI_ADDRESS_1_LINE : HAL_OSPI_ADDRESS_NONE;
  c->AddressSize = HAL_OSPI_ADDRESS_24_BITS;
  c->AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  c->AlternateBytes = 0u;
  c->AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  c->AlternateBytesSize = HAL_OSPI_ALTERNATE_BYTES_8_BITS;
  c->AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
  c->DataMode = ndata ? HAL_OSPI_DATA_1_LINE : HAL_OSPI_DATA_NONE;
  c->NbData = ndata;
  c->DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  c->DummyCycles = 0u;
  c->DQSMode = HAL_OSPI_DQS_DISABLE;
  c->SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
}

static uint8_t W25q_Simple(uint8_t ins)
{
  OSPI_RegularCmdTypeDef c = {0};

  W25q_Cmd(&c, ins, 0u, 0u, 0u);
  return (HAL_OSPI_Command(&hospi1, &c, W25Q_T_CMD_MS) == HAL_OK) ? 1u : 0u;
}

static uint8_t W25q_ReadReg(uint8_t ins, uint8_t *buf, uint32_t n)
{
  OSPI_RegularCmdTypeDef c = {0};

  W25q_Cmd(&c, ins, 0u, 0u, n);
  if (HAL_OSPI_Command(&hospi1, &c, W25Q_T_CMD_MS) != HAL_OK) return 0u;
  return (HAL_OSPI_Receive(&hospi1, buf, W25Q_T_CMD_MS) == HAL_OK) ? 1u : 0u;
}

/* Waits until the chip is idle; long operations yield to other threads. */
static uint8_t W25q_WaitIdle(uint32_t timeout_ms, uint8_t yield)
{
  uint32_t t0 = osKernelGetTickCount();
  uint8_t sr;

  for (;;)
  {
    if (!W25q_ReadReg(W25Q_CMD_RDSR1, &sr, 1u)) return 0u;
    if ((sr & W25Q_SR1_BUSY) == 0u) return 1u;
    if ((osKernelGetTickCount() - t0) > timeout_ms) return 0u;
    if (yield) osDelay(1);
  }
}

static uint8_t W25q_WriteEnable(void)
{
  uint8_t sr;

  if (!W25q_Simple(W25Q_CMD_WREN)) return 0u;
  if (!W25q_ReadReg(W25Q_CMD_RDSR1, &sr, 1u)) return 0u;
  return (sr & W25Q_SR1_WEL) ? 1u : 0u;
}

/* Single-line commands leave IO2/IO3 undriven; with QE set the chip ignores /WP and /HOLD on them. */
static void W25q_EnableQuadPins(void)
{
  OSPI_RegularCmdTypeDef c = {0};
  uint8_t sr2;

  if (!W25q_ReadReg(W25Q_CMD_RDSR2, &sr2, 1u) || (sr2 & W25Q_SR2_QE)) return;
  sr2 |= W25Q_SR2_QE;
  if (!W25q_WriteEnable()) return;
  W25q_Cmd(&c, W25Q_CMD_WRSR2, 0u, 0u, 1u);
  if (HAL_OSPI_Command(&hospi1, &c, W25Q_T_CMD_MS) != HAL_OK) return;
  if (HAL_OSPI_Transmit(&hospi1, &sr2, W25Q_T_CMD_MS) != HAL_OK) return;
  (void)W25q_WaitIdle(W25Q_T_SE_MS, 1u);
}

static void W25q_Lock(void)
{
  if (s_mtx != NULL) osMutexAcquire(s_mtx, osWaitForever);
}

static void W25q_Unlock(void)
{
  if (s_mtx != NULL) osMutexRelease(s_mtx);
}

void W25q_Setup(void)
{
  const osMutexAttr_t a = { .name = "w25q", .attr_bits = osMutexPrioInherit };

  s_mtx = osMutexNew(&a);
}

uint8_t W25q_Probe(void)
{
  uint8_t id[3] = {0};
  uint8_t ok;

  W25q_Lock();
  (void)W25q_Simple(W25Q_CMD_RESET_EN);
  (void)W25q_Simple(W25Q_CMD_RESET);
  osDelay(1);
  ok = W25q_ReadReg(W25Q_CMD_JEDEC, id, 3u);
  if (ok) W25q_EnableQuadPins();
  W25q_Unlock();

  s_jedec = ok ? (((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2]) : 0u;
  /* id[2] = 0x18: 2^24 bytes. Manufacturer is not checked: compatible chips work the same. */
  return (ok && id[0] != 0x00u && id[0] != 0xFFu && id[2] == 0x18u) ? 1u : 0u;
}

uint32_t W25q_JedecId(void)
{
  return s_jedec;
}

uint8_t W25q_Read(uint32_t addr, void *buf, uint32_t len)
{
  OSPI_RegularCmdTypeDef c = {0};
  uint8_t ok;

  if (len == 0u) return 1u;
  if (addr + len > W25Q_SIZE) return 0u;

  W25q_Lock();
  W25q_Cmd(&c, W25Q_CMD_READ, 1u, addr, len);
  ok = (HAL_OSPI_Command(&hospi1, &c, W25Q_T_CMD_MS) == HAL_OK &&
        HAL_OSPI_Receive(&hospi1, (uint8_t *)buf, W25Q_T_CMD_MS + len / 64u) == HAL_OK) ? 1u : 0u;
  W25q_Unlock();
  return ok;
}

uint8_t W25q_Write(uint32_t addr, const void *buf, uint32_t len)
{
  const uint8_t *p = (const uint8_t *)buf;
  uint8_t ok = 1u;

  if (addr + len > W25Q_SIZE) return 0u;

  W25q_Lock();
  while (len > 0u && ok)
  {
    OSPI_RegularCmdTypeDef c = {0};
    uint32_t n = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE);

    if (n > len) n = len;
    ok = W25q_WriteEnable();
    if (ok)
    {
      W25q_Cmd(&c, W25Q_CMD_PP, 1u, addr, n);
      ok = (HAL_OSPI_Command(&hospi1, &c, W25Q_T_CMD_MS) == HAL_OK &&
            HAL_OSPI_Transmit(&hospi1, (uint8_t *)p, W25Q_T_CMD_MS) == HAL_OK) ? 1u : 0u;
    }
    if (ok) ok = W25q_WaitIdle(W25Q_T_PP_MS, 0u);
    addr += n;
    p += n;
    len -= n;
  }
  W25q_Unlock();
  return ok;
}

uint8_t W25q_Erase(uint32_t addr, uint32_t len)
{
  uint32_t end;
  uint8_t ok = 1u;

  if (len == 0u) return 1u;
  end = (addr + len + W25Q_SECTOR_SIZE - 1u) & ~(W25Q_SECTOR_SIZE - 1u);
  addr &= ~(W25Q_SECTOR_SIZE - 1u);
  if (end > W25Q_SIZE) return 0u;

  W25q_Lock();
  while (addr < end && ok)
  {
    OSPI_RegularCmdTypeDef c = {0};
    uint8_t block = ((addr % W25Q_BLOCK_SIZE) == 0u && (end - addr) >= W25Q_BLOCK_SIZE) ? 1u : 0u;

    ok = W25q_WriteEnable();
    if (ok)
    {
      W25q_Cmd(&c, block ? W25Q_CMD_BE64K : W25Q_CMD_SE4K, 1u, addr, 0u);
      ok = (HAL_OSPI_Command(&hospi1, &c, W25Q_T_CMD_MS) == HAL_OK) ? 1u : 0u;
    }
    if (ok) ok = W25q_WaitIdle(block ? W25Q_T_BE_MS : W25Q_T_SE_MS, 1u);
    addr += block ? W25Q_BLOCK_SIZE : W25Q_SECTOR_SIZE;
  }
  W25q_Unlock();
  return ok;
}
