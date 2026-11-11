"""Restore the hardware dependency gate and require the no-camera page test to fail."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
source = root / 'system/apps/camera/camera.c'
driver = root / 'tests/port_camera/absent.c'
entry = next(e for e in json.loads((build / 'compile_commands.json').read_text()) if Path(e['file']).resolve() == driver)
with tempfile.TemporaryDirectory(prefix='camera-absent-negative-', dir=build) as temp:
    work = Path(temp)
    original = source.read_text()
    old = '.system_app = true, .child_ok = true};'
    assert original.count(old) == 1
    camera = work / 'camera.c'
    camera.write_text(original.replace('int ink_camera_app_init(void) {', 'int ink_camera_app_init(void) {\n    static const char *const needs[] = {"camera.capture", NULL};').replace(old, '.system_app = true, .child_ok = true, .needs = needs};'))
    text = driver.read_text()
    for relative in ('../../system/services/camera/camera_service.c', '../../system/apps/camera/camera.c', '../../system/boot/main.c', '../integration/whole_product.h'):
        target = camera if relative.endswith('apps/camera/camera.c') else (driver.parent / relative).resolve()
        text = text.replace(f'"{relative}"', f'"{target}"')
    changed = work / 'absent.c'
    changed.write_text(text)
    command = shlex.split(entry['command']) if 'command' in entry else entry['arguments'][:]
    obj = work / 'absent.o'
    command[command.index(str(driver))] = str(changed)
    command[command.index('-o') + 1] = str(obj)
    command += ['-I', str(source.parent)]
    subprocess.run(command, cwd=entry['directory'], check=True, capture_output=True, text=True)
    link = shlex.split((build / 'CMakeFiles/sim-camera-absent.dir/link.txt').read_text())
    link = [str(obj) if x.endswith('tests/port_camera/absent.c.o') else x for x in link]
    binary = work / 'sim-negative'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
    out = work / 'out'
    run = subprocess.run([str(binary), '--profile', str(root / 'simulator/profiles/metalio-plus.json'),
        '--script', str(root / 'tests/port_camera/absent.script'), '--assets', str(build / 'assets'),
        '--content', str(build / 'test-content'), '--out', str(out), '--quiet', '--no-random-outages'],
        capture_output=True, text=True, timeout=150)
    report = json.loads((out / 'metrics.json').read_text())
    metrics = report.get('metrics', {})
    evidence = run.stdout + run.stderr + json.dumps(report.get('warnings', []))
    assert metrics.get('test.fail', 0) > 0 and 'camera opens even when hardware is absent' in evidence, evidence[-4000:]
    print('restore-hardware-dependency: compile=0, failures=', metrics['test.fail'], '; missing-camera app can no longer open')
