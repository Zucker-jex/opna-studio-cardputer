#!/usr/bin/env python3
"""Generate a 12x12 1bpp CP932 glyph table from Fusion Pixel 12px ja TTF."""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

TTF = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("fusion-pixel-12px-monospaced-ja.ttf")
OUT = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("main/jp_font_data.h")
W = H = 12
GLYPH_BYTES = H * 2

font = ImageFont.truetype(str(TTF), 12)
codes = []
for b in range(0x20, 0x7f):
    codes.append(b)
for b in range(0xa1, 0xe0):
    codes.append(b)
for lead in list(range(0x81, 0xa0)) + list(range(0xe0, 0xfd)):
    for trail in list(range(0x40, 0x7f)) + list(range(0x80, 0xfd)):
        code = (lead << 8) | trail
        try:
            ch = bytes((lead, trail)).decode("cp932")
        except UnicodeDecodeError:
            continue
        if len(ch) == 1:
            codes.append(code)
codes = sorted(set(codes))
glyphs = []
for code in codes:
    if code < 0x80:
        ch = chr(code)
    elif 0xa1 <= code <= 0xdf:
        ch = bytes((code,)).decode("cp932")
    else:
        ch = bytes((code >> 8, code & 0xff)).decode("cp932")
    im = Image.new("L", (W, H), 0)
    ImageDraw.Draw(im).text((0, 0), ch, font=font, fill=255, anchor="lt")
    rows = []
    for y in range(H):
        bits = 0
        for x in range(W):
            if im.getpixel((x, y)) > 0:
                bits |= 0x8000 >> x
        rows.append(bits >> 8)
        rows.append(bits & 0xff)
    glyphs.append(rows)

OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open("w", encoding="ascii") as f:
    f.write("/* Auto-generated from %s. Do not edit. */\n" % TTF.name)
    f.write("#pragma once\n#include <stdint.h>\n")
    f.write("#define JP_FONT_W 12\n#define JP_FONT_H 12\n")
    f.write("#define JP_FONT_COUNT %d\n" % len(codes))
    f.write("static const uint16_t kJpCode[JP_FONT_COUNT] = {\n")
    for i, c in enumerate(codes):
        f.write("0x%04X,%s" % (c, "\n" if (i & 15) == 15 else " "))
    f.write("\n};\n")
    f.write("static const uint8_t kJpBitmap[JP_FONT_COUNT][24] = {\n")
    for i, g in enumerate(glyphs):
        f.write("{" + ",".join("0x%02X" % b for b in g) + "},\n")
    f.write("};\n")
print("wrote", OUT, "glyphs", len(codes), "bytes", OUT.stat().st_size)
