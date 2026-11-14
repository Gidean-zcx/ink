"""keyboard-2 checks, one case per run (tests/keyboard2/kb.c). A reverse case must fail with its named check."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--case', required=True)
p.add_argument('--binary', default='sim-keyboard2')
a = p.parse_args()
build = a.build.resolve()
EXPECT = {'reverse-redraw': 'C1: ten letters refresh only', 'reverse-press': 'a press alone draws nothing'}
out = build / 'test-out' / f'keyboard2-{a.case}-{a.binary}'
shutil.rmtree(out, ignore_errors=True)
result = subprocess.run([str(build / a.binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--script', str(ROOT / 'tests/keyboard2/kb.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out), '--quiet', '--no-random-outages'],
    cwd=ROOT, capture_output=True, text=True, timeout=300, env={**os.environ, 'INK_KB_CASE': a.case})
out.mkdir(parents=True, exist_ok=True)
(out / 'test.log').write_text(result.stdout + result.stderr)
data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
metrics, warnings = data.get('metrics', {}), data.get('warnings', [])
if a.case in EXPECT:
    ok = metrics.get('test.fail', 0) > 0 and any(EXPECT[a.case] in w for w in warnings)
else:
    ok = result.returncode == 0 and metrics.get('test.finished') == 1 and not metrics.get('test.fail') and not warnings
print(json.dumps({'case': a.case, 'binary': a.binary, 'exit': result.returncode, 'passes': metrics.get('test.pass', 0),
                  'failures': metrics.get('test.fail', 0), 'expected_result': ok}, ensure_ascii=False))
if not ok:
    print(result.stderr[-4000:], warnings[:20])
if ok and a.case != 'shots' and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(out)
raise SystemExit(0 if ok else 1)
