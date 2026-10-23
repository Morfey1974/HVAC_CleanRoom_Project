#ifndef ROOM_UI_H
#define ROOM_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ROOM_UI_FAULT_T  0x01u
#define ROOM_UI_FAULT_H  0x02u
#define ROOM_UI_FAULT_P  0x04u

typedef struct
{
  float temperature_c;
  float humidity_pct;
  float pressure_pa;
  float t_set;
  float fan_speed;
  uint8_t fault_mask;   /* ROOM_UI_FAULT_*: value shown as "--" on red */
} RoomUi_Values;

void RoomUi_Init(const char *room_name);
void RoomUi_Update(const RoomUi_Values *values);
/* Doors block in the header: bit N of closed_mask = door N+1 closed; link = 0 -> grey, "НЕТ СВЯЗИ". */
void RoomUi_UpdateDoors(uint8_t closed_mask, uint8_t link);
void RoomUi_SimulateStep(RoomUi_Values *values, uint32_t tick_ms);
void RoomUi_LogoRect(uint16_t *x0, uint16_t *y0, uint16_t *x1, uint16_t *y1);
void RoomUi_Invalidate(void);
void RoomUi_RedrawAll(const RoomUi_Values *values, uint8_t closed_mask, uint8_t link);

#ifdef __cplusplus
}
#endif

#endif /* ROOM_UI_H */
