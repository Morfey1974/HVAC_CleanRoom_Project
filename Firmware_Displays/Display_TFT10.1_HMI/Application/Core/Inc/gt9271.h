#ifndef GT9271_H
#define GT9271_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint8_t GT9271_Init(void);
uint8_t GT9271_Poll(uint16_t *x, uint16_t *y);

#ifdef __cplusplus
}
#endif

#endif /* GT9271_H */
