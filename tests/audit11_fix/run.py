"""B11-01 / B11-02: card pruning keeps ids on a status error and survives a query between batches."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--case', choices=('b11-01', 'b11-02'), required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out' / f'audit11-{a.case}'
if area.exists():
    shutil.rmtree(area)
content = area / 'sd/content'
for root in ('', '.m/owner', '.m/kid', 'SYNTH_EMPTY'):
    (content / root).mkdir(parents=True, exist_ok=True)
for i in range(60):
    root = '' if i < 30 else '.m/owner' if i < 50 else '.m/kid'
    file = content / root / f'SYNTH_{i:04d}.txt'
    file.write_bytes(b'X' * (i + 1))
    os.utime(file, (1000000 + i, 1000000 + i))

previous = None
for stage in ('exercise', 'restart'):
    env = {**os.environ, 'INK_AUDIT11_CASE': a.case, 'UBSAN_OPTIONS': 'halt_on_error=1'}
    if previous:
        env['INK_AUDIT11_ID'] = str(int(previous['test.audit11.original']['max']))
    out = area / stage
    result = subprocess.run([str(build / 'sim-audit11-fix'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--sd-dir', str(area / 'sd'), '--flash-image', str(area / 'user.img'),
        '--out', str(out), '--end', '300s', '--quiet', '--no-random-outages'], cwd=ROOT,
        env=env, capture_output=True, text=True, timeout=150)
    (area / f'{stage}.log').write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text())
    m = data['metrics']
    ok = result.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and not data.get('warnings')
    if not ok:
        raise SystemExit(f'FAIL {a.case}-{stage}: exit={result.returncode} {data.get("warnings")}\n{result.stderr[-2000:]}')
    print(f'PASS {a.case}-{stage}: checks={m.get("test.pass", 0)}', flush=True)
    previous = previous or m
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
