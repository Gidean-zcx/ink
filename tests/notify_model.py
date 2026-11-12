"""Notification service regression with the same synthetic flash across boots."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-notify-model'
area = build / 'notify-model-evidence'
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
for phase in ('seed', 'main', 'restart'):
    out = area / phase
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/integration/notify.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages'],
        cwd=ROOT, capture_output=True, text=True, timeout=180,
        env={**os.environ, 'INK_NOTIFY_MODEL_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    out.mkdir(parents=True, exist_ok=True)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    (out / 'run.log').write_text(result.stdout + result.stderr + '\n' + '\n'.join(data.get('warnings', [])))
    counts = data.get('metrics', {})
    print(f'{phase}: exit={result.returncode}, pass={counts.get("test.pass", 0)}, '
          f'fail={counts.get("test.fail", 0)}, finished={counts.get("test.finished", 0)}', flush=True)
    delays = counts.get('notify.ring_delay_ms')
    if delays:
        print('ring delay:', delays, flush=True)
    if result.returncode or counts.get('test.fail', 0) or counts.get('test.finished') != 1 or data.get('warnings'):
        raise SystemExit((out / 'run.log').read_text()[-12000:])
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
