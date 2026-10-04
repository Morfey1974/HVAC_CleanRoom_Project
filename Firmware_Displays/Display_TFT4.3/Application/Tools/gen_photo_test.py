from PIL import Image
from pathlib import Path

src = Path(r"C:\Users\morfe\Downloads\WhatsApp Image 2026-05-08 at 05.39.57.jpeg")
root = Path(r"C:\Users\morfe\STM32CubeIDE\workspace_1.17.0\STM32H723ZGT6 with TFT5 SSD1963")
out_h = root / "Core" / "Inc" / "photo_test.h"
out_c = root / "Core" / "Src" / "photo_test.c"

im = Image.open(src).convert("RGB")
print("orig", im.size)
# 90° clockwise so portrait selfie fits landscape TFT upright
im = im.transpose(Image.Transpose.ROTATE_270)
print("rotated", im.size)
im.thumbnail((800, 480), Image.Resampling.LANCZOS)
w, h = im.size
print("scaled", w, h, "bytes", w * h * 2)

pixels = []
for y in range(h):
    for x in range(w):
        r, g, b = im.getpixel((x, y))
        c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        pixels.append(c)

out_h.write_text(
    "#ifndef PHOTO_TEST_H_\n"
    "#define PHOTO_TEST_H_\n\n"
    "#include <stdint.h>\n\n"
    f"#define PHOTO_TEST_W {w}U\n"
    f"#define PHOTO_TEST_H {h}U\n\n"
    "extern const uint16_t PhotoTest_RGB565[];\n\n"
    "#endif\n",
    encoding="utf-8",
)

with out_c.open("w", encoding="utf-8") as f:
    f.write('#include "photo_test.h"\n\n')
    f.write("const uint16_t PhotoTest_RGB565[PHOTO_TEST_W * PHOTO_TEST_H] = {\n")
    for i, c in enumerate(pixels):
        if i % 12 == 0:
            f.write("  ")
        f.write(f"0x{c:04X}U,")
        if i % 12 == 11:
            f.write("\n")
        else:
            f.write(" ")
    if len(pixels) % 12:
        f.write("\n")
    f.write("};\n")

print("wrote", out_c, "size_kb", out_c.stat().st_size // 1024)
