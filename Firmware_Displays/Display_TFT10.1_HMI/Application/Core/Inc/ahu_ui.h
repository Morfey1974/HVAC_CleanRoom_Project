#ifndef AHU_UI_H
#define AHU_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "room_ui.h"

#define AHU_SEC_NONE   0xFFU
#define AHU_SEC_INLET  0U
#define AHU_SEC_F1     1U
#define AHU_SEC_F2     2U
#define AHU_SEC_FAN    3U
#define AHU_SEC_COIL   4U
#define AHU_SEC_F95    5U
#define AHU_SEC_OUTLET 6U
#define AHU_SEC_COUNT  7U
#define AHU_UI_USE_SNAP 1

void AhuUi_Draw(const RoomUi_Values *values, uint32_t tick_ms);
void AhuUi_Update(const RoomUi_Values *values, uint32_t tick_ms);
uint8_t AhuUi_Hit(uint16_t x, uint16_t y);
void AhuUi_Select(uint8_t sec, const RoomUi_Values *values);

#ifdef __cplusplus
}
#endif

#endif /* AHU_UI_H */
