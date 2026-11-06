#!/usr/bin/env python3
"""Pictures for the panel tests, 4 bits a pixel, packed. The output stays in the build directory.

gen_images.py [--lab] font.ttc out.h name=picture.png ...
The first picture is drawn here: numbered dots to tap, at places the report can compare with.
--lab adds what the refresh test shows: the wedge, reading pages, plain white, the last picture.
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

W, H = 684, 1216
DOTS = ((110, 300), (574, 300), (574, 1000), (110, 1000))
# The wedge, on the logical screen. Written into the header too, so the firmware pushes exactly these patches.
WEDGE = dict(WEDGE_COLS=6, WEDGE_ROWS=8, WEDGE_X0=30, WEDGE_Y0=118, WEDGE_PATCH=84, WEDGE_PITCH_X=106, WEDGE_PITCH_Y=124)
STRIP_Y, STRIP_H = 1120, 52
MARK = 22  # corner marks, for finding the screen in a photo


def font_at(path, px):
    return ImageFont.truetype(path, px, index=2)


def dots(font):
    im = Image.new("L", (W, H), 255)
    d = ImageDraw.Draw(im)
    big = font_at(font, 60)
    d.text((342, 90), "这一头朝上", font=big, fill=0, anchor="mm")
    d.text((342, 640), "竖着拿好", font=big, fill=0, anchor="mm")
    d.text((342, 720), "依次点 1 2 3 4", font=big, fill=0, anchor="mm")
    for i, (x, y) in enumerate(DOTS):
        d.ellipse((x - 70, y - 70, x + 70, y + 70), outline=0, width=6)
        d.text((x, y), str(i + 1), font=big, fill=0, anchor="mm")
    return im


def wedge(font):
    """Patches start white (upper half) or black (lower half); the firmware pushes them n times."""
    im = Image.new("L", (W, H), 255)
    d = ImageDraw.Draw(im)
    w = WEDGE
    title, small = font_at(font, 36), font_at(font, 26)
    d.text((W // 2, 52), "拍一张照，拍好后点一下屏幕", font=title, fill=0, anchor="mm")
    half = w["WEDGE_ROWS"] // 2
    for r in range(w["WEDGE_ROWS"]):
        for c in range(w["WEDGE_COLS"]):
            x, y = w["WEDGE_X0"] + c * w["WEDGE_PITCH_X"], w["WEDGE_Y0"] + r * w["WEDGE_PITCH_Y"]
            n = (r % half) * w["WEDGE_COLS"] + c + 1
            d.text((x + w["WEDGE_PATCH"] // 2, y - 17), str(n), font=small, fill=0, anchor="mm")
            d.rectangle((x, y, x + w["WEDGE_PATCH"] - 1, y + w["WEDGE_PATCH"] - 1), fill=255 if r < half else 0,
                        outline=None if r >= half else 150)
    for level in range(16):  # the factory's own levels, to read the patches against
        x = 22 + level * 40
        d.rectangle((x, STRIP_Y, x + 39, STRIP_Y + STRIP_H - 1), fill=level * 17)
    for x, y in ((6, 6), (W - 6 - MARK, 6), (6, H - 6 - MARK), (W - 6 - MARK, H - 6 - MARK)):
        d.rectangle((x, y, x + MARK - 1, y + MARK - 1), fill=0)
    return im


def reading_pages(font, book, count=4):
    """Text as the reader lays it out: 44 px, with the page's margins."""
    face = font_at(font, 44)
    text = open(book, encoding="utf-8").read().replace("\r", "")
    pages, at = [], 0
    for _ in range(count):
        im = Image.new("L", (W, H), 255)
        d = ImageDraw.Draw(im)
        y = 70
        while y < H - 110 and at < len(text):
            x = 38
            while at < len(text):
                ch = text[at]
                at += 1
                if ch == "\n":
                    break
                if x + 44 > W - 38:
                    at -= 1
                    break
                d.text((x, y), ch, font=face, fill=0)
                x += int(face.getlength(ch))
            y += 66
        pages.append(im)
    return pages


def pack(data):
    out, i, n = bytearray(), 0, len(data)
    while i < n:
        same = 1
        while i + same < n and data[i + same] == data[i] and same < 130:
            same += 1
        if same >= 3:
            out += bytes((same + 125, data[i]))
            i += same
            continue
        k = i
        while k < n and k - i < 128 and not (k + 2 < n and data[k] == data[k + 1] == data[k + 2]):
            k += 1
        out.append(k - i - 1)
        out += data[i:k]
        i = k
    return bytes(out)


def main():
    args = sys.argv[1:]
    lab = "--lab" in args
    if lab:
        args.remove("--lab")
    font, out, wanted = args[0], args[1], [a.split("=", 1) for a in args[2:]]
    pictures = [("dots", dots(font))]
    for name, path in wanted:
        if not lab or name in ("Main", "Settings", "Today"):
            pictures.append((name, Image.open(path).convert("L").resize((W, H))))
    if lab:
        book = os.path.join(os.path.dirname(out), "..", "..", "..", "build", "test-content", "books", "星星邮差.txt")
        pictures.append(("wedge", wedge(font)))
        pictures += [(f"read{i + 1}", page) for i, page in enumerate(reading_pages(font, book))]
        pictures.append(("white", Image.new("L", (W, H), 255)))
        done = Image.new("L", (W, H), 255)
        ImageDraw.Draw(done).text((W // 2, H // 2), "测完了，马上恢复", font=font_at(font, 60), fill=0, anchor="mm")
        pictures.append(("done", done))
    with open(out, "w") as f:
        f.write("#define INK_BENCH_IMAGES %d\n" % len(pictures))
        for k, v in WEDGE.items():
            f.write(f"#define {k} {v}\n")
        names = []
        for name, im in pictures:
            name = name.replace("-", "_")
            names.append(name)
            px = im.convert("L").tobytes()
            nibbles = bytes(px[i] & 0xF0 | (px[i + 1] >> 4 if i + 1 < len(px) else 0) for i in range(0, len(px), 2))
            data = pack(nibbles)
            f.write(f"static const unsigned char IMG_{name}[] = {{")
            f.write(",".join(map(str, data)))
            f.write("};\n")
        f.write("static const struct { const char *name; const unsigned char *data; unsigned size; } BENCH_IMAGES[] = {")
        f.write(",".join(f'{{"{n}", IMG_{n}, sizeof IMG_{n}}}' for n in names) + "};\n")


main()
