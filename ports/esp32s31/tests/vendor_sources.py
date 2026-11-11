#!/usr/bin/env python3
"""Verify immutable vendor files; optionally count adaptation lines against a baseline."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
VENDOR = ROOT / 'ports/esp32s31/vendor/board_874538e'
PRODUCT = ['ports/esp32s31/' + f for f in ('s31_board.c', 'hal_i2c_s31.c', 'hal_input_s31.c', 's31_irq.c', 'hal_device_s31.c')]
PRODUCT += ['drivers/chips/' + p for p in ('tca9555/tca9555.c', 'sc7a20h/sc7a20h.c', 'sc7a20h/sc7a20h.h', 'ft6336u/ft6336u.c')]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--baseline')  # Manual check only: CI may have no history.
    args = ap.parse_args()
    entries = json.loads((VENDOR / 'sources.json').read_text())
    for entry in entries:
        digest = hashlib.sha256((VENDOR / entry['path']).read_bytes()).hexdigest()
        assert digest == entry['sha256'], entry['path']
    print(f'{len(entries)} vendor files: SHA-256 matches the unchanged upstream copies')
    if not args.baseline:
        return
    original = {line.strip() for entry in entries if entry['path'] != 'LICENSE'
                for line in (VENDOR / entry['path']).read_text().splitlines()}
    adaptation = 0
    for path in sorted(VENDOR.glob('*.inc')):
        count = sum(line.strip() not in original for line in path.read_text().splitlines()
                    if line.strip() and not line.lstrip().startswith(('/*', '//', '*')))
        adaptation += count
        print(f'{path.name}: {count} added/adapted non-original lines')
    diff = subprocess.check_output(['git', 'diff', '--numstat', args.baseline, '--', *PRODUCT], cwd=ROOT, text=True)
    additions = sum(int(line.split('\t')[0]) for line in diff.splitlines())
    print(f'port/driver added physical lines={additions}; vendor adaptation lines={adaptation}; glue={additions + adaptation}/350')
    assert additions + adaptation <= 350


if __name__ == '__main__':
    main()
