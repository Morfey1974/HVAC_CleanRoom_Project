#ifndef HMI_MENU_H
#define HMI_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "room_ui.h"

typedef struct
{
  RoomUi_Values *values;
  uint8_t *doors_mask;
  uint8_t *link;
  uint8_t *sim_on;
} HmiMenu_Ctx;

void HmiMenu_Init(HmiMenu_Ctx *ctx);
uint8_t HmiMenu_IsOpen(void);
void HmiMenu_Poll(uint8_t down, uint16_t x, uint16_t y);

#ifdef __cplusplus
}
#endif

#endif /* HMI_MENU_H */
