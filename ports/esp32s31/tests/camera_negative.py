"""Compile isolated camera regressions; require assertions to catch each."""
import json
from pathlib import Path
import resource
import shlex
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[3]
source = root / 'ports/esp32s31/hal_camera_s31.c'
session = source.parent / 'vendor/dvp_camera/session.inc'
driver = root / 'ports/esp32s31/tests/camera.c'
commands = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in commands if Path(e['file']).resolve() == driver)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
mutations = [
    ('keep-warmup', session, '== INK_CAM_WARMUP + 1', '== 1'),
    ('reset-without-touch-restore', session, 'gpio_reset_pin(INK_CAM_MCLK);',
     'gpio_reset_pin(INK_CAM_MCLK); if (S.frame) metalio_board_set(1, MB_P1_SHARED_RST, 0);'),
    ('p05-during-waveform', source, 'if (ink_s31_display_acquire(timeout_ms)) {',
     'metalio_board_set(0, MB_P0_CAM_SCR, 0);\n    if (ink_s31_display_acquire(timeout_ms)) {'),
    ('per-frame-allocation', source, 'r || !buf || !ink_cam_preview', 'true'),
    ('cached-negative', source, 'ink_cam_checked = false;', 'ink_cam_checked = true;'),
    ('repeat-warmup', session, 'S.requests = INK_CAM_WARMUP;', 'S.requests = 0;'),
    ('ignore-cancel', session, 'if (S.cancelled && S.cancelled(S.context))',
     'if (false && S.cancelled && S.cancelled(S.context))'),
    ('ignore-page-close', source, 'return ink_cam_preview && !__atomic_load_n(ink_cam_preview, __ATOMIC_ACQUIRE);',
     'return false;'),
]
with tempfile.TemporaryDirectory(prefix='camera-negative-', dir=build) as temp:
    work = Path(temp)
    for name, target, old, new in mutations:
        text = target.read_text()
        assert text.count(old) == 1, name
        mutant = work / ('session.inc' if target == session else 'camera.c')
        mutant.write_text(text.replace(old, new))
        if target == session:
            adapted = source.read_text().replace('"vendor/dvp_camera/session.inc"', f'"{mutant}"')
            mutant = work / 'camera.c'
            mutant.write_text(adapted)
        args = shlex.split(entry['command']) if 'command' in entry else entry['arguments'][:]
        obj = work / 'driver.o'
        args[args.index('-o') + 1] = str(obj)
        args += ['-I', str(source.parent), f'-DINK_CAMERA_SOURCE="{mutant}"']
        subprocess.run(args, cwd=entry['directory'], check=True, capture_output=True, text=True)
        link = shlex.split((build / 'CMakeFiles/test-s31-camera.dir/link.txt').read_text())
        link = [str(obj) if x.endswith('tests/camera.c.o') or x == 'CMakeFiles/test-s31-camera.dir/camera.c.o' else x for x in link]
        binary = work / 'test'
        link[link.index('-o') + 1] = str(binary)
        subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
        assert result.returncode != 0, name
        assert 'Assertion' in result.stderr or 'assertion' in result.stderr, result.stderr
        print(f'{name}: compile=0, test={result.returncode}; {result.stderr.strip().splitlines()[-1]}')
