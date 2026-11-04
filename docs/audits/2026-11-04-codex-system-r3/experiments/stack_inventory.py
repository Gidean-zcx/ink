#!/usr/bin/env python3
"""Compile existing product sources to audit-only objects with host stack-usage data."""
from pathlib import Path
import re
import shlex
import subprocess

root = Path(__file__).resolve().parents[4]
build = root / 'build-audit'
out = build / 'audit-stack'
out.mkdir(exist_ok=True)
for target in ['ink_system', 'ink_apps']:
    d = build / ('CMakeFiles/' + target + '.dir')
    make = (d / 'flags.make').read_text()
    flags = []
    for name in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
        flags += shlex.split(re.search('^' + name + r' = (.*)$', make, re.M).group(1))
    for obj in sorted(d.rglob('*.c.o')):
        source = root / str(obj.relative_to(d))[:-2]
        dest = out / (str(obj.relative_to(d)).replace('/', '_'))
        subprocess.run(['cc', *flags, '-fstack-usage', '-c', str(source), '-o', str(dest)], check=True)
rows = []
for p in sorted(out.glob('*.su')):
    for line in p.read_text().splitlines():
        parts = line.split('\t')
        if len(parts) == 3 and int(parts[1]) >= 512:
            rows.append((int(parts[1]), parts[0], parts[2]))
for size, fn, kind in sorted(rows, reverse=True):
    print(size, kind, fn)
