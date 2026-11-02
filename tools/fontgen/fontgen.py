#!/usr/bin/env python3
"""
fontgen — render a TrueType/OpenType font into ink's bitmap font format (INKF v1).

  python3 tools/fontgen/fontgen.py --font NotoSansCJK-Regular.ttc --face 2 \
      --px 32 --bpp 2 --charset gb2312,ascii,punct,pinyin -o build/assets/fonts/sans-32.inkf

INKF v1 (little-endian):
  header (40 B): "INKF", u16 version=1, u8 bpp, u8 flags, u16 px, i16 ascent,
                 i16 descent, u16 line_height, u32 n_glyphs, u32 index_off,
                 u32 bitmap_off, u32 bitmap_size, u32 reserved[2]
  index  (16 B per glyph, sorted by codepoint):
                 u32 codepoint, u32 bitmap_offset, u8 w, u8 h, i8 x_ofs,
                 i8 y_ofs (top of the bitmap relative to the baseline, negative = above),
                 u8 advance, u8 reserved[3]
  bitmaps: rows packed MSB-first at `bpp` bits per pixel, each row padded to a byte.

The output is deterministic for a given font file and arguments.
Noto Sans CJK is licensed under the SIL Open Font License 1.1; bitmaps derived
from it must carry that license (see assets/fonts/README.md).
"""
import argparse
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

PUNCT = (
    list(range(0x2010, 0x2028)) + [0x2030, 0x2032, 0x2033, 0x203B, 0x2026] +
    list(range(0x3000, 0x3040)) +   # CJK symbols and punctuation
    list(range(0xFF01, 0xFF5F)) +   # fullwidth forms
    [0x00A0, 0x00A9, 0x00AE, 0x00B0, 0x00B7, 0x00D7, 0x00F7]
)
PINYIN = [ord(c) for c in "āáǎàēéěèīíǐìōóǒòūúǔùǖǘǚǜüĀÁǍÀĒÉĚÈĪÍǏÌŌÓǑÒŪÚǓÙǕǗǙǛÜ"]


def gb2312():
    out = []
    for hi in range(0xA1, 0xF8):
        for lo in range(0xA1, 0xFF):
            try:
                ch = bytes([hi, lo]).decode("gb2312")
            except UnicodeDecodeError:
                continue
            out.append(ord(ch))
    return out


def charset(spec):
    cps = set()
    for part in spec.split(","):
        part = part.strip()
        if part == "ascii":
            cps.update(range(0x20, 0x7F))
        elif part == "punct":
            cps.update(PUNCT)
        elif part == "pinyin":
            cps.update(PINYIN)
        elif part == "gb2312":
            cps.update(gb2312())
        elif part.startswith("file:"):
            with open(part[5:], encoding="utf-8") as f:
                cps.update(ord(c) for c in f.read() if ord(c) >= 0x20)
        elif part:
            raise SystemExit(f"unknown charset part: {part}")
    return sorted(cps)


def quantize(img, bpp):
    levels = (1 << bpp) - 1
    w, h = img.size
    px = img.load()
    rows = []
    for y in range(h):
        bits = []
        for x in range(w):
            v = px[x, y]
            q = (v * levels + 127) // 255
            bits.append(q)
        row = bytearray()
        acc = 0
        n = 0
        for q in bits:
            acc = (acc << bpp) | q
            n += bpp
            if n == 8:
                row.append(acc)
                acc = 0
                n = 0
        if n:
            row.append(acc << (8 - n))
        rows.append(bytes(row))
    return b"".join(rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--font", required=True)
    ap.add_argument("--face", type=int, default=0, help="face index in a .ttc (Noto Sans CJK: 2 = SC)")
    ap.add_argument("--px", type=int, required=True)
    ap.add_argument("--bpp", type=int, default=2, choices=[1, 2, 4, 8])
    ap.add_argument("--charset", default="ascii,punct,gb2312")
    ap.add_argument("--line-height", type=float, default=1.0, help="multiple of the font's natural line height")
    ap.add_argument("-o", "--out", required=True)
    a = ap.parse_args()

    font = ImageFont.truetype(a.font, a.px, index=a.face)
    ascent, descent = font.getmetrics()
    line_h = int(round((ascent + descent) * a.line_height))
    index = []
    bitmaps = bytearray()
    missing = 0
    for cp in charset(a.charset):
        ch = chr(cp)
        if cp != 0x20 and cp != 0x3000 and not font.getmask(ch).getbbox():
            missing += 1
            continue
        adv = int(round(font.getlength(ch)))
        x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
        w, h = max(0, x1 - x0), max(0, y1 - y0)
        if w > 255 or h > 255 or adv > 255:
            raise SystemExit(f"glyph U+{cp:04X} too large for INKF v1 ({w}x{h}, adv {adv})")
        off = len(bitmaps)
        if w and h:
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
            bitmaps += quantize(img, a.bpp)
        x_ofs = max(-128, min(127, x0))
        y_ofs = max(-128, min(127, y0))
        index.append(struct.pack("<IIBBbbB3x", cp, off, w, h, x_ofs, y_ofs, adv))
    hdr_size = 40
    index_off = hdr_size
    bitmap_off = index_off + 16 * len(index)
    header = struct.pack("<4sHBBHhhHIIII8x", b"INKF", 1, a.bpp, 0, a.px, ascent, descent, line_h, len(index),
                         index_off, bitmap_off, len(bitmaps))
    assert len(header) == hdr_size
    with open(a.out, "wb") as f:
        f.write(header)
        f.write(b"".join(index))
        f.write(bitmaps)
    total = bitmap_off + len(bitmaps)
    print(f"fontgen: {a.out}: {len(index)} glyphs ({missing} missing in font), {a.px}px {a.bpp}bpp, "
          f"{total / 1024:.0f} KiB", file=sys.stderr)


if __name__ == "__main__":
    main()
