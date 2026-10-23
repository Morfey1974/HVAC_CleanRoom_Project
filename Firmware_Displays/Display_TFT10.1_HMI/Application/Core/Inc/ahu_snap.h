#ifndef AHU_SNAP_INCLUDED
#define AHU_SNAP_INCLUDED

#include <stdint.h>

#define AHU_SNAP_W 1024U
#define AHU_SNAP_HT 600U
#define AHU_SNAP_RLE_N 270000U

extern const uint8_t AhuSnap_Rle[AHU_SNAP_RLE_N];
void AhuSnap_Draw(void);
void AhuSnap_Tick(uint32_t tick_ms, uint8_t on);

#endif
