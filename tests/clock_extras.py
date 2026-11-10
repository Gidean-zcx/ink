"""Clock extras: real UI/capabilities, virtual time and a process restart."""
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
binary = a.binary.resolve() if a.binary else build / 'sim-clock-extras'
area = build / 'clock-extras-evidence' / binary.name
shutil.rmtree(area, ignore_errors=True)
(area / 'sd').mkdir(parents=True)
for phase in ('create', 'restart'):
    out = area / phase
    r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--end', '300s', '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'),
        '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')],
        env={**os.environ, 'INK_CLOCK_EXTRAS_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'},
        capture_output=True, text=True, timeout=110)
    (area / f'{phase}.log').write_text(r.stdout + r.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    print(f'{phase}: exit={r.returncode}, pass={m.get("test.pass", 0)}, fail={m.get("test.fail", 0)}, finished={m.get("test.finished", 0)}', flush=True)
    if r.returncode or m.get('test.fail', 0) or m.get('test.finished') != 1 or data.get('warnings'):
        raise SystemExit(r.stdout[-4000:] + r.stderr[-16000:] + '\n' + '\n'.join(data.get('warnings', [])))
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
