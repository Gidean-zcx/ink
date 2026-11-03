#!/usr/bin/env python3
"""Recheck two existing high-priority follow-ups; do not assign new finding IDs."""
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
BUILD = ROOT / 'build-audit'
OUT = BUILD / 'r7-known'
OUT.mkdir(exist_ok=True)
OLD = Path('docs/audits/2026-11-codex-review')
rows = []

def run(name,cmd):
    p = subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,timeout=55)
    (OUT/(name+'.log')).write_text(p.stdout+p.stderr)
    row = dict(name=name,command=[str(c) for c in cmd],exit_code=p.returncode)
    row['output'] = p.stdout.splitlines() if name.startswith('clock') else [s for s in p.stdout.splitlines() if 'tool' in s or 'reader' in s]
    rows.append(row)
    (HERE/'known-results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(row),flush=True)
    return p.returncode

cmd = ['cc','-O1','-std=c11','-Idrivers/display/pqbn','-o',str(OUT/'clock'),str(OLD/'pqbn_clock.c'),'drivers/display/pqbn/pqbn_v4.c','-lm']
if run('clock-compile',cmd): raise SystemExit(1)
run('clock-20-3',[str(OUT/'clock'),'20','3'])
cmd = [str(BUILD/'ink-sim'),'--assets',str(BUILD/'assets'),'--content',str(BUILD/'test-content'),
       '--profile','simulator/profiles/metalio-plus.json','--script',str(OLD/'gen_race.script'),
       '--hub',str(OLD/'gen_race.hub.json'),'--out',str(OUT/'gen-race'),'--no-random-outages','--trace']
run('gen-race',cmd)
p = OUT/'gen-race/metrics.json'
if p.exists():
    m = json.loads(p.read_text())
    rows[-1]['metrics'] = {k:v for k,v in m['metrics'].items() if k.startswith(('tools.','reader.','ai.'))}
    rows[-1]['warnings'] = m.get('warnings')
    (HERE/'known-results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
