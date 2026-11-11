"""Run the real camera UI with the normal model and the retained S31 HAL."""
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
build = Path(sys.argv[1]).resolve()
work = Path(tempfile.mkdtemp(prefix='camera-preview-', dir=build))
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in commands if e['file'].endswith('/tests/lock_camera/lock_camera.c'))
base = shlex.split(entry['command']) if 'command' in entry else entry['arguments'][:]
base += ['-I', str(root / 'ports/esp32s31'), '-I', str(root / 'ports/esp32s31/tests/camera_stub'),
         '-I', str(root / 'system/apps/camera')]
driver = root / 'ports/esp32s31/tests/camera_preview.c'
service = work / 'service.c'
service.write_text('#define hal_camera_info fixture_camera_info\n#define hal_camera_capture fixture_camera_capture\n'
                   f'#include "{root / "system/services/camera/camera_service.c"}"\n')
script = work / 'preview.script'
script.write_text('@0 net wifi\n@60s end\n')


def compile_file(source, obj, extra=()):
    args = base[:]
    args[args.index(entry['file'])] = str(source)
    args[args.index('-o') + 1] = str(obj)
    result = subprocess.run([*args, *extra], cwd=entry['directory'], capture_output=True, text=True)
    if result.returncode:
        raise AssertionError(result.stderr)


compile_file(service, work / 'service.o')
measure_model = '--model' in sys.argv[2:]
cases = [('model', ())] if measure_model else [('dvp', ('-DINK_PREVIEW_DVP=1',))]
if '--reverse' in sys.argv[2:]:
    cases = []
mutations = [
    ('old-period', 'lv_timer_create(preview, LV_DEF_REFR_PERIOD, NULL)', 'lv_timer_create(preview, 3000, NULL)',
     'display-paced preview advances'),
    ('slow-waveform', 'p->preview ? INK_REFRESH_FAST : INK_REFRESH_UI', 'INK_REFRESH_UI',
     'display-paced preview advances'),
    ('no-display-gate', 'if (t && (ink_ui_busy() || ink_display_busy())) { preview_ready = false; return; }', '/* gate removed */',
     'next preview capture waits'),
    ('no-reprobe', '    ink_camera_init();', '    /* reprobe removed */',
     'page entry reprobes'),
]
for name, old, new, expected in mutations:
    if measure_model:
        break
    text = (root / 'system/apps/camera/camera.c').read_text()
    assert text.count(old) == 1, name
    mutant = work / f'{name}.c'
    mutant.write_text(text.replace(old, new))
    cases.append((name, ('-DINK_PREVIEW_DVP=1', f'-DINK_CAMERA_APP_SOURCE="{mutant}"')))

try:
    for name, flags in cases:
        obj = work / f'{name}.o'
        compile_file(driver, obj, flags)
        link = shlex.split((build / 'CMakeFiles/sim-lock-camera.dir/link.txt').read_text())
        link = [str(obj) if x.endswith('tests/lock_camera/lock_camera.c.o') else x for x in link]
        link.append(str(work / 'service.o'))
        binary = work / name
        link[link.index('-o') + 1] = str(binary)
        result = subprocess.run(link, cwd=build, capture_output=True, text=True)
        assert not result.returncode, result.stderr
        out = work / f'{name}-out'
        sd = work / f'{name}-sd'
        sd.mkdir()
        profile = 'metalio-plus.json' if name == 'model' else 'metalio-plus-pqbn.json'
        args = [str(binary), '--profile', str(root / 'simulator/profiles' / profile),
                '--script', str(script), '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
                '--out', str(out), '--sd-dir', str(sd), '--quiet', '--no-random-outages',
                '--budget', str(root / 'tests/ui_scene.budget.json')]
        result = subprocess.run(args, capture_output=True, text=True, timeout=150)
        (work / f'{name}.log').write_text(result.stdout + result.stderr)
        metrics = json.loads((out / 'metrics.json').read_text())
        counts = metrics.get('metrics', {})
        warnings = json.dumps(metrics.get('warnings', []), ensure_ascii=False)
        print(name, 'fps=', counts.get('camera.preview.fps', {}).get('mean'),
              'frames=', counts.get('camera.preview.frames', {}).get('mean'),
              'failed_checks=', counts.get('test.fail', 0), flush=True)
        if name in ('model', 'dvp'):
            assert not result.returncode and counts.get('test.finished') == 1 and not counts.get('test.fail'), warnings + result.stderr[-2000:]
            print(name, 'PASS', 'fps=', counts.get('camera.preview.fps'), 'frames=', counts.get('camera.preview.frames'), flush=True)
        else:
            expected = next(m[3] for m in mutations if m[0] == name)
            assert counts.get('test.fail', 0) and expected in warnings, warnings
            print(name, 'PASS: unchanged assertion rejects removed fix:', expected, flush=True)
except BaseException:
    print('Evidence:', work.relative_to(root), flush=True)
    raise
if os.environ.get('INK_KEEP_EVIDENCE'):
    print('Evidence:', work.relative_to(root))
else:
    shutil.rmtree(work)
