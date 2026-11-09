"""Real sound and timer UI checks, including process-restart persistence."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--phase', choices=('sound', 'timer', 'restart'))
a = p.parse_args()
build = a.build.resolve()
(build / 'test-out').mkdir(exist_ok=True)
area = Path(tempfile.mkdtemp(prefix='sound-clock-', dir=build / 'test-out'))
(area / 'sd').mkdir()
for phase in [a.phase] if a.phase else ('sound', 'timer', 'restart'):
    out = area / phase
    r = subprocess.run([str(a.binary.resolve() if a.binary else build / 'sim-sound-clock'),
        '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'), '--assets', str(build / 'assets'),
        '--content', str(build / 'test-content'), '--out', str(out), '--end', '240s',
        '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages',
        '--budget', str(ROOT / 'tests/ui_scene.budget.json')], env={**os.environ, 'INK_SOUND_PHASE': phase},
        capture_output=True, text=True, timeout=180)
    (area / f'{phase}.log').write_text(r.stdout + r.stderr)
    m = json.loads((out / 'metrics.json').read_text())['metrics']
    print(f'{phase}: exit={r.returncode}, pass={m.get("test.pass", 0)}, fail={m.get("test.fail", 0)}, finished={m.get("test.finished", 0)}; {out.relative_to(ROOT)}', flush=True)
    if r.returncode or m.get('test.fail', 0) or m.get('test.finished', 0) != 1:
        raise SystemExit(r.stdout + r.stderr)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
