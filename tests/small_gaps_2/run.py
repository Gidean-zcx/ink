"""Run UI acceptance; --binary and --out support manual reverse comparisons."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--case', choices=('icons', 'phone', 'picker', 'calculator', 'airplane'), required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--out', type=Path)
a = p.parse_args()
build = a.build.resolve()
out = (a.out or build / 'test-out' / 'small-gaps-2' / a.case).resolve()
shutil.rmtree(out, ignore_errors=True)
result = subprocess.run([str((a.binary or build / 'sim-small-gaps-2').resolve()),
    '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'), '--end', '180s',
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
    '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--quiet', '--no-random-outages'],
    cwd=ROOT, capture_output=True, text=True, timeout=180,
    env={**os.environ, 'INK_SMALL_GAPS_CASE': a.case, 'UBSAN_OPTIONS': 'halt_on_error=1'})
(out / 'run.log').write_text(result.stdout + result.stderr)
data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
counts = data.get('metrics', {})
ok = result.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail', 0) and not data.get('warnings')
print(a.case, 'PASS' if ok else 'FAIL', 'exit', result.returncode, 'checks', counts.get('test.pass', 0), 'failures', counts.get('test.fail', 0), flush=True)
if not ok:
    raise SystemExit(result.stderr[-4000:] + '\n' + str(data.get('warnings', [])))
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(out)
