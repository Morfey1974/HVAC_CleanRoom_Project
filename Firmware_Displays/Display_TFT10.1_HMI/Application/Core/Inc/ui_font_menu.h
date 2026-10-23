#ifndef UI_FONT_MENU_H
#define UI_FONT_MENU_H

#include <stdint.h>
#include "ui_font_aa.h"

#define UI_MENU_FONT_H 30U
const UiFontAa_Glyph *UiMenuFont_Find(uint32_t cp);
uint16_t UiMenuFont_TextWidth(const char *utf8);

#endif
