#!/usr/bin/env python3
"""Manual reverse controls for the copied factory power sequence, on isolated files."""
import argparse
from pathlib import Path
import resource
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
args = parser.parse_args()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
tests = Path(__file__).resolve().parent
port, root = tests.parent, tests.parents[2]
vendor = port / 'vendor/metalio_display'
mutations = [
    ('stop-separated-read', 'power.c',
     'hal_i2c_status_t r = hal_i2c_write(p, &reg, 1);\n    return r == HAL_I2C_OK ? hal_i2c_write_read(p, NULL, 0, out, n) : r;',
     'return hal_i2c_write_read(p, &reg, 1, out, n);'),
    ('off-failure-must-stop', 'epd_power.inc',
     'if (set_ctrl_with_tps(state, &mask, CFG_PIN_VCOM_CTRL | CFG_PIN_PWRUP, 0) != ESP_OK) return;',
     '(void)set_ctrl_with_tps(state, &mask, CFG_PIN_VCOM_CTRL | CFG_PIN_PWRUP, 0);'),
    ('xoe-before-rails', 'epd_power.inc', '    esp_rom_delay_us(12);', ''),
    ('vcom-native-order', 'epd_power.inc',
     'err = tps_write_register(s_ioexp_port, 3, value & 0xff);',
     'err = tps_write_register(s_ioexp_port, 3, 0);'),
]
args.build.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='panel-power-negative-', dir=args.build.resolve()) as temp:
    work = Path(temp)
    for name, filename, old, new in [('baseline', '', '', '')] + mutations:
        for source in vendor.iterdir():
            if source.is_file():
                shutil.copyfile(source, work / source.name)
        if filename:
            p = work / filename
            text = p.read_text()
            assert text.count(old) == 1, name
            p.write_text(text.replace(old, new))
        command = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-UNDEBUG']
        for p in [tests / 'display_stub', port, root / 'platform/include', root / 'drivers/chips', root / 'boards']:
            command += ['-I', str(p)]
        command += [f'-DINK_DISPLAY_POWER_SOURCE="{work / "power.c"}"', str(tests / 'display_power.c'),
                    str(root / 'drivers/chips/tca9555/tca9555.c'), '-o', str(work / 'test')]
        built = subprocess.run(command, capture_output=True, text=True)
        assert built.returncode == 0, built.stderr
        result = subprocess.run([str(work / 'test')], capture_output=True, text=True)
        assert (result.returncode == 0) == (name == 'baseline'), f'{name}: {result.stderr}'
        print(f'{name}: compile=0, test={result.returncode}')
print(f'factory panel power reverse controls: {len(mutations)}/{len(mutations)} rejected; baseline passed')
