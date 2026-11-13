"""Lock, PIN gate and child capture; real persisted PNGs and a bounded negative control."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', required=True, type=Path)
p.add_argument('--negative', action='store_true')
p.add_argument('--late-mode', action='store_true')
p.add_argument('--retain-gallery', action='store_true')
p.add_argument('--sleep-capture', action='store_true')
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix='lock-camera-', dir=build))
binary = build / 'sim-lock-camera'
control = a.negative or a.late_mode or a.retain_gallery or a.sleep_capture
if control:
    source = (ROOT / 'system/apps/camera/camera.c').read_text()
    old = 'bool limited = ink_camera_restricted();'
    assert source.count(old) == 1
    mutant = work / 'camera.c'
    if a.sleep_capture:
        changed = source.replace('    ink_power_hold(INK_NEED_AWAKE);\n', '', 1).replace('ink_power_release(INK_NEED_AWAKE); ', '', 2)
    elif a.retain_gallery:
        changed = source.replace('    ink_ui_drop_app("gallery");\n', '')
    elif a.late_mode:
        changed = source.replace('    ' + old + '\n', '').replace('    if (rgb && p->saved) {', '    if (rgb && p->saved) {\n        ' + old)
    else:
        changed = source.replace(old, 'bool limited = false;')
    mutant.write_text(changed.replace('"../gallery/photo.h"', '"' + str(ROOT / 'system/apps/gallery/photo.h') + '"'))
    driver = (ROOT / 'tests/lock_camera/lock_camera.c').read_text()
    for include, target in (
        ('../../system/apps/camera/camera.c', mutant),
        ('../../system/boot/main.c', ROOT / 'system/boot/main.c'),
        ('../integration/whole_product.h', ROOT / 'tests/integration/whole_product.h'),
        ('../../system/ui/shell/member_pages.h', ROOT / 'system/ui/shell/member_pages.h'),
    ):
        driver = driver.replace('"' + include + '"', '"' + str(target) + '"')
    path = work / 'driver.c'
    path.write_text(driver)
    flags = {}
    for line in (build / 'CMakeFiles/sim-lock-camera.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            k, v = line.split(' = ', 1)
            flags[k] = shlex.split(v)
    link = shlex.split((build / 'CMakeFiles/sim-lock-camera.dir/link.txt').read_text())
    obj = work / 'driver.o'
    result = subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'], '-c', str(path), '-o', str(obj)], cwd=build, capture_output=True, text=True)
    (work / 'compile.log').write_text(result.stdout + result.stderr)
    assert result.returncode == 0, result.stderr
    link = [str(obj) if x.endswith('tests/lock_camera/lock_camera.c.o') else x for x in link]
    binary = work / 'sim-negative'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)

for phase in (['lock'] if control else ['lock', 'gate', 'kid']):
    out = work / phase
    sd = work / (phase + '-sd')
    sd.mkdir()
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/lock_camera/camera.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--flash-image', str(work / (phase + '.img')), '--sd-dir', str(sd), '--quiet', '--no-random-outages'],
        env={**os.environ, 'INK_LOCK_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'},
        cwd=ROOT, capture_output=True, text=True, timeout=180)
    (work / (phase + '.log')).write_text(result.stdout + result.stderr)
    metrics = json.loads((out / 'metrics.json').read_text(errors='replace')) if (out / 'metrics.json').exists() else {}
    counts = metrics.get('metrics', {})
    passed = result.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail', 0)
    print(phase, 'PASS' if passed else 'FAIL', 'exit', result.returncode, 'assertions', counts.get('test.pass'), 'failures', counts.get('test.fail', 0), flush=True)
    if control:
        evidence = result.stdout + result.stderr + json.dumps(metrics.get('warnings', []))
        expected = ('accepted capture finishes after HOME',) if a.sleep_capture else ('unlocked Gallery contains the public captures', 'touch shutter saves the first session photo') if a.retain_gallery else ('in-flight locked capture stays in public storage after HOME',) if a.late_mode else ('locked captures are stored outside every member',)
        hit = next((message for message in expected if message in evidence), None)
        assert not passed and counts.get('test.finished') == 1 and hit, evidence[-5000:]
        print('PASS: removing the fix fails the unchanged regression:', hit, flush=True)
        continue
    if not passed:
        print('Evidence:', work.relative_to(ROOT), flush=True)
        raise SystemExit(json.dumps(metrics.get('warnings', []), ensure_ascii=False) + result.stderr[-3000:])
    shared = list((sd / 'content/锁屏拍的照片').glob('*.png'))
    private = list((sd / 'content/.m/owner/照片').glob('*.png'))
    assert len(shared) == {'lock': 4, 'gate': 3, 'kid': 1}[phase] and len(private) == 1, 'locked photos escaped their public destination'
    assert not list((sd / 'content/.m/kid').rglob('*.png')), 'locked photos belong to a child'
    for photo in shared + private:
        with Image.open(photo) as im:
            assert im.format == 'PNG' and im.mode == 'RGB' and im.size == (640, 480)
    assert all((out / ('snap-' + name + '.png')).exists() for name in (('lock-entry',) if phase == 'kid' else ('lock-entry', 'restricted', 'confirmation')))
    print(phase, 'persisted PNGs, ownership and screenshots PASS', flush=True)
if os.environ.get('INK_KEEP_EVIDENCE'):
    print('Evidence:', work.relative_to(ROOT))
else:
    shutil.rmtree(work)
