"""Antialiased menu/door glyphs for the 10.1 HMI."""
from PIL import Image, ImageDraw, ImageFont
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
INC = os.path.join(ROOT, "Core", "Inc")
SRC = os.path.join(ROOT, "Core", "Src")
SIZE = 26
PAD = 2

FONT_CANDIDATES = [
    r"C:\Windows\Fonts\segoeuib.ttf",
    r"C:\Windows\Fonts\arialbd.ttf",
    r"C:\Windows\Fonts\calibrib.ttf",
    r"C:\Windows\Fonts\segoeui.ttf",
    r"C:\Windows\Fonts\arial.ttf",
]
FONT_PATH = next(p for p in FONT_CANDIDATES if os.path.exists(p))
FONT = ImageFont.truetype(FONT_PATH, SIZE)

chars = []
for cp in range(0x20, 0x7F):
    chars.append(chr(cp))
chars.append("Ё")
chars.append("ё")
for cp in range(0x0410, 0x0450):
    chars.append(chr(cp))

seen = set()
uniq = []
for ch in chars:
    if ch not in seen:
        seen.add(ch)
        uniq.append(ch)
chars = uniq


def render(ch):
    if ch == " ":
        w = max(8, SIZE // 3)
        return Image.new("L", (w, SIZE + 2 * PAD), 0)
    tmp = Image.new("L", (1, 1), 0)
    d = ImageDraw.Draw(tmp)
    bbox = d.textbbox((0, 0), ch, font=FONT)
    tw = max(1, bbox[2] - bbox[0])
    th = max(1, bbox[3] - bbox[1])
    img = Image.new("L", (tw + 2 * PAD, th + 2 * PAD), 0)
    d = ImageDraw.Draw(img)
    d.text((PAD - bbox[0], PAD - bbox[1]), ch, font=FONT, fill=255)
    return img


glyphs = {ch: render(ch) for ch in chars}
vh = max(g.size[1] for g in glyphs.values())
for ch, g in list(glyphs.items()):
    if g.size[1] != vh:
        canvas = Image.new("L", (g.size[0], vh), 0)
        canvas.paste(g, (0, (vh - g.size[1]) // 2))
        glyphs[ch] = canvas

hdr = os.path.join(INC, "ui_font_menu.h")
src = os.path.join(SRC, "ui_font_menu.c")

with open(hdr, "w", encoding="utf-8") as f:
    f.write("#ifndef UI_FONT_MENU_H\n#define UI_FONT_MENU_H\n\n")
    f.write("#include <stdint.h>\n")
    f.write('#include "ui_font_aa.h"\n\n')
    f.write(f"#define UI_MENU_FONT_H {vh}U\n")
    f.write("const UiFontAa_Glyph *UiMenuFont_Find(uint32_t cp);\n")
    f.write("uint16_t UiMenuFont_TextWidth(const char *utf8);\n\n")
    f.write("#endif\n")

with open(src, "w", encoding="utf-8") as f:
    f.write('#include "ui_font_menu.h"\n\n')
    meta = []
    for ch in chars:
        img = glyphs[ch]
        w, h = img.size
        name = f"ui_menu_{ord(ch):04X}"
        data = list(img.getdata())
        f.write(f"static const uint8_t {name}[{w * h}] = {{\n")
        for i, a in enumerate(data):
            if i % 16 == 0:
                f.write("  ")
            f.write(f"0x{a:02X}U,")
            if i % 16 == 15:
                f.write("\n")
            else:
                f.write(" ")
        if len(data) % 16:
            f.write("\n")
        f.write("};\n\n")
        meta.append((ord(ch), w, h, name))

    f.write("static const UiFontAa_Glyph ui_menu_glyphs[] = {\n")
    for cp, w, h, name in sorted(meta, key=lambda t: t[0]):
        f.write(f"  {{ 0x{cp:04X}U, {w}U, {h}U, {name} }},\n")
    f.write("};\n\n")
    n = len(meta)
    f.write("const UiFontAa_Glyph *UiMenuFont_Find(uint32_t cp)\n{\n")
    f.write("  uint32_t lo = 0U;\n")
    f.write(f"  uint32_t hi = {n}U;\n")
    f.write("  while (lo < hi) {\n")
    f.write("    uint32_t mid = lo + ((hi - lo) / 2U);\n")
    f.write("    uint32_t v = ui_menu_glyphs[mid].cp;\n")
    f.write("    if (v == cp) return &ui_menu_glyphs[mid];\n")
    f.write("    if (v < cp) lo = mid + 1U;\n")
    f.write("    else hi = mid;\n")
    f.write("  }\n  return 0;\n}\n\n")
    f.write("static uint32_t menu_utf8_next(const char **text)\n{\n")
    f.write("  const uint8_t *p = (const uint8_t *)(*text);\n")
    f.write("  uint32_t cp;\n")
    f.write("  if (p[0] == 0U) return 0U;\n")
    f.write("  if (p[0] < 0x80U) { cp = p[0]; p += 1U; }\n")
    f.write("  else if ((p[0] & 0xE0U) == 0xC0U) {\n")
    f.write("    cp = ((uint32_t)(p[0] & 0x1FU) << 6) | (uint32_t)(p[1] & 0x3FU); p += 2U;\n")
    f.write("  } else if ((p[0] & 0xF0U) == 0xE0U) {\n")
    f.write("    cp = ((uint32_t)(p[0] & 0x0FU) << 12) | ((uint32_t)(p[1] & 0x3FU) << 6) | (uint32_t)(p[2] & 0x3FU); p += 3U;\n")
    f.write("  } else { cp = (uint32_t)'?'; p += 1U; }\n")
    f.write("  *text = (const char *)p;\n")
    f.write("  return cp;\n}\n\n")
    f.write("uint16_t UiMenuFont_TextWidth(const char *utf8)\n{\n")
    f.write("  uint16_t w = 0U;\n")
    f.write("  if (utf8 == 0) return 0U;\n")
    f.write("  while (*utf8) {\n")
    f.write("    uint32_t cp = menu_utf8_next(&utf8);\n")
    f.write("    const UiFontAa_Glyph *g;\n")
    f.write("    if (cp == 0U) break;\n")
    f.write("    g = UiMenuFont_Find(cp);\n")
    f.write("    if (g) w = (uint16_t)(w + g->w);\n")
    f.write("    else w = (uint16_t)(w + (UI_MENU_FONT_H / 3U));\n")
    f.write("  }\n  return w;\n}\n")

print("font", FONT_PATH)
print("H", vh, "glyphs", len(meta))
print("wrote", hdr)
print("wrote", src)
