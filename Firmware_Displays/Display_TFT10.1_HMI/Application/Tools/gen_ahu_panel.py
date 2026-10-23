"""Bake the 1024x600 still (no arrows, empty hub) plus motion patches."""
from PIL import Image
import os

root = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
src = os.path.join(root, "Core", "Src", "ahu_snap.png")
still = r"c:\Users\morfe\AppData\Local\Temp\cursor\screenshots\ahu_still2.png"
if os.path.isfile(still):
    src = still
out_png = os.path.join(root, "Core", "Src", "ahu_snap.png")
out_h = os.path.join(root, "Core", "Inc", "ahu_snap.h")
out_rle = os.path.join(root, "Core", "Src", "ahu_snap.c")
out_mot_h = os.path.join(root, "Core", "Inc", "ahu_motion.h")
out_mot_c = os.path.join(root, "Core", "Src", "ahu_motion.c")

raw = Image.open(src).convert("RGB").resize((1024, 600), Image.Resampling.BOX)
px = raw.load()
im = Image.new("RGB", (1024, 600))
op = im.load()
for y in range(600):
    for x in range(1024):
        r, g, b = px[x, y]
        op[x, y] = (r & 0xF8, g & 0xFC, b & 0xF8)
im.save(out_png, "PNG")
w, h = im.size
pix = im.tobytes()


def rgb565_at(x, y):
    i = (y * w + x) * 3
    r, g, b = pix[i], pix[i + 1], pix[i + 2]
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


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
        c = rgb565_at(x, y)
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

with open(out_h, "w", encoding="utf-8") as f:
    f.write("#ifndef AHU_SNAP_INCLUDED\n#define AHU_SNAP_INCLUDED\n\n")
    f.write("#include <stdint.h>\n\n")
    f.write(f"#define AHU_SNAP_W {w}U\n")
    f.write(f"#define AHU_SNAP_HT {h}U\n")
    f.write(f"#define AHU_SNAP_RLE_N {len(rle)}U\n\n")
    f.write("extern const uint8_t AhuSnap_Rle[AHU_SNAP_RLE_N];\n")
    f.write("void AhuSnap_Draw(void);\n")
    f.write("void AhuSnap_Tick(uint32_t tick_ms, uint8_t on);\n\n")
    f.write("#endif\n")

with open(out_rle, "w", encoding="utf-8", newline="\n") as f:
    f.write('#include "ahu_snap.h"\n#include "lt7683.h"\n\n')
    f.write("void AhuSnap_Draw(void)\n{\n")
    f.write("  LT7683_DrawRle8(0U, 0U, AHU_SNAP_W, AHU_SNAP_HT, AhuSnap_Rle, AHU_SNAP_RLE_N);\n")
    f.write("}\n\n")
    f.write("const uint8_t AhuSnap_Rle[AHU_SNAP_RLE_N] = {\n")
    for i in range(0, len(rle), 16):
        chunk = rle[i : i + 16]
        f.write("  " + ", ".join(f"0x{b:02X}U" for b in chunk) + ",\n")
    f.write("};\n")

# Motion patches: gray margins for arrows, square around the fan hub.
L = dict(x=48, y=258, w=96, h=48)
R = dict(x=912, y=258, w=96, h=48)
# Hub on the still is the dark ellipse around 576,311 (not the snail horn).
F = dict(x=544, y=279, s=64, cx=32, cy=32, r=22)


def patch(x0, y0, pw, ph):
    out = []
    for y in range(ph):
        for x in range(pw):
            out.append(rgb565_at(x0 + x, y0 + y))
    return out


lp = patch(L["x"], L["y"], L["w"], L["h"])
rp = patch(R["x"], R["y"], R["w"], R["h"])
fp = patch(F["x"], F["y"], F["s"], F["s"])


def dump_arr(f, name, data, per=12):
    f.write(f"const uint16_t {name}[{len(data)}] = {{\n")
    for i in range(0, len(data), per):
        chunk = data[i : i + per]
        f.write("  " + ", ".join(f"0x{c:04X}U" for c in chunk) + ",\n")
    f.write("};\n\n")


with open(out_mot_h, "w", encoding="utf-8") as f:
    f.write("#ifndef AHU_MOTION_H\n#define AHU_MOTION_H\n\n#include <stdint.h>\n\n")
    f.write(f"#define FLOW_L_X {L['x']}U\n#define FLOW_L_Y {L['y']}U\n")
    f.write(f"#define FLOW_L_W {L['w']}U\n#define FLOW_L_H {L['h']}U\n")
    f.write(f"#define FLOW_R_X {R['x']}U\n#define FLOW_R_Y {R['y']}U\n")
    f.write(f"#define FLOW_R_W {R['w']}U\n#define FLOW_R_H {R['h']}U\n")
    f.write(f"#define FAN_X {F['x']}U\n#define FAN_Y {F['y']}U\n#define FAN_S {F['s']}U\n")
    f.write(f"#define FAN_CX {F['cx']}\n#define FAN_CY {F['cy']}\n#define FAN_R {F['r']}\n")
    f.write(f"#define FLOW_L_N {len(lp)}U\n#define FLOW_R_N {len(rp)}U\n#define FAN_N {len(fp)}U\n\n")
    f.write("extern const uint16_t AhuFlow_LeftBg[FLOW_L_N];\n")
    f.write("extern const uint16_t AhuFlow_RightBg[FLOW_R_N];\n")
    f.write("extern const uint16_t AhuFan_Bg[FAN_N];\n\n#endif\n")

with open(out_mot_c, "w", encoding="utf-8", newline="\n") as f:
    f.write('#include "ahu_motion.h"\n\n')
    dump_arr(f, "AhuFlow_LeftBg", lp)
    dump_arr(f, "AhuFlow_RightBg", rp)
    dump_arr(f, "AhuFan_Bg", fp)

print("png", out_png, "rle", len(rle), "left", len(lp) * 2, "fan", len(fp) * 2)
