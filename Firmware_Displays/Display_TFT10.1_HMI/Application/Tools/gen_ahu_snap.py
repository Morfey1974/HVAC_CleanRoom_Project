"""Pack the 1024x600 AHU snapshot as RGB565 RLE for the 10.1 panel."""
from PIL import Image
import os

root = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
src = os.path.join(root, "Core", "Src", "ahu_snap.png")
out_h = os.path.join(root, "Core", "Inc", "ahu_snap.h")
out_c = os.path.join(root, "Core", "Src", "ahu_snap.c")

im = Image.open(src).convert("RGB")
w, h = im.size
if (w, h) != (1024, 600):
    raise SystemExit(f"expected 1024x600, got {w}x{h}")

pix = im.tobytes()
rle = bytearray()
prev = None
n = 0
pixels = 0


def flush():
    global n
    while n > 0:
        chunk = 255 if n > 255 else n
        rle.append(chunk)
        rle.append(prev & 0xFF)
        rle.append((prev >> 8) & 0xFF)
        n -= chunk


for y in range(h):
    for x in range(w):
        i = (y * w + x) * 3
        r, g, b = pix[i], pix[i + 1], pix[i + 2]
        c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        if c == prev:
            n += 1
        else:
            if prev is not None:
                flush()
            prev = c
            n = 1
        pixels += 1
if n:
    flush()

if pixels != w * h:
    raise SystemExit("pixel count mismatch")

with open(out_h, "w", encoding="utf-8") as f:
    f.write("#ifndef AHU_SNAP_H\n#define AHU_SNAP_H\n\n")
    f.write("#include <stdint.h>\n\n")
    f.write(f"#define AHU_SNAP_W {w}U\n")
    f.write(f"#define AHU_SNAP_H {h}U\n")
    f.write(f"#define AHU_SNAP_RLE_N {len(rle)}U\n\n")
    f.write("extern const uint8_t AhuSnap_Rle[AHU_SNAP_RLE_N];\n")
    f.write("void AhuSnap_Draw(void);\n\n")
    f.write("#endif\n")

with open(out_c, "w", encoding="utf-8", newline="\n") as f:
    f.write('#include "ahu_snap.h"\n')
    f.write('#include "lt7683.h"\n\n')
    f.write("void AhuSnap_Draw(void)\n")
    f.write("{\n")
    f.write("  LT7683_DrawRle8(0U, 0U, AHU_SNAP_W, AHU_SNAP_H, AhuSnap_Rle, AHU_SNAP_RLE_N);\n")
    f.write("}\n\n")
    f.write("const uint8_t AhuSnap_Rle[AHU_SNAP_RLE_N] = {\n")
    for i in range(0, len(rle), 16):
        chunk = rle[i : i + 16]
        f.write("  " + ", ".join(f"0x{b:02X}U" for b in chunk) + ",\n")
    f.write("};\n")

print("ok", w, h, "rle", len(rle), "pixels", pixels)
