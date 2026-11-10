"""Run time settings against a synthetic network and fresh device image."""
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
binary = a.binary.resolve() if a.binary else build / 'sim-time-settings'
area = build / 'time-settings-evidence' / binary.name
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--script', str(ROOT / 'tests/time_settings.script'), '--hub', str(ROOT / 'tests/time_settings_hub.json'),
    '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--assets', str(build / 'assets'),
    '--content', str(build / 'test-content'), '--out', str(area), '--flash-image', str(area / 'user.img'),
    '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages'], cwd=ROOT,
    capture_output=True, text=True, timeout=180, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
(area / 'run.log').write_text(result.stdout + result.stderr)
data = json.loads((area / 'metrics.json').read_text()) if (area / 'metrics.json').exists() else {}
counts = data.get('metrics', {})
print(f'exit={result.returncode}, pass={counts.get("test.pass", 0)}, fail={counts.get("test.fail", 0)}, finished={counts.get("test.finished", 0)}', flush=True)
if result.returncode or counts.get('test.fail', 0) or counts.get('test.finished') != 1 or data.get('warnings'):
    raise SystemExit(result.stderr[-9000:] + '\n' + '\n'.join(data.get('warnings', [])))
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
