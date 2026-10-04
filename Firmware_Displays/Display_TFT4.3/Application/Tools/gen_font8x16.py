from PIL import Image, ImageDraw, ImageFont
import os

candidates = [
    r"C:\Windows\Fonts\consola.ttf",
    r"C:\Windows\Fonts\arial.ttf",
    r"C:\Windows\Fonts\tahoma.ttf",
    r"C:\Windows\Fonts\cour.ttf",
]
font_path = next(p for p in candidates if os.path.exists(p))
font = ImageFont.truetype(font_path, 13)

chars = [chr(c) for c in range(0x20, 0x7F)]
cyr = "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя°"
chars += list(cyr)

W, H = 8, 16
glyphs = {}
for ch in chars:
    img = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(img)
    bbox = d.textbbox((0, 0), ch, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    x = max(0, (W - tw) // 2 - bbox[0])
    y = max(0, (H - th) // 2 - bbox[1])
    d.text((x, y), ch, font=font, fill=255)
    rows = []
    for yy in range(H):
        byte = 0
        for xx in range(W):
            if img.getpixel((xx, yy)) > 80:
                byte |= 0x80 >> xx
        rows.append(byte)
    glyphs[ch] = rows

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = os.path.join(root, "Core", "Src", "font8x16_data.c")
hdr = os.path.join(root, "Core", "Inc", "font8x16.h")

items = sorted({ord(ch): glyphs[ch] for ch in glyphs}.items())

with open(hdr, "w", encoding="utf-8") as f:
    f.write("#ifndef FONT8X16_H\n#define FONT8X16_H\n\n")
    f.write("#include <stdint.h>\n\n")
    f.write("#define FONT8X16_WIDTH  8U\n")
    f.write("#define FONT8X16_HEIGHT 16U\n\n")
    f.write("const uint8_t *Font8x16_GetGlyph(uint32_t codepoint);\n\n")
    f.write("#endif\n")

with open(out, "w", encoding="utf-8") as f:
    f.write('#include "font8x16.h"\n\n')
    f.write("typedef struct {\n  uint32_t cp;\n  uint8_t rows[16];\n} Font8x16_Glyph;\n\n")
    f.write("static const Font8x16_Glyph font8x16_table[] = {\n")
    for cp, rows in items:
        row_str = ", ".join(f"0x{b:02X}U" for b in rows)
        f.write(f"  {{ 0x{cp:04X}U, {{ {row_str} }} }},\n")
    f.write("};\n\n")
    f.write("static const uint8_t font8x16_empty[16] = {0};\n\n")
    f.write("const uint8_t *Font8x16_GetGlyph(uint32_t codepoint)\n{\n")
    f.write("  uint32_t lo = 0U;\n")
    f.write(f"  uint32_t hi = {len(items)}U;\n")
    f.write("  while (lo < hi) {\n")
    f.write("    uint32_t mid = lo + ((hi - lo) / 2U);\n")
    f.write("    uint32_t v = font8x16_table[mid].cp;\n")
    f.write("    if (v == codepoint) {\n")
    f.write("      return font8x16_table[mid].rows;\n")
    f.write("    }\n")
    f.write("    if (v < codepoint) {\n")
    f.write("      lo = mid + 1U;\n")
    f.write("    } else {\n")
    f.write("      hi = mid;\n")
    f.write("    }\n")
    f.write("  }\n")
    f.write("  return font8x16_empty;\n")
    f.write("}\n")

print("glyphs", len(items), "font", font_path)
print("wrote", out)
print("wrote", hdr)
