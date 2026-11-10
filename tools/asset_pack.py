#!/usr/bin/env python3
"""Pack the same generated resources for font_data and the application image."""
import argparse
from pathlib import Path
import struct

FONT_LIMIT = 4608 * 1024
APP_LIMIT = 5 * 1024 * 1024
EMBEDDED_LIMIT = 1536 * 1024  # Preserve the existing 6 MiB total resource ceiling.
HEADER = struct.Struct('<4sII')
ENTRY = struct.Struct('<96sII')


def pack(items):
    offset = HEADER.size + ENTRY.size * len(items)
    table, payload = bytearray(), bytearray()
    for name, data in items:
        key = name.encode('utf-8')
        if len(key) >= 96 or b'\0' in key:
            raise ValueError(f'asset name exceeds 95 bytes: {name}')
        table += ENTRY.pack(key, offset, len(data))
        payload += data
        offset += len(data)
    return HEADER.pack(b'INKA', len(items), offset) + table + payload


def split(assets, font_limit=FONT_LIMIT, embedded_limit=EMBEDDED_LIMIT):
    files = sorted([*assets.glob('fonts/*.inkf'), *assets.glob('icons/*.inki')])
    if not files:
        raise ValueError('no generated fonts/icons; build host assets first')
    groups, used, second = [[], []], HEADER.size, False
    for path in files:
        data = path.read_bytes()
        cost = ENTRY.size + len(data)
        if used + cost > font_limit:
            second = True
        group = 1 if second else 0
        groups[group].append((path.relative_to(assets).as_posix(), data))
        if not second:
            used += cost
    result = tuple(pack(group) for group in groups)
    for label, data, limit in zip(('font_data', 'embedded'), result, (font_limit, embedded_limit)):
        if len(data) > limit:
            raise ValueError(f'{label}: {len(data)} bytes exceed {limit} by {len(data) - limit} bytes')
    return result


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
    except ValueError as error:
        ap.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
