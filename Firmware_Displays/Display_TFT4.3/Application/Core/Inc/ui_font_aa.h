#ifndef UI_FONT_AA_H
#define UI_FONT_AA_H

#include <stdint.h>

typedef struct {
  uint32_t cp;
  uint16_t w;
  uint16_t h;
  const uint8_t *alpha;
} UiFontAa_Glyph;

#define UI_TITLE_W 394U
#define UI_TITLE_H 54U
extern const uint16_t UiTitle_CleanRoom[];

#define UI_LETTER_T_W 102U
#define UI_LETTER_T_H 116U
extern const uint8_t UiLetter_T[];
#define UI_LETTER_H_W 117U
#define UI_LETTER_H_H 116U
extern const uint8_t UiLetter_H[];
#define UI_LETTER_P_W 109U
#define UI_LETTER_P_H 116U
extern const uint8_t UiLetter_P[];

#define UI_VALUE_FONT_H 61U
const UiFontAa_Glyph *UiFontAa_Find(uint32_t cp);
uint16_t UiFontAa_TextWidth(const char *utf8);

#endif
