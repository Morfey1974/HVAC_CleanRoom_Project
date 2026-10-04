#ifndef AHTXX_H
#define AHTXX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
  float temperature_c;
  float humidity_pct;
} Ahtxx_Data;

/* 0 = датчик найден и инициализирован */
uint8_t Ahtxx_Init(void);
/* 0 = ок; 2 = нет связи по шине */
uint8_t Ahtxx_Read(Ahtxx_Data *out);

#ifdef __cplusplus
}
#endif

#endif /* AHTXX_H */
