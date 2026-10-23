"""RGB565 portrait for the About page. Keep original aspect ratio."""
from PIL import Image
import os

src = r"C:\Users\morfe\.cursor\projects\c-Users-morfe-STM32CubeIDE-workspace-1-17-0-HVAC-CleanRoom-Project\assets\c__Users_morfe_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images_2708-c99f685c-2956-4817-b51b-58e7157e923b.png"
root = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
out_c = os.path.join(root, "Core", "Src", "author_photo.c")
out_h = os.path.join(root, "Core", "Inc", "author_photo.h")

MAX_W, MAX_H = 184, 228

im = Image.open(src).convert("RGB")
print("src", im.size)
im.thumbnail((MAX_W, MAX_H), Image.Resampling.LANCZOS)
W, H = im.size

pixels = []
for y in range(H):
    for x in range(W):
        r, g, b = im.getpixel((x, y))
        c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        pixels.append(c)

with open(out_h, "w", encoding="utf-8") as f:
    f.write("#ifndef AUTHOR_PHOTO_INCLUDED\n#define AUTHOR_PHOTO_INCLUDED\n\n")
    f.write("#include <stdint.h>\n\n")
    f.write(f"#define AUTHOR_PHOTO_W {W}U\n")
    f.write(f"#define AUTHOR_PHOTO_H {H}U\n")
    f.write("extern const uint16_t AuthorPhoto_RGB565[];\n\n")
    f.write("#endif\n")

with open(out_c, "w", encoding="utf-8") as f:
    f.write('#include "author_photo.h"\n\n')
    f.write("const uint16_t AuthorPhoto_RGB565[AUTHOR_PHOTO_W * AUTHOR_PHOTO_H] = {\n")
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

print("ok", W, H, "aspect", round(W / H, 4), "src_aspect", "bytes", len(pixels) * 2)
