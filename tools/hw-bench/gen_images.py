#!/usr/bin/env python3
"""Pictures for the panel test, 4 bits a pixel. The output stays in the build directory.

gen_images.py font.ttc out.h name=picture.png ...
The first picture is drawn here: numbered dots to tap, at places the report can compare with.
"""
import sys
from PIL import Image, ImageDraw, ImageFont

DOTS = ((110, 300), (574, 300), (574, 1000), (110, 1000))


def dots(font_path):
    im = Image.new("L", (684, 1216), 255)
    d = ImageDraw.Draw(im)
    big = ImageFont.truetype(font_path, 60, index=2)
    d.text((342, 90), "这一头朝上", font=big, fill=0, anchor="mm")
    d.text((342, 640), "竖着拿好", font=big, fill=0, anchor="mm")
    d.text((342, 720), "依次点 1 2 3 4", font=big, fill=0, anchor="mm")
    for i, (x, y) in enumerate(DOTS):
        d.ellipse((x - 70, y - 70, x + 70, y + 70), outline=0, width=6)
        d.text((x, y), str(i + 1), font=big, fill=0, anchor="mm")
    return im


font, out, pictures = sys.argv[1], sys.argv[2], [("dots", None)] + [p.split("=", 1) for p in sys.argv[3:]]
with open(out, "w") as f:
    f.write("#define INK_BENCH_IMAGES %d\n" % len(pictures))
    names = []
    for name, path in pictures:
        name = name.replace("-", "_")
        names.append(name)
        im = dots(font) if path is None else Image.open(path).convert("L").resize((684, 1216))
        px = im.tobytes()
        f.write(f"static const unsigned char IMG_{name}[] = {{")
        for i in range(0, len(px), 2):
            b = px[i] >> 4 << 4 | (px[i + 1] >> 4 if i + 1 < len(px) else 0)
            f.write(f"{b},")
        f.write("};\n")
    f.write("static const struct { const char *name; const unsigned char *data; } BENCH_IMAGES[] = {")
    f.write(",".join(f'{{"{n}", IMG_{n}}}' for n in names) + "};\n")
