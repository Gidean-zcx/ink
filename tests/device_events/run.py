"""Exercise device delivery identity, directory transactions and reboot event identity."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', required=True)
a = p.parse_args()
build = Path(a.build).resolve()
out = build / 'test-out/device-events'
out.mkdir(parents=True, exist_ok=True)
flash = out / 'user.img'
flash.unlink(missing_ok=True)
boots = []
for number in (1, 2):
    folder = out / str(number)
    cmd = [str(build / 'sim-device-events'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
           '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(folder),
           '--end', '90s', '--quiet', '--no-random-outages', '--flash-image', str(flash)]
    run = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=50)
    (out / f'{number}.log').write_text(run.stdout + run.stderr)
    metrics = json.loads((folder / 'metrics.json').read_text()) if (folder / 'metrics.json').exists() else {}
    m = metrics.get('metrics', {})
    assert run.returncode == 0 and m.get('test.fail', 0) == 0 and m.get('test.finished', 0) == 1, (run.returncode, m, run.stderr[-2500:])
    boots.append(m['test.event_boot']['max'])
    print(f'boot {number}: {m.get("test.pass", 0)} checks passed')
assert boots[1] == boots[0] + 1, boots
print('durable boot sequence advances across restart')
folder = out / 'review'
run = subprocess.run([str(build / 'sim-device-events-review'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                      '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(folder),
                      '--end', '180s', '--quiet', '--no-random-outages'], cwd=ROOT, capture_output=True, text=True, timeout=50)
(out / 'review.log').write_text(run.stdout + run.stderr)
metrics = json.loads((folder / 'metrics.json').read_text()) if (folder / 'metrics.json').exists() else {}
m = metrics.get('metrics', {})
assert run.returncode == 0 and m.get('test.fail', 0) == 0 and m.get('test.finished', 0) == 1, (run.returncode, m, metrics.get('warnings'), run.stderr[-2500:])
print(f'review regressions: {m.get("test.pass", 0)} checks passed')
