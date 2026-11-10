"""Manual boot comparison: reverse only the merged display admission fix."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--reverse-display', action='store_true')
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out/cell-features-boot' / ('reverse' if a.reverse_display else 'normal')
if area.exists():
    shutil.rmtree(area)  # A device log ring survives runs; compare fresh output only.
area.mkdir(parents=True)
binary = build / 'sim-hub-e2e'
if a.reverse_display:
    path = ROOT / 'ports/sim/hal/hal_display_sim.c'
    source = path.read_text()
    for old, new in (
        ('    sim_power_lease(true); /* Serialize every display start with sleep preparation. */\n', ''),
        ('    sim_power_lease(false);\n    return ok;', '    return ok;'),
    ):
        assert source.count(old) == 1
        source = source.replace(old, new)
    copy, obj = area / 'display.c', area / 'display.o'
    copy.write_text(source)
    entries = json.loads((build / 'compile_commands.json').read_text())
    entry = next(e for e in entries if Path(e['file']) == path)
    command = shlex.split(entry['command'])
    command[command.index(entry['file'])] = str(copy)
    command[command.index('-o') + 1] = str(obj)
    subprocess.run(command, cwd=entry['directory'], check=True)
    binary = area / 'sim-hub-e2e'
    link = shlex.split((build / 'CMakeFiles/sim-hub-e2e.dir/link.txt').read_text())
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, value in enumerate(link) if value.endswith('libink_port_sim.a')), str(obj))
    subprocess.run(link, cwd=build, check=True, capture_output=True)
hub_python = next(line.split('=', 1)[1] for line in (build / 'CMakeCache.txt').read_text().splitlines()
                  if line.startswith('INK_HUB_PYTHON:FILEPATH='))
env = {**os.environ, 'PYTHONDONTWRITEBYTECODE': '1', 'PYTHONPATH': str(ROOT / 'hub')}
result = subprocess.run([hub_python, '-B', str(ROOT / 'tests/hub/e2e.py'), '--sim', str(binary),
    '--parser', str(build / 'hub-json-check'), '--assets', str(build / 'assets'),
    '--content', str(build / 'test-content'), '--out', str(area / 'out')],
    cwd=ROOT, env=env, capture_output=True, text=True, timeout=60)
(area / 'run.log').write_text(result.stdout + result.stderr)
metrics = json.loads((area / 'out/sim/metrics.json').read_text())['metrics']
timing = {key: metrics['test.hub_' + key + '_ms']['max']
          for key in ('product_ready', 'member_pending', 'member_confirmed', 'member_ready')}
print(json.dumps({'reverse_display': a.reverse_display, 'exit': result.returncode, 'timing_ms': timing}))
assert timing['product_ready'] < 3000
if a.reverse_display:
    assert result.returncode == 1 and 'tool_calls=0' in result.stderr, result.stderr[-1500:]
    assert timing['member_ready'] >= 3000
    print('PASS: reversing display admission reproduces the unchanged hub regression')
else:
    assert result.returncode == 0, result.stderr[-1500:]
    assert timing['member_ready'] < 3000
    print('PASS: merged product meets the original first-tap deadline')
