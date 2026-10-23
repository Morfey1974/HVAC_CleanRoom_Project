"""Whole-word RGB565 menu labels: same baseline, 3x supersample."""
from PIL import Image, ImageDraw, ImageFont
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
INC = os.path.join(ROOT, "Core", "Inc")
SRC = os.path.join(ROOT, "Core", "Src")

FONT_PATH = r"C:\Windows\Fonts\segoeuib.ttf"
SCALE = 3
SIZE = 26 * SCALE
PAD_X = 4 * SCALE
PAD_Y = 2 * SCALE

PANEL = (204, 204, 204)
BTN = (230, 230, 230)
COL_T = (255, 96, 0)
COL_H = (0, 92, 160)
COL_P = (0, 210, 32)
OK = (0, 140, 40)
CLOSE = (200, 0, 0)
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)

FONT = ImageFont.truetype(FONT_PATH, SIZE)
ASCENT, DESCENT = FONT.getmetrics()
H3 = ASCENT + DESCENT + (2 * PAD_Y)
BASE = PAD_Y + ASCENT
H = H3 // SCALE


COV_MIN = 40


def rgb565(im):
    pixels = []
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            r, g, b = im.getpixel((x, y))
            pixels.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
    return pixels


def compose(alpha, fg, bg):
    w, h = alpha.size
    we = w + (w & 1)
    he = h + (h & 1)
    out = Image.new("RGB", (we, he), bg)
    pa = alpha.load()
    po = out.load()
    fr, fg_, fb = fg
    br, bg_, bb = bg
    for y in range(h):
        for x in range(w):
            a = pa[x, y]
            if a < COV_MIN:
                continue
            if a >= 250:
                po[x, y] = fg
            else:
                t = a / 255.0
                po[x, y] = (
                    int(br + (fr - br) * t + 0.5),
                    int(bg_ + (fg_ - bg_) * t + 0.5),
                    int(bb + (fb - bb) * t + 0.5),
                )
    return out


