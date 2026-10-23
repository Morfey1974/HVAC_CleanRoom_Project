#ifndef UI_MENU_LABELS_H
#define UI_MENU_LABELS_H

#include <stdint.h>

typedef struct
{
  uint16_t w;
  uint16_t h;
  const uint16_t *rgb;
} HmiLbl;

#define HMI_LBL_H 39U
#define HMI_DIG_W 24U
#define HMI_DIG_H 40U

extern const HmiLbl HmiLbl_TitleMenu;
extern const HmiLbl HmiLbl_TitleSp;
extern const HmiLbl HmiLbl_TitleBright;
extern const HmiLbl HmiLbl_TitleSim;
extern const HmiLbl HmiLbl_TitleAbout;
extern const HmiLbl HmiLbl_BtnSp;
extern const HmiLbl HmiLbl_BtnBright;
extern const HmiLbl HmiLbl_BtnSim;
extern const HmiLbl HmiLbl_BtnAbout;
extern const HmiLbl HmiLbl_BtnClose;
extern const HmiLbl HmiLbl_BtnBack;
extern const HmiLbl HmiLbl_NameT;
extern const HmiLbl HmiLbl_NameH;
extern const HmiLbl HmiLbl_NameP;
extern const HmiLbl HmiLbl_BtnMinus;
extern const HmiLbl HmiLbl_BtnPlus;
extern const HmiLbl HmiLbl_AboutHvac;
extern const HmiLbl HmiLbl_AboutPanel;
extern const HmiLbl HmiLbl_AboutAuthor;
extern const HmiLbl HmiLbl_AboutName;
extern const HmiLbl HmiLbl_SimOn;
extern const HmiLbl HmiLbl_SimOff;
extern const HmiLbl HmiLbl_LinkOn;
extern const HmiLbl HmiLbl_LinkOff;
extern const HmiLbl HmiLbl_UnitC;
extern const HmiLbl HmiLbl_UnitPct;
extern const HmiLbl HmiLbl_UnitPa;
extern const HmiLbl HmiLbl_DoorOk1;
extern const HmiLbl HmiLbl_DoorNo1;
extern const HmiLbl HmiLbl_DoorOk2;
extern const HmiLbl HmiLbl_DoorNo2;
extern const HmiLbl HmiLbl_DoorOk3;
extern const HmiLbl HmiLbl_DoorNo3;
extern const HmiLbl HmiLbl_DoorOk4;
extern const HmiLbl HmiLbl_DoorNo4;
extern const HmiLbl HmiLbl_DoorOk5;
extern const HmiLbl HmiLbl_DoorNo5;
extern const uint16_t * const HmiDig_Btn[10];
extern const HmiLbl * const HmiLbl_DoorOk[5];
extern const HmiLbl * const HmiLbl_DoorNo[5];

#endif
