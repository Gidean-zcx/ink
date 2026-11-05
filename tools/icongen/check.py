#!/usr/bin/env python3
"""Check extracted geometry and every icon required by a portable UI source."""
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
folder = ROOT / 'assets/icons'
manifest = json.loads((folder / 'manifest.json').read_text())
errors = []
for name, digest in manifest['files'].items():
    file = folder / name
    if not file.exists() or hashlib.sha256(file.read_bytes()).hexdigest() != digest:
        errors.append(f'missing or changed geometry: {name}')
for source in (ROOT / 'system/ui').rglob('*.c'):
    for name in re.findall(r'ink_ui_(?:icon|circle)\([^,\n]+,\s*"([A-Za-z]+)"', source.read_text()):
        if name not in manifest['names']: errors.append(f'{source.name}: unknown required icon {name}')
for message in errors: print(message, file=sys.stderr)
print(f'icons: {len(manifest["names"])} names, {len(manifest["files"])} SVG files, {len(errors)} errors')
sys.exit(bool(errors))
