"""Generate antialiased UI glyphs for Clean Room screen."""
from PIL import Image, ImageDraw, ImageFont
import os

ROOT = r"C:\Users\morfe\STM32CubeIDE\workspace_1.17.0\STM32H723ZGT6 with TFT5 SSD1963"
INC = os.path.join(ROOT, "Core", "Inc")
SRC = os.path.join(ROOT, "Core", "Src")

FONT_CANDIDATES = [
    r"C:\Windows\Fonts\arialbd.ttf",
    r"C:\Windows\Fonts\segoeuib.ttf",
    r"C:\Windows\Fonts\calibrib.ttf",
    r"C:\Windows\Fonts\arial.ttf",
]
FONT_PATH = next(p for p in FONT_CANDIDATES if os.path.exists(p))


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def render_text_rgba(text, font_size, pad=4):
    font = ImageFont.truetype(FONT_PATH, font_size)
    tmp = Image.new("L", (1, 1), 0)
    d = ImageDraw.Draw(tmp)
    bbox = d.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    w, h = tw + 2 * pad, th + 2 * pad
    img = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(img)
    d.text((pad - bbox[0], pad - bbox[1]), text, font=font, fill=255)
    return img


def write_alpha_array(f, name, img):
    w, h = img.size
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
    return w, h


def write_rgb565_array(f, name, img_rgb):
    w, h = img_rgb.size
    f.write(f"const uint16_t {name}[{w * h}] = {{\n")
    i = 0
    for y in range(h):
        for x in range(w):
            r, g, b = img_rgb.getpixel((x, y))
            c = rgb565(r, g, b)
            if i % 12 == 0:
                f.write("  ")
            f.write(f"0x{c:04X}U,")
            if i % 12 == 11:
                f.write("\n")
            else:
                f.write(" ")
            i += 1
    if i % 12:
        f.write("\n")
    f.write("};\n\n")
    return w, h


# --- Title on header background ---
HEADER = (48, 48, 48)
title_alpha = render_text_rgba("Clean Room", 36, pad=6)
tw, th = title_alpha.size
title_rgb = Image.new("RGB", (tw, th), HEADER)
for y in range(th):
    for x in range(tw):
        a = title_alpha.getpixel((x, y)) / 255.0
        if a > 0:
            r = int(HEADER[0] + (255 - HEADER[0]) * a)
            g = int(HEADER[1] + (255 - HEADER[1]) * a)
            b = int(HEADER[2] + (255 - HEADER[2]) * a)
            title_rgb.putpixel((x, y), (r, g, b))

# --- Big letters T H P as alpha ---
letters = {}
for ch in ("T", "H", "P"):
    letters[ch] = render_text_rgba(ch, 140, pad=8)

# --- Value font glyphs ---
value_chars = list("0123456789 C%Pa°")
# unique preserving order
seen = set()
value_chars = [c for c in value_chars if not (c in seen or seen.add(c))]
glyphs = {}
for ch in value_chars:
    glyphs[ch] = render_text_rgba(ch if ch != " " else " ", 72, pad=3)
