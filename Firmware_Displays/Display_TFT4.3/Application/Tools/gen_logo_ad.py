from PIL import Image

src = r"C:\Users\morfe\.cursor\projects\c-Users-morfe-STM32CubeIDE-workspace-1-17-0-STM32H723ZGT6-with-TFT5-SSD1963\assets\c__Users_morfe_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_LOGO_EDUARD_1-233ea6a8-a69d-4844-857a-6bdddb5ca645.png"
out_c = r"C:\Users\morfe\STM32CubeIDE\workspace_1.17.0\STM32H723ZGT6 with TFT5 SSD1963\Core\Src\logo_ad.c"
out_h = r"C:\Users\morfe\STM32CubeIDE\workspace_1.17.0\STM32H723ZGT6 with TFT5 SSD1963\Core\Inc\logo_ad.h"
SIZE = 56
HEADER = (48, 48, 48, 255)

im = Image.open(src).convert("RGBA")
bbox = im.getbbox()
im = im.crop(bbox)
s = max(im.size)
canvas = Image.new("RGBA", (s, s), (0, 0, 0, 0))
canvas.paste(im, ((s - im.size[0]) // 2, (s - im.size[1]) // 2), im)
im = canvas.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
bg = Image.new("RGBA", (SIZE, SIZE), HEADER)
bg = Image.alpha_composite(bg, im)
rgb = bg.convert("RGB")

pixels = []
for y in range(SIZE):
    for x in range(SIZE):
        r, g, b = rgb.getpixel((x, y))
        c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        pixels.append(c)

with open(out_h, "w", encoding="utf-8") as f:
    f.write("#ifndef LOGO_AD_H\n#define LOGO_AD_H\n\n")
    f.write("#include <stdint.h>\n\n")
    f.write(f"#define LOGO_AD_WIDTH  {SIZE}U\n")
    f.write(f"#define LOGO_AD_HEIGHT {SIZE}U\n")
    f.write("extern const uint16_t LogoAd_RGB565[];\n\n")
    f.write("#endif\n")

with open(out_c, "w", encoding="utf-8") as f:
    f.write('#include "logo_ad.h"\n\n')
    f.write("const uint16_t LogoAd_RGB565[LOGO_AD_WIDTH * LOGO_AD_HEIGHT] = {\n")
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

print("ok", SIZE, len(pixels))
