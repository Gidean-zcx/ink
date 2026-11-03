#!/usr/bin/env python3
"""Reproduce the old/new clock control or the known F-6 replay without product edits."""
import json
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
os.chdir(ROOT)

def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=55,
                       env={**os.environ, 'SDL_VIDEODRIVER': 'dummy'})
    return dict(command=cmd, exit_code=p.returncode, output=(p.stdout+p.stderr).splitlines())

if sys.argv[1] == 'clock':
    rows = []
    for old in (True, False):
        exe = 'build-audit/r8-clock-' + ('old' if old else 'new')
        cmd = ['cc', '-O2', '-std=c11', '-Idrivers/display/pqbn',
               str(HERE.relative_to(ROOT)/'clock_cycles.c'),
               'drivers/display/pqbn/pqbn_v4.c', '-lm', '-o', exe]
        if old: cmd += ['-DAUDIT_OLD_SKIP']
        for label, command in [('compile', cmd), ('run', [exe, '20', '3'])]:
            row = dict(old_idle_algorithm=old, label=label, **run(command))
            rows.append(row)
            (HERE/'clock-cycle-results.json').write_text(json.dumps(rows, indent=2)+'\n')
            print(json.dumps(row), flush=True)
            expected = 1 if old and label == 'run' else 0
            if row['exit_code'] != expected: raise SystemExit('unexpected clock exit status')
elif sys.argv[1] == 'known':
    cmd = ['build-audit/ink-sim', '--assets', 'build-audit/assets', '--content', 'build-audit/test-content',
           '--profile', 'simulator/profiles/metalio-plus.json',
           '--script', 'docs/audits/2026-11-codex-review/gen_race.script',
           '--hub', 'docs/audits/2026-11-codex-review/gen_race.hub.json',
           '--out', 'build-audit/r8-known/gen-race', '--no-random-outages', '--trace']
    row = run(cmd)
    if row['exit_code']: raise SystemExit(json.dumps(row))
    data = json.loads(Path('build-audit/r8-known/gen-race/metrics.json').read_text())
    row['metrics'] = {k: v for k, v in data['metrics'].items() if k.startswith(('reader.', 'tools.'))}
    row['warnings'] = data.get('warnings')
    (HERE/'known-results.json').write_text(json.dumps([row], ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(row, ensure_ascii=False))
else:
    raise SystemExit('usage: auxiliary.py clock|known')
