/*
 * plc_rtc.h — external real-time clock Nyfea FRTC8900S (Epson RX8900 compatible) on I2C1.
 *
 * The clock keeps UTC. Its own task reads it twice a second; the time is set from the web server
 * (POST /api/rtc/set), the write is done by the same task so I2C1 has a single user.
 */
#ifndef PLC_RTC_H
#define PLC_RTC_H

#include <stdint.h>

typedef struct
{
  uint16_t year;   /* 2000..2099 */
  uint8_t  month;  /* 1..12 */
  uint8_t  day;    /* 1..31 */
  uint8_t  hour;   /* 0..23 */
  uint8_t  min;
  uint8_t  sec;
} PlcRtcTime;

typedef struct
{
  uint8_t    present;  /* chip answered the last read */
  uint8_t    valid;    /* time is trusted: set since the backup voltage was last lost */
  PlcRtcTime t;
  uint32_t   errors;   /* failed I2C transfers */
  uint32_t   sets;     /* successful time writes */
  uint32_t   last_err; /* HAL_I2C_ERROR_* of the last failed transfer (0x04 = no acknowledge) */
  uint8_t    scan_n;   /* devices that answered the bus scan at start */
  uint8_t    scan_first; /* 7-bit address of the first of them */
} PlcRtcSnapshot;

void PlcRtc_Start(void);
void PlcRtc_Get(PlcRtcSnapshot *out);
/* Queues a time write; 0 if the time is out of range. */
uint8_t PlcRtc_RequestSet(const PlcRtcTime *t);

#endif /* PLC_RTC_H */
