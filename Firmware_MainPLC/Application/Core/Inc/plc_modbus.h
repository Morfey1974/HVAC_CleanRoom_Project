/*
 * plc_modbus.h — Main PLC Modbus RTU master on RS-485 (UART4, DE on PA15).
 *
 * Polls the DCM door controller (SuperPharm, doc. 1.0):
 *   115200 8N1, slave 1, function 0x02, address 0, quantity 5, every 500 ms.
 *   Response timeout 300 ms; 3 failed polls in a row -> link lost.
 */
#ifndef PLC_MODBUS_H
#define PLC_MODBUS_H

#include <stdint.h>

typedef struct
{
  uint8_t  closed_mask; /* bit N = door N+1, 1 = closed; valid only while link = 1 */
  uint8_t  link;        /* 1: DCM answered within the last 3 polls */
  uint8_t  fail_row;    /* failed polls in a row */
  uint32_t tx;          /* requests sent */
  uint32_t ok;          /* valid responses */
  uint32_t timeout;     /* no response within 300 ms */
  uint32_t bad;         /* wrong length, address, function or CRC, UART errors */
  uint32_t exc;         /* Modbus exception responses (0x82) */
} PlcDoorsSnapshot;

/* Creates the doorsTask thread. Call from MX_FREERTOS_Init. */
void PlcModbus_Start(void);

/* Thread-safe copy of the doors state. */
void PlcModbus_GetDoors(PlcDoorsSnapshot *out);

#endif /* PLC_MODBUS_H */