def render_mask(text, x0):
    probe = Image.new("L", (1, 1), 0)
    d = ImageDraw.Draw(probe)
    bbox = d.textbbox((0, BASE), text, font=FONT, anchor="ls")
    w3 = max(SCALE, (bbox[2] - bbox[0]) + (2 * PAD_X))
    img = Image.new("L", (w3, H3), 0)
    d = ImageDraw.Draw(img)
    d.text((x0, BASE), text, font=FONT, fill=255, anchor="ls")
    w = max(1, w3 // SCALE)
    return img.resize((w, H), Image.Resampling.LANCZOS)


def render_line(text, fg, bg):
    return compose(render_mask(text, PAD_X), fg, bg)


def digit_cell_w3():
    probe = Image.new("L", (1, 1), 0)
    d = ImageDraw.Draw(probe)
    widest = 0
    for ch in "0123456789":
        bbox = d.textbbox((0, BASE), ch, font=FONT, anchor="ls")
        widest = max(widest, bbox[2] - bbox[0])
    w3 = widest + (2 * PAD_X)
    w3 += SCALE - (w3 % SCALE)
    return w3


def render_cell(ch, fg, bg, w3):
    probe = Image.new("L", (1, 1), 0)
    d = ImageDraw.Draw(probe)
    bbox = d.textbbox((0, BASE), ch, font=FONT, anchor="ls")
    tw = bbox[2] - bbox[0]
    x = ((w3 - tw) // 2) - bbox[0]
    img = Image.new("L", (w3, H3), 0)
    d = ImageDraw.Draw(img)
    d.text((x, BASE), ch, font=FONT, fill=255, anchor="ls")
    w = w3 // SCALE
    return compose(img.resize((w, H), Image.Resampling.LANCZOS), fg, bg)


labels = [
    ("TitleMenu", "МЕНЮ", BLACK, PANEL),
    ("TitleSp", "SP SYSTEM", BLACK, PANEL),
    ("TitleBright", "ЯРКОСТЬ", BLACK, PANEL),
    ("TitleSim", "СИМУЛЯЦИЯ", BLACK, PANEL),
    ("TitleAbout", "О СИСТЕМЕ", BLACK, PANEL),
    ("BtnSp", "SP SYSTEM", WHITE, COL_T),
    ("BtnBright", "ЯРКОСТЬ", WHITE, COL_H),
    ("BtnSim", "СИМУЛЯЦИЯ", BLACK, COL_P),
    ("BtnAbout", "О СИСТЕМЕ", BLACK, BTN),
    ("BtnClose", "ЗАКРЫТЬ", WHITE, CLOSE),
    ("BtnBack", "НАЗАД", BLACK, BTN),
    ("NameT", "T", BLACK, PANEL),
    ("NameH", "H", BLACK, PANEL),
    ("NameP", "P", BLACK, PANEL),
    ("BtnMinus", "−", WHITE, COL_T),
    ("BtnPlus", "+", BLACK, COL_P),
    ("AboutHvac", "HVAC CLEAN ROOM", BLACK, PANEL),
    ("AboutPanel", "ПАНЕЛЬ ОПЕРАТОРА 10.1", BLACK, PANEL),
    ("AboutAuthor", "Автор", BLACK, PANEL),
    ("AboutName", "Эдуард Ключерев", BLACK, PANEL),
    ("SimOn", "СИМ: ВКЛ", WHITE, OK),
    ("SimOff", "СИМ: ВЫКЛ", BLACK, BTN),
    ("LinkOn", "СВЯЗЬ: ЕСТЬ", WHITE, OK),
    ("LinkOff", "СВЯЗЬ: НЕТ", WHITE, CLOSE),
    ("UnitC", " C", BLACK, BTN),
    ("UnitPct", " %", BLACK, BTN),
    ("UnitPa", " Pa", BLACK, BTN),
]

doors = []
for i in range(1, 6):
    doors.append((f"DoorOk{i}", str(i), WHITE, OK))
    doors.append((f"DoorNo{i}", str(i), WHITE, CLOSE))

cell_w3 = digit_cell_w3()
dig_imgs = [render_cell(str(i), BLACK, BTN, cell_w3) for i in range(10)]
DIG_W, DIG_H = dig_imgs[0].size

rendered = []
for name, text, fg, bg in labels + doors:
    im = render_line(text, fg, bg)
    rendered.append((name, im, rgb565(im)))

preview_w = max(im.size[0] for _, im, _ in rendered)
preview = Image.new("RGB", (preview_w + 16, (H + 8) * len(rendered) + 8), (40, 40, 40))
py = 8
for _, im, _ in rendered:
    preview.paste(im, (8, py))
    py += H + 8
preview.save(os.path.join(os.path.dirname(__file__), "preview_menu_labels.png"))

hdr_path = os.path.join(INC, "ui_menu_labels.h")
src_path = os.path.join(SRC, "ui_menu_labels.c")

with open(hdr_path, "w", encoding="utf-8") as f:
    f.write("#ifndef UI_MENU_LABELS_H\n#define UI_MENU_LABELS_H\n\n")
    f.write("#include <stdint.h>\n\n")
    f.write("typedef struct\n{\n  uint16_t w;\n  uint16_t h;\n  const uint16_t *rgb;\n} HmiLbl;\n\n")
    f.write(f"#define HMI_LBL_H {H}U\n")
    f.write(f"#define HMI_DIG_W {DIG_W}U\n")
    f.write(f"#define HMI_DIG_H {DIG_H}U\n\n")
    for name, im, _ in rendered:
        f.write(f"extern const HmiLbl HmiLbl_{name};\n")
    f.write("extern const uint16_t * const HmiDig_Btn[10];\n")
    f.write("extern const HmiLbl * const HmiLbl_DoorOk[5];\n")
    f.write("extern const HmiLbl * const HmiLbl_DoorNo[5];\n\n")
    f.write("#endif\n")

with open(src_path, "w", encoding="utf-8") as f:
    f.write('#include "ui_menu_labels.h"\n\n')
    for name, im, pix in rendered:
        f.write(f"static const uint16_t HmiLbl_{name}_rgb[{im.size[0] * im.size[1]}] = {{\n")
        for i, c in enumerate(pix):
            if i % 12 == 0:
                f.write("  ")
            f.write(f"0x{c:04X}U,")
            f.write("\n" if i % 12 == 11 else " ")
        if len(pix) % 12:
            f.write("\n")
        f.write("};\n")
        f.write(f"const HmiLbl HmiLbl_{name} = {{ {im.size[0]}U, {im.size[1]}U, HmiLbl_{name}_rgb }};\n\n")

    for i, im in enumerate(dig_imgs):
        pix = rgb565(im)
        f.write(f"static const uint16_t HmiDig_{i}_rgb[{DIG_W * DIG_H}] = {{\n")
        for n, c in enumerate(pix):
            if n % 12 == 0:
                f.write("  ")
            f.write(f"0x{c:04X}U,")
            f.write("\n" if n % 12 == 11 else " ")
        if len(pix) % 12:
            f.write("\n")
        f.write("};\n\n")

    f.write("const uint16_t * const HmiDig_Btn[10] = {\n")
    for i in range(10):
        f.write(f"  HmiDig_{i}_rgb,\n")
    f.write("};\n\n")
    f.write("const HmiLbl * const HmiLbl_DoorOk[5] = {\n")
    for i in range(1, 6):
        f.write(f"  &HmiLbl_DoorOk{i},\n")
    f.write("};\n\n")
    f.write("const HmiLbl * const HmiLbl_DoorNo[5] = {\n")
    for i in range(1, 6):
        f.write(f"  &HmiLbl_DoorNo{i},\n")
    f.write("};\n")

print("H", H, "dig", DIG_W, DIG_H, "labels", len(rendered))
print("bytes", sum(im.size[0] * im.size[1] * 2 for _, im, _ in rendered) + (10 * DIG_W * DIG_H * 2))
