#ifndef SHT21_H
#define SHT21_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
  float temperature_c;
  float humidity_pct;
  uint16_t raw_h;
  uint16_t raw_t;
} Sht21_Data;

/* GY-21 / SHT21 / Si7021 / HTU21D, адрес 0x40 */
uint8_t Sht21_Init(void);
uint8_t Sht21_Read(Sht21_Data *out);

#ifdef __cplusplus
}
#endif

#endif /* SHT21_H */
