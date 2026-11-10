#!/usr/bin/env python3
"""Validate generated font families, line heights, digit subsets and icon formats."""
import argparse
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent / 'fontgen'))
from ui_fonts import ACCENT_FACES, SIZES, TOKENS


from asset_pack import EMBEDDED_LIMIT, FONT_LIMIT, split


def unpacked(data):
    out, at = bytearray(), 8
    while at < len(data):
        code = data[at]
        if code < 128:
            out += data[at + 1:at + 2 + code]
            at += code + 2
        else:
            out += data[at + 1:at + 2] * (code - 125)
            at += 2
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('assets', type=Path)
    a = ap.parse_args()
    faces = [(0, 'sans', 44), *((w, 'ui', px) for w, px in ACCENT_FACES.items())]
    faces += [(w, 'outfit', px) for w in range(3) for px in SIZES if px >= 88]
    for weight, family, px in faces:
        p = a.assets / 'fonts' / (f'sans-{px}.inkf' if family == 'sans' else f'{family}-{weight}-{px}.inkf')
        data = p.read_bytes()
        assert data[:4] == b'INKF' and struct.unpack_from('<H', data, 8)[0] == px, p
        count, start = struct.unpack_from('<II', data, 16)
        cps = [struct.unpack_from('<I', data, start + i * 16)[0] for i in range(count)]
        assert all(ord(c) in cps for c in '0123456789'), p
        if family == 'outfit': assert set(cps) <= set(map(ord, '0123456789:./%+- ?')), p
    for px in (v for k, v in TOKENS.items() if k.startswith('INK_ICON_')):
        for look in ('line', 'solid'):
            p = a.assets / 'icons' / f'clock-{look}-{px}.inki'
            data = p.read_bytes()
            assert data[:4] == b'INKI' and struct.unpack_from('<HH', data, 4) == (px, px), p
            assert len(unpacked(data)) == px * px, p
    fonts = sum(p.stat().st_size for p in (a.assets / 'fonts').glob('*.inkf'))
    icons = sum(p.stat().st_size for p in (a.assets / 'icons').glob('*.inki'))
    first, second = split(a.assets)
    print(f'assets OK: fonts {fonts} + icons {icons}; font_data {len(first)}/{FONT_LIMIT}; embedded {len(second)}/{EMBEDDED_LIMIT}')



if __name__ == '__main__': main()
