"""Validate a verified full backup and print commands; never access a device."""
import hashlib
import os
from pathlib import Path
import shlex
import struct
import sys
import zlib
ROOT = Path(__file__).resolve().parents[2]
LAYOUT = {'otadata': (0xd000, 8192), 'model': (0x10000, 409600),
          'ota_0': (0x80000, 5242880), 'ota_1': (0x580000, 5242880),
          'resources': (0xa80000, 409600), 'font_data': (0xae4000, 5242880)}
def plan(mode):
    backup = Path(os.environ['INK_FLASH_BACKUP'])
    digest = os.environ['INK_FLASH_BACKUP_SHA256'].lower()
    image = backup.read_bytes()
    if len(image) != 16 * 1024 * 1024 or hashlib.sha256(image).hexdigest() != digest:
        raise ValueError('full 16 MiB backup size/SHA256 mismatch; use the independently verified digest')
    partitions = {}
    for off in range(0x8000, 0x8c00, 32):
        magic, _, _, address, size, label, _ = struct.unpack_from('<HBBII16sI', image, off)
        if magic == 0x50aa:
            partitions[label.rstrip(b'\0').decode()] = (address, size)
    if any(partitions.get(k) != v for k, v in LAYOUT.items()):
        raise ValueError('backup partition table differs from the fixed trial layout')
    out = ROOT / 'build-s31' / (mode + '-parts')
    out.mkdir(parents=True, exist_ok=True)
    pieces = []
    if mode == 'trial':
        for name, filename, limit in [('ota_1', 'ink-s31.bin', 5242880), ('font_data', 'ink-fonts.bin', 4718592)]:
            data = (ROOT / 'build-s31' / filename).read_bytes()
            if not data or len(data) > limit:
                raise ValueError(f'{filename}: {len(data)} bytes; limit {limit}; excess {max(0, len(data) - limit)}')
            pieces.append((name, data))
        seq = struct.pack('<I', 2)  # (sequence - 1) % 2 selects ota_1.
        pieces.append(('otadata', seq + b'\xff' * 24 + struct.pack('<I', zlib.crc32(seq, 0xffffffff)) + b'\xff' * (8192 - 32)))
    elif mode == 'restore':
        pieces = [(name, image[LAYOUT[name][0]:sum(LAYOUT[name])]) for name in ('model', 'font_data', 'otadata')]
    else:
        raise ValueError('mode must be trial or restore')
    commands = ['#!/usr/bin/env bash', 'set -euo pipefail', ': "${INK_FLASH_PORT:?set the reviewed device port}"']
    for name, data in pieces:
        target = out / (name + '.bin')
        target.write_bytes(data)
        q = shlex.quote(str(target))
        address = hex(LAYOUT[name][0])
        prefix = 'python3 -m esptool --chip esp32s31 --port "${INK_FLASH_PORT}" --after no-reset '
        commands += [prefix + f'write-flash {address} {q}', prefix + f'verify-flash {address} {q}']
    print('\n'.join(commands))
if __name__ == '__main__':
    try:
        plan(sys.argv[1])
    except (KeyError, OSError, ValueError) as error:
        sys.exit(str(error))
