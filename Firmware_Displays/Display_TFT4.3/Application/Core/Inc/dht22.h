#ifndef DHT22_H
#define DHT22_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
  float temperature_c;
  float humidity_pct;
  uint8_t raw[5]; /* сырые байты для отладки */
} Dht22_Data;

void Dht22_Init(void);
/* 0 = ок, иначе ошибка. Между опросами ≥ 2 с. */
uint8_t Dht22_Read(Dht22_Data *out);

#ifdef __cplusplus
}
#endif

#endif /* DHT22_H */
