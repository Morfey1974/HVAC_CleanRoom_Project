#ifndef AHU_MOTION_H
#define AHU_MOTION_H

#include <stdint.h>

#define FLOW_L_X 48U
#define FLOW_L_Y 258U
#define FLOW_L_W 96U
#define FLOW_L_H 48U
#define FLOW_R_X 912U
#define FLOW_R_Y 258U
#define FLOW_R_W 96U
#define FLOW_R_H 48U
#define FAN_X 544U
#define FAN_Y 279U
#define FAN_S 64U
#define FAN_CX 32
#define FAN_CY 32
#define FAN_R 22
#define FLOW_L_N 4608U
#define FLOW_R_N 4608U
#define FAN_N 4096U

extern const uint16_t AhuFlow_LeftBg[FLOW_L_N];
extern const uint16_t AhuFlow_RightBg[FLOW_R_N];
extern const uint16_t AhuFan_Bg[FAN_N];

#endif
