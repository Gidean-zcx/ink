"""Compile isolated protocol mutations; requires no Git history and never opens a device."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('build', type=Path)
p.add_argument('--cc', default='cc')
p.add_argument('--sanitize', action='store_true')
a = p.parse_args()
source = (ROOT / 'ports/esp32s31/s31_modem.c').read_text()
mutations = [
    ('no-wake-handshake', '        gpio_set_level(INK_MRDY, 0);\n        uint64_t until', '        uint64_t until'),
    ('bad-header-checksum', 'f->data[3] = (uint8_t)((sum >> 8) ^ sum ^ 3);', 'f->data[3] = (uint8_t)((sum >> 8) ^ sum ^ 4);'),
    ('allow-storage-writes', 'if (!ink_allowed(cmd)) return HAL_AT_ERROR;', 'if (false && !ink_allowed(cmd)) return HAL_AT_ERROR;'),
]
a.build.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='cell-negative-', dir=a.build.resolve()) as temp:
    area = Path(temp)
    for name, old, new in [('baseline', '', '')] + mutations:
        assert not old or source.count(old) == 1, name
        copy = area / 'modem.c'
        copy.write_text(source.replace(old, new) if old else source)
        binary = area / name
        command = [a.cc, '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror', '-UNDEBUG', '-Wno-unused-parameter']
        if sys.platform == 'darwin':
            sdk = subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip()
            command += ['-isysroot', sdk]
        if a.sanitize:
            command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        for d in ['ports/esp32s31/tests/cell_stub', 'ports/esp32s31', 'platform/include', 'drivers/chips', 'boards']:
            command += ['-I', str(ROOT / d)]
        command += [f'-DINK_TEST_CELL_SOURCE="{copy}"', str(ROOT / 'ports/esp32s31/tests/cell.c'),
                    str(ROOT / 'ports/esp32s31/hal_cellular_s31.c'), str(ROOT / 'drivers/chips/nt26/nt26.c'), '-o', str(binary)]
        compiled = subprocess.run(command, capture_output=True, text=True)
        if compiled.returncode:
            raise SystemExit(f'{name}: compile failed\n{compiled.stderr}')
        result = subprocess.run([str(binary)], cwd=area, capture_output=True, text=True, timeout=15)
        if (result.returncode == 0) != (not old):
            raise SystemExit(f'{name}: unexpected test={result.returncode}\n{result.stdout}\n{result.stderr}')
        print(f'{name}: compile=0 test={result.returncode}', flush=True)
print('cell reverse controls: 3/3 rejected; baseline passed')
