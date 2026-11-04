#!/usr/bin/env python3
"""Host Mach-O BSS symbol spans; not a target linker map or simulated heap accounting."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[4]
build = root / 'build-audit'
for target in ['ink_system', 'ink_apps']:
    d = build / ('CMakeFiles/' + target + '.dir')
    for obj in sorted(d.rglob('*.c.o')):
        layout = subprocess.check_output(['xcrun', 'otool', '-l', str(obj)], text=True)
        match = re.search(r'sectname __bss\s+segname __DATA\s+addr (0x[0-9a-f]+)\s+size (0x[0-9a-f]+)', layout)
        if not match:
            continue
        end = int(match[1], 16) + int(match[2], 16)
        nm = subprocess.check_output(['xcrun', 'nm', '-nm', str(obj)], text=True)
        syms = [(int(m[1], 16), m[2]) for m in re.finditer(r'^([0-9a-f]+) \(__DATA,__bss\).* (_\S+)$', nm, re.M)]
        for i, (addr, name) in enumerate(syms):
            span = (syms[i+1][0] if i+1 < len(syms) else end) - addr
            if span >= 512:
                print(str(obj.relative_to(d))[:-2], name, span)
