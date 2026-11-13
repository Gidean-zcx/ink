#!/usr/bin/env python3
"""Pack the same generated resources for font_data and the application image."""
import argparse
from pathlib import Path
import struct
import zlib

FONT_LIMIT = 4608 * 1024
APP_LIMIT = 5 * 1024 * 1024
# 2 MiB: the pinyin word list (about 0.95 MiB) went in 2026-11-11. ota_1's 5 MiB (APP_LIMIT, checked on the
# linked image) is the hard limit; this one only catches a pack growing by mistake.
EMBEDDED_LIMIT = 2048 * 1024
HEADER = struct.Struct('<4sII')
ENTRY = struct.Struct('<96sII')


def pack(items, compress=False):
    offset = HEADER.size + ENTRY.size * len(items)
    table, payload = bytearray(), bytearray()
    for name, data in items:
        key = name.encode('utf-8')
        if len(key) >= 96 or b'\0' in key:
            raise ValueError(f'asset name exceeds 95 bytes: {name}')
        stored = len(data)
        if compress and not name.startswith('fonts/'):  # a font stays mapped for good: inflated, it would hold PSRAM
            zipped = struct.pack('<I', len(data)) + zlib.compress(data, 9)
            if len(zipped) < len(data):
                data, stored = zipped, len(zipped) | 0x80000000
        table += ENTRY.pack(key, offset, stored)
        payload += data
        offset += len(data)
    return HEADER.pack(b'INKA', len(items), offset) + table + payload


def split(assets, font_limit=FONT_LIMIT, embedded_limit=EMBEDDED_LIMIT):
    files = sorted([*assets.glob('fonts/*.inkf'), *assets.glob('icons/*.inki'), *assets.glob('ime/*'),
                    *assets.glob('sample/*.txt')])  # ime/: the pinyin word list and its licences
    # Only these names use INK_ICON_STATE (106 px); keep both looks and all other sizes.
    state_icons = set('battery check clock cloud cloudoff cloudsun download family globe lock mic '
                      'person rain sd search snow sun undo usb warn wave wifioff'.split())
    files = [p for p in files if p.parent.name != 'icons' or not p.name.endswith('-106.inki')
             or p.name.rsplit('-', 2)[0] in state_icons]
    if not files:
        raise ValueError('no generated fonts/icons; build host assets first')
    # font_data holds the fonts: each font that fits goes in, also after one that did not (one big face no longer
    # pushes all the others into the image). Icons and the word list stay in the image, compressed, where they
    # were: a new image then still finds them while font_data holds an older pack (upgrades write it afterwards).
    groups, used = [[], []], HEADER.size
    for path in files:
        data = path.read_bytes()
        cost = ENTRY.size + len(data)
        first = path.parent.name == 'fonts' and used + cost <= font_limit
        groups[0 if first else 1].append((path.relative_to(assets).as_posix(), data))
        used += cost if first else 0
    result = tuple(pack(group, compress=(i == 1)) for i, group in enumerate(groups))
    for label, data, limit in zip(('font_data', 'embedded'), result, (font_limit, embedded_limit)):
        if len(data) > limit:
            raise ValueError(f'{label}: {len(data)} bytes exceed {limit} by {len(data) - limit} bytes')
    return result


def entries(data):
    """(name, stored bytes) of each entry of a pack."""
    count = HEADER.unpack_from(data)[1]
    return [(key.rstrip(b'\0').decode('utf-8'), stored & 0x7fffffff)
            for key, _, stored in (ENTRY.unpack_from(data, HEADER.size + i * ENTRY.size) for i in range(count))]


def spilled(embedded):
    """What the build says when fonts went into the app image: which ones and how big."""
    fonts = [f'{name} {size} bytes' for name, size in entries(embedded) if name.startswith('fonts/')]
    return 'fonts that did not fit in font_data, now in the app image: ' + ', '.join(fonts) if fonts else ''


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('assets', type=Path)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--firmware', type=Path)
    args = ap.parse_args()
    try:
        if args.firmware:
            size = args.firmware.stat().st_size
            if size > APP_LIMIT:
                raise ValueError(f'ota_1: {size} bytes exceed {APP_LIMIT} by {size - APP_LIMIT} bytes')
        else:
            first, second = split(args.assets)
            args.out.mkdir(parents=True, exist_ok=True)
            (args.out / 'ink-fonts.bin').write_bytes(first)
            (args.out / 'ink-embedded.bin').write_bytes(second)
            print(f'font_data {len(first)}/{FONT_LIMIT}; embedded {len(second)}/{EMBEDDED_LIMIT} bytes')
            if spilled(second):
                print(spilled(second))
    except ValueError as error:
        ap.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
