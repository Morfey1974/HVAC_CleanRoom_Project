/*
 * plc_modbus.c — Main PLC Modbus RTU master: polls the DCM door controller.
 */
#include "plc_modbus.h"

#include <string.h>
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "hvac_can.h"

#define MB_UART             (&huart4)
#define MB_SLAVE            1u
#define MB_FUNC_READ_DI     0x02u
#define MB_DOORS_START      0u
#define MB_POLL_MS          500u
#define MB_RESP_TMO_MS      300u
#define MB_TX_TMO_MS        20u
#define MB_FAILS_LINK_LOST  3u
#define MB_RX_BUF           32u

static const osThreadAttr_t s_task_attr = {
  .name = "doorsTask",
  .stack_size = 384 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

static osSemaphoreId_t s_rx_sem;
static uint8_t s_rx[MB_RX_BUF];
static volatile uint16_t s_rx_len;
static volatile uint8_t s_rx_err;
static PlcDoorsSnapshot s_doors;

typedef enum
{
  MB_RES_OK,
  MB_RES_EXC,
  MB_RES_TIMEOUT,
  MB_RES_BAD
} MbResult;

static uint16_t Mb_Crc16(const uint8_t *p, uint32_t n)
{
  uint16_t crc = 0xFFFFu;

  while (n-- > 0u)
  {
    crc ^= *p++;
    for (uint32_t i = 0u; i < 8u; i++)
    {
      crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
    }
  }
  return crc;
}

static uint8_t Mb_CrcOk(const uint8_t *p, uint32_t n)
{
  uint16_t crc;

  if (n < 3u) return 0u;
  crc = Mb_Crc16(p, n - 2u);
  return (p[n - 2u] == (uint8_t)(crc & 0xFFu) && p[n - 1u] == (uint8_t)(crc >> 8)) ? 1u : 0u;
}

/* RO of the transceiver floats while DE/RE is high: drop what came in during TX. */
static void Mb_FlushRx(void)
{
  __HAL_UART_CLEAR_FLAG(MB_UART, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF |
                                 UART_CLEAR_PEF | UART_CLEAR_IDLEF);
  __HAL_UART_SEND_REQ(MB_UART, UART_RXDATA_FLUSH_REQUEST);
}

static MbResult Mb_ReadDoors(uint8_t *closed_mask)
{
  uint8_t req[8];
  uint16_t crc;
  uint16_t n;

  req[0] = MB_SLAVE;
  req[1] = MB_FUNC_READ_DI;
  req[2] = (uint8_t)(MB_DOORS_START >> 8);
  req[3] = (uint8_t)(MB_DOORS_START & 0xFFu);
  req[4] = 0u;
  req[5] = HVAC_DOORS_COUNT;
  crc = Mb_Crc16(req, 6u);
  req[6] = (uint8_t)(crc & 0xFFu);
  req[7] = (uint8_t)(crc >> 8);

  HAL_UART_AbortReceive(MB_UART);
  while (osSemaphoreAcquire(s_rx_sem, 0u) == osOK) {}
  s_rx_len = 0u;
  s_rx_err = 0u;

  if (HAL_UART_Transmit(MB_UART, req, sizeof(req), MB_TX_TMO_MS) != HAL_OK) return MB_RES_BAD;

  Mb_FlushRx();
  if (HAL_UARTEx_ReceiveToIdle_IT(MB_UART, s_rx, sizeof(s_rx)) != HAL_OK) return MB_RES_BAD;

  if (osSemaphoreAcquire(s_rx_sem, MB_RESP_TMO_MS) != osOK)
  {
    HAL_UART_AbortReceive(MB_UART);
    return MB_RES_TIMEOUT;
  }
  HAL_UART_AbortReceive(MB_UART);

  n = s_rx_len;
  if (s_rx_err || n < 5u || s_rx[0] != MB_SLAVE || !Mb_CrcOk(s_rx, n)) return MB_RES_BAD;
  if (n == 5u && s_rx[1] == (MB_FUNC_READ_DI | 0x80u)) return MB_RES_EXC;
  if (n != 6u || s_rx[1] != MB_FUNC_READ_DI || s_rx[2] != 1u) return MB_RES_BAD;

  *closed_mask = (uint8_t)(s_rx[3] & HVAC_DOORS_MASK);
  return MB_RES_OK;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart != MB_UART) return;
  s_rx_len = Size;
  osSemaphoreRelease(s_rx_sem);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart != MB_UART) return;
  s_rx_err = 1u;
  osSemaphoreRelease(s_rx_sem);
}

static void PlcModbus_Task(void *argument)
{
  uint32_t next;

  (void)argument;
  next = osKernelGetTickCount();

  for (;;)
  {
    uint8_t mask = 0u;
    MbResult r = Mb_ReadDoors(&mask);

    taskENTER_CRITICAL();
    s_doors.tx++;
    switch (r)
    {
      case MB_RES_OK:
        s_doors.ok++;
        s_doors.closed_mask = mask;
        s_doors.fail_row = 0u;
        s_doors.link = 1u;
        break;
      case MB_RES_EXC:
        s_doors.exc++;
        s_doors.fail_row = 0u;
        break;
      case MB_RES_TIMEOUT:
        s_doors.timeout++;
        break;
      default:
        s_doors.bad++;
        break;
    }
    if (r == MB_RES_TIMEOUT || r == MB_RES_BAD)
    {
      if (s_doors.fail_row < 255u) s_doors.fail_row++;
      if (s_doors.fail_row >= MB_FAILS_LINK_LOST) s_doors.link = 0u;
    }
    taskEXIT_CRITICAL();

    next += MB_POLL_MS;
    if ((int32_t)(next - osKernelGetTickCount()) <= 0) next = osKernelGetTickCount();
    else osDelayUntil(next);
  }
}

void PlcModbus_Start(void)
{
  memset(&s_doors, 0, sizeof(s_doors));
  s_rx_sem = osSemaphoreNew(1u, 0u, NULL);
  if (s_rx_sem != NULL) osThreadNew(PlcModbus_Task, NULL, &s_task_attr);
}

void PlcModbus_GetDoors(PlcDoorsSnapshot *out)
{
  taskENTER_CRITICAL();
  *out = s_doors;
  taskEXIT_CRITICAL();
}
