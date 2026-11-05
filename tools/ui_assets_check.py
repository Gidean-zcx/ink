#!/usr/bin/env python3
"""Validate generated font families, line heights, digit subsets and icon formats."""
import argparse
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent / 'fontgen'))
from ui_fonts import SIZES


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('assets', type=Path)
    a = ap.parse_args()
    for weight in range(3):
        for px, line in SIZES.items():
            p = a.assets / 'fonts' / f'{"outfit" if px >= 88 else "ui"}-{weight}-{px}.inkf'
            data = p.read_bytes()
            assert data[:4] == b'INKF' and struct.unpack_from('<H', data, 8)[0] == px, p
            assert struct.unpack_from('<H', data, 14)[0] == line, p
            count, start = struct.unpack_from('<II', data, 16)
            cps = [struct.unpack_from('<I', data, start + i * 16)[0] for i in range(count)]
            assert all(ord(c) in cps for c in '0123456789'), p
            if px >= 88: assert set(cps) <= set(map(ord, '0123456789:./%+- ?')), p
            assert len(data) <= 5 * 1024 * 1024, p
    for px in (28, 36, 44, 64, 88):
        for look in ('line', 'duo', 'solid', 'solid-inv'):
            p = a.assets / 'icons' / f'clock-{look}-{px}.inki'
            data = p.read_bytes()
            assert data[:4] == b'INKI' and len(data) == 8 + px * px * 4, p
            assert struct.unpack_from('<HH', data, 4) == (px, px), p
    total = sum(p.stat().st_size for p in (a.assets / 'fonts').glob('*.inkf'))
    print(f'font/icon structure OK; fonts {total} bytes; SP-05 target {4 * 1024 * 1024} bytes')


if __name__ == '__main__': main()
