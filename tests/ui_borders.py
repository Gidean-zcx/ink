"""Capture presenters in fresh processes, within the simulator's 64 mapped-asset limit."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out/ui-borders'
shutil.rmtree(area, ignore_errors=True)
total, index = 1, 0
while index < total:
    out = area / f'{index:02d}'
    result = subprocess.run([str(build / 'sim-ui-borders'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/ui_shell.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--quiet', '--no-random-outages'], env={**os.environ, 'INK_BORDER_CASE': str(index)}, capture_output=True, text=True, timeout=60)
    out.mkdir(parents=True, exist_ok=True)
    (out / 'process.log').write_text(result.stdout + result.stderr)
    counters = json.loads((out / 'metrics.json').read_text()).get('metrics', {}) if (out / 'metrics.json').exists() else {}
    assert result.returncode == 0 and counters.get('test.finished') == 1 and not counters.get('test.fail', 0), (index, result.returncode, result.stderr[-2000:])
    total = int(counters['test.border.cases']['max'])
    frames = list(out.glob('snap-*.png'))
    assert len(frames) == 1, (index, frames)
    print(frames[0].relative_to(build), flush=True)
    index += 1
print(f'{total} app/settings pages checked')
