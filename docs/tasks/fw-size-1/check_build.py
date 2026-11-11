#!/usr/bin/env python3
"""Check the real S31 compile database: one inflate and unchanged driver optimization."""
import argparse
import json
from pathlib import Path
import re
import shlex

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
parser.add_argument('--before', type=Path)
parser.add_argument('--expect-os', action='store_true')
args = parser.parse_args()
rows = json.loads((args.build / 'compile_commands.json').read_text())
miniz = [r for r in rows if r['file'].endswith('/reader_miniz/miniz.c')]
assert len(miniz) == 1, f'miniz compiled {len(miniz)} times, expected one'
assert not any('/ap_transfer/zlib/' in r['file'] for r in rows), 'upload zlib still compiled'
assert 'engine_alloc.h' in miniz[0]['command'], 'reader allocator connection lost'


def optimization(row):
    return [a for a in shlex.split(row['command']) if re.fullmatch(r'-O[0-3sgz]|-Ofast', a)]


checked = 0
if args.before:
    before = {str(Path(r['file']).resolve()): r for r in json.loads(args.before.read_text())}
    for row in rows:
        if (any(marker in row['command'] for marker in ('__idf_ink_drivers.dir/', '__idf_ink_platform.dir/', '__idf_main.dir/'))
                or row['file'].endswith('/system/ui/app_packages.c')):
            old = before.get(str(Path(row['file']).resolve()))
            assert old and optimization(row) == optimization(old), f'driver/platform/main flags changed: {Path(row["file"]).name}'
            checked += 1
if args.expect_os:
    counts = {'lvgl': 0, 'apps': 0, 'ui': 0}
    for row in rows:
        source = str(Path(row['file']).resolve())
        group = ('lvgl' if '__idf_ink_lvgl.dir/' in row['command'] else
                 'apps' if '__idf_ink_apps.dir/' in row['command'] else
                 'ui' if ('/system/ui/' in source or '/system/apps/' in source)
                 and not source.endswith('/app_packages.c') else None)
        if group:
            assert optimization(row)[-1] == '-Os', f'missing -Os: {Path(source).name}'
            counts[group] += 1
    assert all(counts.values()), counts
    print('optimized source counts:', counts)
print(f'S31 build contract: one miniz, zero upload zlib, {checked} unchanged optimization entries')
