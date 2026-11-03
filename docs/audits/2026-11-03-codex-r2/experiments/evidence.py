#!/usr/bin/env python3
"""Collect compact, reproducible read-only evidence; runtime files stay in build-audit."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
UPSTREAM = Path('/Users/bytedance/Documents/Workspace/ink-hw/upstream/Metalio-E-INK4-Plus')
table_paths = [ROOT / 'drivers/gc2145/gc2145_vga_table.h',
               UPSTREAM / 'main/boards/common/camera/gc2145_vga_settings.h']
tables = []
for p in table_paths:
    s = re.sub(r'/\*.*?\*/|//[^\n]*', '', p.read_text(), flags=re.S)
    pairs = [(int(a, 16), int(b, 16)) for a, b in
             re.findall(r'\{\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*\}', s)]
    tables.append(pairs)
cmd = ['cc', '-O2', '-std=gnu11', '-Iplatform/include', '-Isim', '-Isim/vtk',
       '-Isim/mem', '-Ithird_party/tlsf', str(HERE / 'memory_probe.c'),
       'third_party/tlsf/tlsf.c', '-o', 'build-audit/r2-memory-probe']
build = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
assert build.returncode == 0, build.stderr
mem = subprocess.run(['build-audit/r2-memory-probe'], cwd=ROOT, capture_output=True, text=True)
from PIL import Image
p = ROOT / 'build-audit/r2-probes/r2_display_cut'
a, b = (Image.open(p / ('snap-' + x + '.png')).convert('RGB') for x in ['before', 'after'])
diff = sum(x != y for x, y in zip(a.getdata(), b.getdata()))
result = {
    'table_compare': {
        'files': [str(p) for p in table_paths],
        'pairs': [len(t) for t in tables], 'equal': tables[0] == tables[1],
        'sha256_register_value_bytes': [hashlib.sha256(bytes(v for pair in t for v in pair)).hexdigest() for t in tables]
    },
    'memory': {'build_command': cmd, 'build_exit': build.returncode, 'exit_code': mem.returncode,
               'output': mem.stdout.splitlines()},
    'panel_cut': {'size': a.size, 'changed_pixels': diff,
                  'before_0_0': a.getpixel((0, 0)), 'after_0_0': b.getpixel((0, 0))}
}
(HERE / 'driver-evidence.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(result, ensure_ascii=False, indent=2))
