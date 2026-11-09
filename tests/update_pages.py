"""Exercise update pages, real OTA boots and retained comparison inputs."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--mode')
a = p.parse_args()
build = a.build.resolve()
sim = a.binary.resolve() if a.binary else build / 'sim-update-pages'
area = build / 'test-out' / ('update-pages' if not a.binary else 'update-pages-' + sim.name)
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)


def run(mode, *, shared=False, package=None, reboot=False, badboot=False):
    out = area / mode
    env = dict(os.environ, INK_UPDATE_PAGE_MODE=mode, UBSAN_OPTIONS='halt_on_error=1')
    if package:
        env['INK_SIM_UPDATE_PKG'] = str(package)
        env['INK_UPDATE_GOOD_PKG'] = str(area / 'good.inkfw')
    command = [str(sim), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '--script', str(ROOT / 'tests/update_pages.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
               '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
               '--quiet', '--no-random-outages']
    if shared:
        command += ['--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd')]
    result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=90)
    out.mkdir(exist_ok=True)
    (out / 'stdout.log').write_text(result.stdout)
    (out / 'stderr.log').write_text(result.stderr)
    metrics = json.loads((out / 'metrics.json').read_text())
    counts = metrics['metrics']
    assert result.returncode == 0 and not counts.get('test.fail') and not metrics.get('warnings'), (
        mode, result.returncode, result.stderr[-3000:], metrics.get('warnings'))
    assert counts.get('test.finished') == 1 or badboot, (mode, 'not finished')
    if reboot:
        assert counts.get('power.reboot') == 1, (mode, 'not rebooted')
    if badboot:
        assert counts.get('update.self_check_failed') == 1, (mode, 'self-check did not fail')
    if mode.startswith('Update') or mode == 'BootRecovery':
        assert counts.get('test.snapshot.settled') == counts.get('test.snapshot.' + mode) == 1
        assert (out / 'snap-scene.png').exists()
    print(f'{mode}: PASS assertions={counts.get("test.pass", 0)} internal_min={counts.get("mem.internal_total.min_free_bytes")}', flush=True)


if a.mode:
    run(a.mode)
else:
    tool = ROOT / 'tools/inkfw/make_inkfw.py'
    for name, version, code, extra in [('bad', '1.3.0', 130, ['--corrupt-signature']),
                                      ('good', '1.3.0', 130, []), ('next', '1.4.0', 140, [])]:
        subprocess.run([sys.executable, str(tool), str(area / (name + '.inkfw')), '--version', version,
                        '--code', str(code), '--app-size', '1048576', *extra], check=True, capture_output=True)
    for name in ['Update', 'UpdateReady', 'UpdateBlocked', 'UpdateFail', 'UpdateInstall', 'UpdateDone', 'BootRecovery']:
        run(name)
    run('flow', shared=True, package=area / 'bad.inkfw', reboot=True)
    run('done', shared=True)
    run('rollback', shared=True, package=area / 'next.inkfw', reboot=True)
    run('badboot', shared=True, reboot=True, badboot=True)
    run('recovery', shared=True)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
