"""accel-use (D-122): the behaviour checks, then the standby current with lift to wake on and off, lying flat and
untouched and with a bump every two minutes (simulator power model; its currents are estimates)."""
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
p.add_argument('--phases', default='main standby_on standby_off bumps_on bumps_off')
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-accel-use'
area = build / 'test-out' / ('accel-use' if not a.binary else 'accel-use-' + binary.name)
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
got = {}
for phase in a.phases.split():
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/accel_use/accel_use.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(area / phase),
        '--flash-image', str(area / (phase + '.img')), '--sd-dir', str(area / (phase + '-sd')), '--quiet', '--no-random-outages'],
        cwd=ROOT, capture_output=True, text=True, timeout=600,
        env={**os.environ, 'INK_ACCEL_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (area / (phase + '.log')).write_text(result.stdout + result.stderr)
    path = area / phase / 'metrics.json'
    data = json.loads(path.read_text(errors='replace')) if path.exists() else {}
    counts = data.get('metrics', {})
    if result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or data.get('warnings'):
        raise SystemExit(f'{phase}: FAIL exit={result.returncode}\n{result.stderr[-6000:]}\n{data.get("warnings", [])}')
    got[phase] = counts
    print(f'{phase}: PASS checks={counts.get("test.pass")}', flush=True)


def m(phase, key):
    return got[phase].get(key, 0)


if 'standby_on' in got and 'standby_off' in got:
    for kind in ('standby', 'bumps'):
        on, off = kind + '_on', kind + '_off'
        if on not in got or off not in got:
            continue
        print(f'{kind}: average from the lock {m(on, "test.standby_ma"):.4f} mA with lift to wake, '
              f'{m(off, "test.standby_ma"):.4f} mA without; motion wakes {m(on, "power.wakes.motion"):.0f} / '
              f'{m(off, "power.wakes.motion"):.0f}; accelerometer {m(on, "power.mah.accel"):.5f} / {m(off, "power.mah.accel"):.5f} mAh',
              flush=True)
    # untouched: the low-power motion watch costs about 1 uA more than the quiet mode, nothing wakes the chip
    if m('standby_on', 'power.wakes.motion') != 0 or m('standby_off', 'power.wakes.motion') != 0:
        raise SystemExit('standby: a motion wake without any movement')
    extra = m('standby_on', 'test.standby_ma') - m('standby_off', 'test.standby_ma')
    if not 0 <= extra < 0.005:
        raise SystemExit(f'standby: lift to wake costs {extra:.4f} mA, more than the 5 uA expected')
    if 'bumps_on' in got and 'bumps_off' in got:
        if m('bumps_off', 'power.wakes.motion') != 0 or m('bumps_on', 'power.wakes.motion') < 10:
            raise SystemExit('bumps: motion wakes only with lift to wake on')
        if m('bumps_on', 'test.standby_ma') <= m('bumps_off', 'test.standby_ma'):
            raise SystemExit('bumps: a motion wake should cost something')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
