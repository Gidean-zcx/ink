"""Lock renderer coverage; deliberately does not claim the missing settings-page integration."""
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
binary = a.binary.resolve() if a.binary else build / 'sim-lock-faces'
area = build / 'test-out' / ('lock-faces-' + binary.name)
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(area),
    '--end', '900s', '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'),
    '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')],
    capture_output=True, text=True, timeout=180, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
(area / 'test.log').write_text(r.stdout + r.stderr)
data = json.loads((area / 'metrics.json').read_text()) if (area / 'metrics.json').exists() else {}
m = data.get('metrics', {})
ok = r.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and not data.get('warnings')
print(f'lock-faces: exit={r.returncode}, pass={m.get("test.pass", 0)}, fail={m.get("test.fail", 0)}, finished={m.get("test.finished", 0)}', flush=True)
if not ok:
    print(r.stderr[-10000:], data.get('warnings', []))
elif not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
raise SystemExit(0 if ok else 1)
