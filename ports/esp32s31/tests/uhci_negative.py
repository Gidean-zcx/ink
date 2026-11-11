"""Run real glue/parser and rail ordering checks, then reject four one-place regressions."""
import argparse
from pathlib import Path
import resource
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('build', type=Path)
p.add_argument('--cc', default='cc')
p.add_argument('--sanitize', action='store_true')
a = p.parse_args()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
port = ROOT / 'ports/esp32s31'
tests = port / 'tests'
sources = {name: path.read_text() for name, path in {
    'board': ROOT / 'boards/metalio_plus/metalio_board.c',
    'power': port / 'hal_power_s31.c'}.items()}
mutations = [
    ('no-rail-settle', 'board',
     '    if (s == HAL_I2C_OK) osal_task_delay_us(POWER_RAIL_SETTLE_MS * 1000u);\n    if (s == HAL_I2C_OK) s = metalio_board_set(0, MB_P0_CAM_SCR, MB_P0_CAM_SCR);',
     '    if (s == HAL_I2C_OK) s = metalio_board_set(0, MB_P0_CAM_SCR, MB_P0_CAM_SCR);'),
    ('late-cell-notify', 'power', '    cut = true;\n',
     '    metalio_board_cut_rails(cfg->keep_modem);\n    cut = true;\n'),
]
cell_args = [sys.executable, str(tests / 'cell_negative.py'), str(a.build), '--case', 'dma-return', '--case', 'rx-gap']
if a.sanitize:
    cell_args.append('--sanitize')
subprocess.run(cell_args, check=True)
# Move the existing notification block as a single mutation, rather than add a second call.
notify = '''    if (!cfg->keep_modem) {
        if (!ink_s31_cell_suspend()) goto out;
        cell_stopped = true;
        ink_s31_bh_suspend();
    }
'''
with tempfile.TemporaryDirectory(prefix='uhci-negative-', dir=a.build.resolve()) as tmp:
    area = Path(tmp)
    for name, kind, old, new in [(f'baseline-{k}', k, '', '') for k in sources] + mutations:
        source = sources[kind]
        if old:
            if name == 'late-cell-notify':
                assert source.count(notify) == 1
                source = source.replace(notify, '')
                old = '    if (metalio_board_cut_rails(cfg->keep_modem) != HAL_I2C_OK) goto out;\n'
                new = old + notify
            assert source.count(old) == 1, name
            source = source.replace(old, new)
        copy = area / f'{kind}.c'
        copy.write_text(source)
        command = [a.cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-UNDEBUG']
        if sys.platform == 'darwin':
            command += ['-isysroot', subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip()]
        if a.sanitize:
            command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        stub = 'stub'
        for folder in [port, tests / stub, ROOT / 'platform/include', ROOT / 'drivers/chips', ROOT / 'boards']:
            command += ['-I', str(folder)]
        if kind == 'power':
            command += ['-DINK_S31_LIGHT_SLEEP=1']
            units = [tests / 'power.c', copy]
        else:
            units = [tests / 'board.c', copy, ROOT / 'boards/metalio_plus/metalio_epd_power.c',
                     ROOT / 'drivers/chips/tca9555/tca9555.c', ROOT / 'drivers/chips/tps65185/tps65185.c']
        binary = area / name
        compiled = subprocess.run(command + [str(unit) for unit in units] + ['-o', str(binary)], capture_output=True, text=True)
        if compiled.returncode:
            raise SystemExit(f'{name}: compile failed\n{compiled.stderr}')
        result = subprocess.run([str(binary)], cwd=area, capture_output=True, text=True, timeout=15)
        if (result.returncode == 0) != (not old):
            raise SystemExit(f'{name}: unexpected test={result.returncode}\n{result.stdout}\n{result.stderr}')
        evidence = result.stderr.strip().splitlines()
        print(f'{name}: compile=0 test={result.returncode}' + (f'; {evidence[-1]}' if evidence else ''), flush=True)
print('UHCI/rails reverse controls: 4/4 rejected; vendor/HAL and rail baselines passed')
