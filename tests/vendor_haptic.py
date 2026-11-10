"""Haptic inputs and settings; --reverse mutates temporary objects only, outside CTest."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--reverse', action='store_true')
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out/vendor-haptic'
area.mkdir(parents=True, exist_ok=True)


def run(binary, name, restart=False, expected=None):
    out = area / name
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir()
    state = area / ('state-' + name)
    shutil.rmtree(state, ignore_errors=True)
    state.mkdir(exist_ok=True)
    phases = ('first', 'restart') if restart else ('first',)
    for phase in phases:
        result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
            '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--end', '240s',
            '--out', str(out / phase), '--flash-image', str(state / 'user.img'), '--sd-dir', str(state / 'sd'),
            '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')],
            cwd=ROOT, capture_output=True, text=True, timeout=180,
            env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1', 'INK_VENDOR_HAPTIC_RESTART': str(int(phase == 'restart'))})
        log = result.stdout + result.stderr
        data = json.loads((out / phase / 'metrics.json').read_text())
        log += '\n'.join(data.get('warnings', []))
        (out / (phase + '.log')).write_text(log)
        m = data['metrics']
        passed = result.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and not data.get('warnings')
        print(f'{name}/{phase}: exit={result.returncode}, checks={m.get("test.pass", 0)}, '
              f'failures={m.get("test.fail", 0)}, finished={m.get("test.finished", 0)}', flush=True)
        if expected:
            assert result.returncode != 0 and m.get('test.fail', 0) > 0 and expected in log, log[-8000:]
        else:
            assert passed, log[-10000:]
            if phase == 'first':
                for lang in ('zh', 'en'):
                    assert (out / phase / f'clean-vibration-{lang}.png').exists()


if not a.reverse:
    run(build / 'sim-vendor-haptic', 'fixed', restart=True)
else:
    db = json.loads((build / 'compile_commands.json').read_text())
    cases = (
        ('touch', 'system/services/input/input_service.c', 'if (pulse) ink_audio_haptic();', '(void)pulse;', 'finger down pulses before release'),
        ('keys', 'system/services/input/input_service.c', '        ink_audio_haptic();', '', 'physical key'),
        ('switch', 'system/services/audio/audio_service.c', 'if (ink_setting_get("audio.vibration")) hal_haptic_pulse', 'hal_haptic_pulse', 'switch off suppresses touch and key output'),
        ('strength', 'system/services/audio/audio_service.c', 'level == 2 ? 70', 'level == 2 ? 50', 'one pulse of 70 ms'),
    )
    for name, path, before, after, expected in cases:
        source = (ROOT / path).read_text()
        assert source.count(before) == 1
        source = source.replace(before, after)
        entry = next(e for e in db if Path(e['file']) == ROOT / path)
        command = shlex.split(entry['command'])
        obj = area / (name + '.o')
        command[command.index('-o') + 1] = str(obj)
        del command[command.index('-c'):command.index('-c') + 2]
        subprocess.run(command + ['-iquote', str((ROOT / path).parent), '-x', 'c', '-c', '-'],
                       input=source, cwd=build, text=True, check=True)
        link = shlex.split((build / 'CMakeFiles/sim-vendor-haptic.dir/link.txt').read_text())
        binary = area / ('without-' + name)
        link[link.index('-o') + 1] = str(binary)
        link.insert(next(i for i, part in enumerate(link) if part.endswith('libink_system.a')), str(obj))
        subprocess.run(link, cwd=build, check=True)
        run(binary, name, expected=expected)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
