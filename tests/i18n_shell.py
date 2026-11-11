"""Exercise translated shell pages and retain English frames when INK_KEEP_EVIDENCE=1."""
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
p.add_argument('--out', type=Path)
p.add_argument('--min-frames', type=int, default=25)
a = p.parse_args()
build = a.build.resolve()
out = a.out.resolve() if a.out else build / 'test-out/i18n-shell'
shutil.rmtree(out, ignore_errors=True)
r = subprocess.run([str(a.binary.resolve() if a.binary else build / 'sim-i18n-shell'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--script', str(ROOT / 'tests/i18n_lang.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
    '--quiet', '--no-random-outages'], cwd=ROOT, capture_output=True, text=True, timeout=180,
    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
out.mkdir(parents=True, exist_ok=True)
(out / 'stdout.log').write_text(r.stdout)
(out / 'stderr.log').write_text(r.stderr)
data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
counts = data.get('metrics', {})
frames = sorted(out.glob('snap-*.png'))
assert not r.returncode and counts.get('test.finished') == 1 and not counts.get('test.fail') and not data.get('warnings'), (
    r.returncode, counts.get('test.finished'), counts.get('test.fail'), r.stderr[-12000:], data.get('warnings'))
assert len(frames) == counts.get('test.frames') and len(frames) >= a.min_frames, (len(frames), counts.get('test.frames'))
print(f'i18n-shell: PASS {counts.get("test.pass")} checks, {len(frames)} measured frames')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(out)
