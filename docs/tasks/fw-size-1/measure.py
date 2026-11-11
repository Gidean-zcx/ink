#!/usr/bin/env python3
"""Record linked image sizes without storing binaries or local paths."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def entries(path):
    data = path.read_bytes()
    magic, count, total = struct.unpack_from('<4sII', data)
    assert magic == b'INKA' and total == len(data)
    for i in range(count):
        name, offset, size = struct.unpack_from('<96sII', data, 12 + i * 104)
        assert offset + size <= total
        yield name.split(b'\0')[0].decode(), size, hashlib.sha256(data[offset:offset + size]).hexdigest()


def measure(build, label):
    result = {'label': label, 'firmware': (build / 'ink-s31.bin').stat().st_size,
              'font_data': (build / 'ink-fonts.bin').stat().st_size,
              'embedded': (build / 'ink-embedded.bin').stat().st_size}
    result['sha256'] = hashlib.sha256((build / 'ink-s31.bin').read_bytes()).hexdigest()
    # JSON avoids Rich wrapping every archive name at narrow terminal widths.
    python = re.search(r'^PYTHON:[^=]*=(.+)$', (build / 'CMakeCache.txt').read_text(), re.M)[1]
    report = json.loads(subprocess.check_output(
        [python, '-m', 'esp_idf_size', '--archives', '--format', 'json2', str(build / 'ink-s31.map')]))
    for row in report.values():
        name = row['abbrev_name']
        if name in ('libmesh.a', 'libesp_stdio.a', 'libink_lvgl.a', 'libink_miniz.a',
                    'libink_system.a', 'libink_apps.a', 'libink_reader_vendor.a', 'libink_platform.a'):
            result[name] = {'total': row['size'], 'flash': row['memory_types'].get('Flash', {}).get('size', 0)}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('label')
    args = parser.parse_args()
    folder = Path(__file__).resolve().parent
    result = measure(args.build, args.label)
    records = folder / 'sizes.json'
    previous = json.loads(records.read_text()) if records.exists() else []
    assert not any(row['label'] == args.label for row in previous), 'duplicate measurement'
    records.write_text(json.dumps(previous + [result], ensure_ascii=False, indent=2) + '\n')
    if args.label in ('baseline', 'final'):
        with (folder / f'assets-{args.label}.csv').open('w') as out:
            writer = csv.writer(out)
            writer.writerow(('pack', 'name', 'payload_bytes', 'sha256'))
            for pack in ('fonts', 'embedded'):
                for name, size, digest in entries(args.build / f'ink-{pack}.bin'):
                    writer.writerow((pack, name, size, digest))
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
