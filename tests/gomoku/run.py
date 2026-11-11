"""Gomoku acceptance, a fresh process on the same flash, and 120% text screenshots."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--phases', default='main reboot big')
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-gomoku'
area = build / 'test-out' / ('gomoku' if not a.binary else 'gomoku-' + binary.parent.name)
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
for phase in a.phases.split():
    flash = 'big' if phase == 'big' else 'main'
    out = area / phase
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/gomoku/test.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--flash-image', str(area / (flash + '.img')), '--sd-dir', str(area / (flash + '-sd')), '--quiet', '--no-random-outages'],
        cwd=ROOT, env={**os.environ, 'INK_GOMOKU_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'}, capture_output=True, text=True, timeout=300)
    out.mkdir(exist_ok=True)
    (out / 'run.log').write_text(result.stdout + result.stderr)
    path = out / 'metrics.json'
    data = json.loads(path.read_text()) if path.exists() else {}
    metrics = data.get('metrics', {})
    print(f'{phase}: exit={result.returncode} checks={metrics.get("test.pass", 0)} failures={metrics.get("test.fail", 0)} finished={metrics.get("test.finished", 0)} refresh_max_px={metrics.get("test.move_refresh_max_px", 0)}', flush=True)
    if result.returncode or metrics.get('test.finished') != 1 or metrics.get('test.fail', 0) or data.get('warnings'):
        raise SystemExit(result.stderr[-5000:] + '\n' + repr(data.get('warnings', [])))
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
