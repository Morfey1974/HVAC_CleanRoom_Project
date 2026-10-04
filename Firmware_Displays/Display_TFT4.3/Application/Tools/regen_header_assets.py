from PIL import Image, ImageDraw, ImageFont
import os
import re

ROOT = r"C:\Users\morfe\STM32CubeIDE\workspace_1.17.0\STM32H723ZGT6 with TFT5 SSD1963"
INC = os.path.join(ROOT, "Core", "Inc")
SRC = os.path.join(ROOT, "Core", "Src")
ASSETS = r"C:\Users\morfe\.cursor\projects\c-Users-morfe-STM32CubeIDE-workspace-1-17-0-STM32H723ZGT6-with-TFT5-SSD1963\assets\c__Users_morfe_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_LOGO_EDUARD_1-233ea6a8-a69d-4844-857a-6bdddb5ca645.png"

FONT_PATH = next(
    p
    for p in (
        r"C:\Windows\Fonts\arialbd.ttf",
        r"C:\Windows\Fonts\segoeuib.ttf",
        r"C:\Windows\Fonts\arial.ttf",
    )
    if os.path.exists(p)
)

HEADER = (204, 204, 204)  # CMYK K=20
LOGO_SIZE = 128
TITLE_TEXT = "CLEAN ROOM"
TITLE_SIZE = 56
TITLE_FG = (41, 107, 122)  # CMYK 79/45/37/24


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def dump_rgb565_body(img_rgb):
    w, h = img_rgb.size
    lines = []
    row = []
    i = 0
    for y in range(h):
        for x in range(w):
            r, g, b = img_rgb.getpixel((x, y))
            row.append(f"0x{rgb565(r, g, b):04X}U")
            i += 1
            if len(row) == 12:
                lines.append("  " + ", ".join(row) + ",")
                row = []
    if row:
        lines.append("  " + ", ".join(row) + ",")
    return w, h, "\n".join(lines)


# --- Logo ---
im = Image.open(ASSETS).convert("RGBA")
bbox = im.getbbox()
im = im.crop(bbox)
s = max(im.size)
canvas = Image.new("RGBA", (s, s), (0, 0, 0, 0))
canvas.paste(im, ((s - im.size[0]) // 2, (s - im.size[1]) // 2), im)
im = canvas.resize((LOGO_SIZE, LOGO_SIZE), Image.Resampling.LANCZOS)
bg = Image.new("RGBA", (LOGO_SIZE, LOGO_SIZE), (*HEADER, 255))
logo_rgb = Image.alpha_composite(bg, im).convert("RGB")
lw, lh, logo_body = dump_rgb565_body(logo_rgb)

with open(os.path.join(INC, "logo_ad.h"), "w", encoding="utf-8") as f:
    f.write("#ifndef LOGO_AD_H\n#define LOGO_AD_H\n\n")
    f.write("#include <stdint.h>\n\n")
    f.write(f"#define LOGO_AD_WIDTH  {lw}U\n")
    f.write(f"#define LOGO_AD_HEIGHT {lh}U\n")
    f.write("extern const uint16_t LogoAd_RGB565[];\n\n")
    f.write("#endif\n")

with open(os.path.join(SRC, "logo_ad.c"), "w", encoding="utf-8") as f:
    f.write('#include "logo_ad.h"\n\n')
    f.write("const uint16_t LogoAd_RGB565[LOGO_AD_WIDTH * LOGO_AD_HEIGHT] = {\n")
    f.write(logo_body + "\n")
    f.write("};\n")

# --- Title ---
font = ImageFont.truetype(FONT_PATH, TITLE_SIZE)
tmp = Image.new("L", (1, 1), 0)
d = ImageDraw.Draw(tmp)
bbox = d.textbbox((0, 0), TITLE_TEXT, font=font)
tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
pad = 6
w, h = tw + 2 * pad, th + 2 * pad
alpha = Image.new("L", (w, h), 0)
d = ImageDraw.Draw(alpha)
d.text((pad - bbox[0], pad - bbox[1]), TITLE_TEXT, font=font, fill=255)
title_rgb = Image.new("RGB", (w, h), HEADER)
for y in range(h):
    for x in range(w):
        a = alpha.getpixel((x, y)) / 255.0
        if a > 0:
            r = int(HEADER[0] + (TITLE_FG[0] - HEADER[0]) * a)
            g = int(HEADER[1] + (TITLE_FG[1] - HEADER[1]) * a)
            b = int(HEADER[2] + (TITLE_FG[2] - HEADER[2]) * a)
            title_rgb.putpixel((x, y), (r, g, b))

tw, th, title_body = dump_rgb565_body(title_rgb)

# Update ui_font_aa.h title sizes
hdr = os.path.join(INC, "ui_font_aa.h")
with open(hdr, "r", encoding="utf-8") as f:
    text = f.read()
text = re.sub(r"#define UI_TITLE_W \d+U", f"#define UI_TITLE_W {tw}U", text)
text = re.sub(r"#define UI_TITLE_H \d+U", f"#define UI_TITLE_H {th}U", text)
with open(hdr, "w", encoding="utf-8") as f:
    f.write(text)

# Replace title array in ui_font_aa.c
src = os.path.join(SRC, "ui_font_aa.c")
with open(src, "r", encoding="utf-8") as f:
    ctext = f.read()

new_title = (
    f"const uint16_t UiTitle_CleanRoom[{tw} * {th}] = {{\n"
    f"{title_body}\n"
    f"}};\n"
)
ctext2, n = re.subn(
    r"const uint16_t UiTitle_CleanRoom\[[^\]]+\] = \{.*?\n\};\n",
    new_title,
    ctext,
    count=1,
    flags=re.S,
)
if n != 1:
    raise SystemExit(f"title replace failed: {n}")
with open(src, "w", encoding="utf-8") as f:
    f.write(ctext2)

print("logo", lw, lh)
print("title", tw, th, TITLE_TEXT)
print("ok")
