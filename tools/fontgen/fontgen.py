#!/usr/bin/env python3
"""
fontgen — render a TrueType/OpenType font into ink's bitmap font format (INKF).

  python3 tools/fontgen/fontgen.py --font NotoSansCJK-Regular.ttc --face 2 \
      --px 44 --bpp 2 --charset gb2312,ascii,punct,pinyin -o build/assets/fonts/sans-44.inkf

INKF (little-endian):
  header (40 B): "INKF", u16 version, u8 bpp, u8 flags, u16 px, i16 ascent,
                 i16 descent, u16 line_height, u32 n_glyphs, u32 index_off,
                 u32 bitmap_off, u32 bitmap_size, u32 reserved[2]
  index  (16 B per glyph, sorted by codepoint):
                 u32 codepoint, u32 bitmap_offset, u8 w, u8 h, i8 x_ofs,
                 i8 y_ofs (top of the bitmap relative to the baseline, negative = above),
                 u8 advance, then v1: u8 reserved[3]; v2: u16 stored bytes, u8 reserved
  v1 bitmaps: rows packed MSB-first at `bpp` bits per pixel, each row padded to a byte.
  v2 (2 bpp, written for every 2 bpp font): each glyph is compressed on its own and the font stays
     mapped in flash; system/text/ink_font.c unpacks a glyph row by row while drawing it. The pixels,
     row after row with no padding, as runs of one value written in nibbles, high nibble first:
     vvnn = nn pixels (1-3) of value vv; nn = 0: 4 pixels plus the nibbles that follow, up to and
     including the first one below 15. A last odd nibble is padding.

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
    [0x00A0, 0x00A9, 0x00AE, 0x00B0, 0x00B7, 0x00D7, 0x00F7, 0x25B6, 0x25C0]
)
PINYIN = [ord(c) for c in "āáǎàēéěèīíǐìōóǒòūúǔùǖǘǚǜüĀÁǍÀĒÉĚÈĪÍǏÌŌÓǑÒŪÚǓÙǕǗǙǛÜ"]


def font_coverage(path, face=0):
    """Read cmap formats 4/12 without an extra Python dependency (TTF/OTF/TTC)."""
    data = open(path, 'rb').read()
    u16 = lambda at: struct.unpack_from('>H', data, at)[0]
    u32 = lambda at: struct.unpack_from('>I', data, at)[0]
    base = u32(12 + face * 4) if data[:4] == b'ttcf' else 0
    tables = {data[at:at + 4]: u32(at + 8) for at in range(base + 12, base + 12 + u16(base + 4) * 16, 16)}
    cmap = tables[b'cmap']
    result = set()
    for i in range(u16(cmap + 2)):
        at = cmap + 4 + 8 * i
        platform, encoding = u16(at), u16(at + 2)
        if platform != 0 and not (platform == 3 and encoding in (1, 10)):
            continue
        start = cmap + u32(at + 4)
        fmt = u16(start)
        if fmt == 12:
            for j in range(u32(start + 12)):
                lo, hi, gid = struct.unpack_from('>III', data, start + 16 + 12 * j)
                result.update(range(lo + (gid == 0), hi + 1))
        elif fmt == 4:
            count = u16(start + 6) // 2
            ends = start + 14
            starts = ends + 2 * count + 2
            deltas = starts + 2 * count
            offsets = deltas + 2 * count
            for j in range(count):
                lo, hi = u16(starts + 2 * j), u16(ends + 2 * j)
                delta, offset = u16(deltas + 2 * j), u16(offsets + 2 * j)
                for cp in range(lo, min(hi, 0xfffe) + 1):
                    gid = u16(offsets + 2 * j + offset + 2 * (cp - lo)) if offset else cp
                    if (gid + delta) & 0xffff:
                        result.add(cp)
    return result


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
        elif part == "digits":
            cps.update(ord(c) for c in "0123456789:./%+- ?")
        elif part.startswith("file:"):
            with open(part[5:], encoding="utf-8") as f:
                cps.update(ord(c) for c in f.read() if ord(c) >= 0x20)
        elif part:
            raise SystemExit(f"unknown charset part: {part}")
    return sorted(cps)


def quantize(img, bpp):
    """Pixel values (0 .. 2^bpp-1), row by row."""
    levels = (1 << bpp) - 1
    w, h = img.size
    px = img.load()
    return [[(px[x, y] * levels + 127) // 255 for x in range(w)] for y in range(h)]


def packed(rows, bpp):
    """v1: rows MSB-first, each padded to a byte."""
    out = bytearray()
    for row in rows:
        acc = n = 0
        for q in row:
            acc, n = (acc << bpp) | q, n + bpp
            if n == 8:
                out.append(acc)
                acc = n = 0
        if n:
            out.append(acc << (8 - n))
    return bytes(out)


def runs(rows):
    """v2: the runs of equal 2 bpp values over all rows, in nibbles (see the module notes)."""
    pixels = [q for row in rows for q in row]
    nibbles, i = [], 0
    while i < len(pixels):
        j = i
        while j < len(pixels) and pixels[j] == pixels[i]:
            j += 1
        value, n = pixels[i], j - i
        if n < 4:
            nibbles.append(value << 2 | n)
        else:
            nibbles.append(value << 2)
            n -= 4
            while n >= 15:
                nibbles.append(15)
                n -= 15
            nibbles.append(n)
        i = j
    nibbles += [0] * (len(nibbles) % 2)
    return bytes(nibbles[k] << 4 | nibbles[k + 1] for k in range(0, len(nibbles), 2))


def inkf(version, bpp, px, ascent, descent, line_h, glyphs):
    """The file: glyphs are (cp, w, h, x_ofs, y_ofs, adv, rows)."""
    index, bitmaps = [], bytearray()
    for cp, w, h, x_ofs, y_ofs, adv, rows in glyphs:
        data = (runs(rows) if version == 2 else packed(rows, bpp)) if w and h else b""
        if version == 2:
            index.append(struct.pack("<IIBBbbBHx", cp, len(bitmaps), w, h, x_ofs, y_ofs, adv, len(data)))
        else:
            index.append(struct.pack("<IIBBbbB3x", cp, len(bitmaps), w, h, x_ofs, y_ofs, adv))
        bitmaps += data
    hdr_size = 40
    bitmap_off = hdr_size + 16 * len(index)
    header = struct.pack("<4sHBBHhhHIIII8x", b"INKF", version, bpp, 0, px, ascent, descent, line_h, len(index),
                         hdr_size, bitmap_off, len(bitmaps))
    assert len(header) == hdr_size
    return header + b"".join(index) + bytes(bitmaps)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--font", required=True)
    ap.add_argument("--face", type=int, default=0, help="face index in a .ttc (Noto Sans CJK: 2 = SC)")
    ap.add_argument("--px", type=int, required=True)
    ap.add_argument("--bpp", type=int, default=2, choices=[1, 2, 4, 8])
    ap.add_argument("--charset", default="ascii,punct,gb2312")
    ap.add_argument("--line-height", type=float, default=1.0, help="multiple of the font's natural line height")
    ap.add_argument("--weight", type=int, help="variable font weight")
    ap.add_argument("--digits-font", help="Outfit font used for ASCII digits")
    ap.add_argument("--digits-weight", type=int, default=400)
    ap.add_argument("--exact-line", type=int, help="contract line height in pixels")
    ap.add_argument("--require-coverage", action="store_true", help="fail if a requested codepoint is absent")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--raw-out", help="also write the uncompressed v1 file (the reference the tests unpack against)")
    a = ap.parse_args()

    font = ImageFont.truetype(a.font, a.px, index=a.face)
    if a.weight:
        font.set_variation_by_axes([a.weight])
    digit_font = ImageFont.truetype(a.digits_font, a.px) if a.digits_font else None
    if digit_font:
        digit_font.set_variation_by_axes([a.digits_weight])
    ascent, descent = font.getmetrics()
    line_h = a.exact_line or int(round((ascent + descent) * a.line_height))
    coverage = font_coverage(a.font, a.face) if a.require_coverage else None
    glyphs = []
    missing = 0
    for cp in charset(a.charset):
        ch = chr(cp)
        f = digit_font if digit_font and ch in "0123456789" else font
        if (coverage is not None and cp not in coverage) or (cp != 0x20 and cp != 0x3000 and not f.getmask(ch).getbbox()):
            missing += 1
            continue
        adv = int(round(f.getlength(ch)))
        x0, y0, x1, y1 = f.getbbox(ch, anchor="ls")
        w, h = max(0, x1 - x0), max(0, y1 - y0)
        if w > 255 or h > 255 or adv > 255:
            raise SystemExit(f"glyph U+{cp:04X} too large for INKF ({w}x{h}, adv {adv})")
        rows = []
        if w and h:
            img = Image.new("L", (w, h), 0)
            ImageDraw.Draw(img).text((-x0, -y0), ch, font=f, fill=255, anchor="ls")
            rows = quantize(img, a.bpp)
        glyphs.append((cp, w, h, max(-128, min(127, x0)), max(-128, min(127, y0)), adv, rows))
    if a.require_coverage and missing:
        raise SystemExit(f"fontgen: {missing} requested glyphs missing from {a.font}; refusing an incomplete UI font")
    raw = inkf(1, a.bpp, a.px, ascent, descent, line_h, glyphs)
    data = inkf(2, a.bpp, a.px, ascent, descent, line_h, glyphs) if a.bpp == 2 else raw  # v2 is 2 bpp only
    with open(a.out, "wb") as f:
        f.write(data)
    if a.raw_out:
        with open(a.raw_out, "wb") as f:
            f.write(raw)
    print(f"fontgen: {a.out}: {len(glyphs)} glyphs ({missing} missing in font), {a.px}px {a.bpp}bpp, "
          f"{len(data) / 1024:.0f} KiB (uncompressed {len(raw) / 1024:.0f} KiB)", file=sys.stderr)

if __name__ == "__main__":
    main()