# normalize height to max
vh = max(g.size[1] for g in glyphs.values())
for ch, g in list(glyphs.items()):
    if g.size[1] != vh:
        canvas = Image.new("L", (g.size[0], vh), 0)
        canvas.paste(g, (0, (vh - g.size[1]) // 2))
        glyphs[ch] = canvas

# Write header
hdr_path = os.path.join(INC, "ui_font_aa.h")
with open(hdr_path, "w", encoding="utf-8") as f:
    f.write("#ifndef UI_FONT_AA_H\n#define UI_FONT_AA_H\n\n")
    f.write("#include <stdint.h>\n\n")
    f.write("typedef struct {\n")
    f.write("  uint32_t cp;\n")
    f.write("  uint16_t w;\n")
    f.write("  uint16_t h;\n")
    f.write("  const uint8_t *alpha;\n")
    f.write("} UiFontAa_Glyph;\n\n")
    f.write(f"#define UI_TITLE_W {tw}U\n")
    f.write(f"#define UI_TITLE_H {th}U\n")
    f.write("extern const uint16_t UiTitle_CleanRoom[];\n\n")
    for ch in ("T", "H", "P"):
        w, h = letters[ch].size
        f.write(f"#define UI_LETTER_{ch}_W {w}U\n")
        f.write(f"#define UI_LETTER_{ch}_H {h}U\n")
        f.write(f"extern const uint8_t UiLetter_{ch}[];\n")
    f.write("\n")
    f.write(f"#define UI_VALUE_FONT_H {vh}U\n")
    f.write("const UiFontAa_Glyph *UiFontAa_Find(uint32_t cp);\n")
    f.write("uint16_t UiFontAa_TextWidth(const char *utf8);\n\n")
    f.write("#endif\n")

# Write source
src_path = os.path.join(SRC, "ui_font_aa.c")
with open(src_path, "w", encoding="utf-8") as f:
    f.write('#include "ui_font_aa.h"\n\n')
    write_rgb565_array(f, "UiTitle_CleanRoom", title_rgb)
    for ch in ("T", "H", "P"):
        write_alpha_array(f, f"UiLetter_{ch}", letters[ch])
        # make them extern by removing static - rewrite
    # Fix: letters were written as static - rewrite file properly

print("partial done, rewriting src without static for letters")

with open(src_path, "w", encoding="utf-8") as f:
    f.write('#include "ui_font_aa.h"\n\n')
    write_rgb565_array(f, "UiTitle_CleanRoom", title_rgb)

    for ch in ("T", "H", "P"):
        img = letters[ch]
        w, h = img.size
        data = list(img.getdata())
        f.write(f"const uint8_t UiLetter_{ch}[{w * h}] = {{\n")
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

    glyph_meta = []
    for ch in value_chars:
        img = glyphs[ch]
        w, h = img.size
        name = f"ui_val_{ord(ch):04X}"
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
        glyph_meta.append((ord(ch), w, h, name))

    f.write("static const UiFontAa_Glyph ui_value_glyphs[] = {\n")
    for cp, w, h, name in sorted(glyph_meta, key=lambda t: t[0]):
        f.write(f"  {{ 0x{cp:04X}U, {w}U, {h}U, {name} }},\n")
    f.write("};\n\n")
    n = len(glyph_meta)
    f.write("const UiFontAa_Glyph *UiFontAa_Find(uint32_t cp)\n{\n")
    f.write("  uint32_t lo = 0U;\n")
    f.write(f"  uint32_t hi = {n}U;\n")
    f.write("  while (lo < hi) {\n")
    f.write("    uint32_t mid = lo + ((hi - lo) / 2U);\n")
    f.write("    uint32_t v = ui_value_glyphs[mid].cp;\n")
    f.write("    if (v == cp) {\n")
    f.write("      return &ui_value_glyphs[mid];\n")
    f.write("    }\n")
    f.write("    if (v < cp) {\n")
    f.write("      lo = mid + 1U;\n")
    f.write("    } else {\n")
    f.write("      hi = mid;\n")
    f.write("    }\n")
    f.write("  }\n")
    f.write("  return 0;\n")
    f.write("}\n\n")

    f.write("static uint32_t ui_utf8_next(const char **text)\n{\n")
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
    f.write("  return cp;\n")
    f.write("}\n\n")

    f.write("uint16_t UiFontAa_TextWidth(const char *utf8)\n{\n")
    f.write("  uint16_t w = 0U;\n")
    f.write("  if (utf8 == 0) return 0U;\n")
    f.write("  while (*utf8) {\n")
    f.write("    uint32_t cp = ui_utf8_next(&utf8);\n")
    f.write("    const UiFontAa_Glyph *g;\n")
    f.write("    if (cp == 0U) break;\n")
    f.write("    g = UiFontAa_Find(cp);\n")
    f.write("    if (g) w = (uint16_t)(w + g->w);\n")
    f.write("    else w = (uint16_t)(w + (UI_VALUE_FONT_H / 2U));\n")
    f.write("  }\n")
    f.write("  return w;\n")
    f.write("}\n")

print("font", FONT_PATH)
print("title", tw, th)
for ch in ("T", "H", "P"):
    print("letter", ch, letters[ch].size)
print("value H", vh, "glyphs", len(glyph_meta))
print("wrote", hdr_path)
print("wrote", src_path)
