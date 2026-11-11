#!/usr/bin/env python3
"""Run reverse controls on disposable source copies; no Git history or source edits."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
PORT = Path('ports/esp32s31')
VENDOR = PORT / 'vendor/board_874538e'
BUS = VENDOR / 'main/boards/metalio-e-ink4-plus/bus/metalio_i2c1.c'
POWER = VENDOR / 'main/boards/common/power/power_i2c.c'
CHIPS = [f'drivers/chips/{n}/{n}.c' for n in ('tca9555', 'ft6336u', 'sc7a20h', 'bq27220', 'cx25601n', 'pcf8563')]
BOARD_SOURCES = [str(PORT / 'tests/vendor_board.c'), 'boards/metalio_plus/metalio_board.c',
                 'boards/metalio_plus/metalio_touch.c', *CHIPS]
I2C_SOURCES = [str(PORT / 'tests/vendor_i2c.c'), CHIPS[0]]
IRQ_SOURCES = [str(PORT / 'tests/vendor_irq.c')]
# Each mutation removes one repaired behavior; assertions are unchanged.
CASES = [
    ('recovery', 'i2c', BUS, 'i < 9 &&', 'i < 0 &&'),
    ('stretch', 'i2c', BUS, '.scl_wait_us = 10000,', '.scl_wait_us = 2500,'),
    ('stop', 'i2c', Path(CHIPS[0]), 'return ink_s31_i2c_read_stop(t->dev, reg, value, 1);', 'return hal_i2c_write_read(t->dev, &reg, 1, value, 1);'),
    ('fallback', 'i2c', POWER, 'if (ret != ESP_OK) {', 'if (false) {'),
    ('budgets', 'i2c', PORT / 'hal_i2c_s31.c', 'dev->addr == 0x55 ? 200 : 100', 'dev->addr == 0x55 ? 1000 : 100'),
    ('flash', 'i2c', BUS, '    quiet_idle_interrupts();\n    return true;', '    return true;'),
    ('boot', 'board', VENDOR / 'boot.inc', 'uhci_ll_reset_register(i);', '(void)i;'),
    ('reset', 'board', VENDOR / 'reset.inc', 'vTaskDelay(pdMS_TO_TICKS(POWER_DISCHARGE_MOTOR_PULSE_MS));', 'vTaskDelay(pdMS_TO_TICKS(POWER_DISCHARGE_MOTOR_PULSE_MS / 2));'),
    ('reset', 'board', VENDOR / 'reset.inc', 'vTaskDelay(pdMS_TO_TICKS(10));', 'vTaskDelay(pdMS_TO_TICKS(1));'),
    ('light', 'board', VENDOR / 'light.inc', '#define FL_OUTPUT_MAX_PERCENT 10', '#define FL_OUTPUT_MAX_PERCENT 100'),
    ('touch', 'board', VENDOR / 'touch_map.inc', 'else if (raw_x >= EPD_LOGICAL_W)', 'else if (false)'),
    ('touch', 'board', PORT / 'hal_input_s31.c', 'g_touch ? pdMS_TO_TICKS(20) :', 'g_touch ? portMAX_DELAY :'),
    ('retry', 'board', VENDOR / 'touch_ready.inc', 'i < 5;', 'i < 1;'),
    ('keys', 'board', VENDOR / 'keys.inc', 'kDebounceUs = 30000;', 'kDebounceUs = 20000;'),
    ('keys', 'board', VENDOR / 'keys.inc', 'if (!refresh_p1()) return false;', 'refresh_p1();'),
    ('accel-alt', 'board', PORT / 's31_board.c', '{SC7A20H_ADDR, 0x18}', '{SC7A20H_ADDR, 0x19}'),
    ('accel-fail', 'board', PORT / 's31_board.c', 'sc7a20h_silence(dev);', '(void)dev;'),
    ('irq', 'irq', PORT / 's31_irq.c', 'gpio == S31_GPIO_AI_KEY ? GPIO_INTR_ANYEDGE : GPIO_INTR_NEGEDGE', 'GPIO_INTR_NEGEDGE'),
    ('vibe', 'board', VENDOR / 'vibe.inc', '{0, 14000, 18000, 20000}', '{0, 20000, 18000, 20000}'),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cc', default='cc')
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-UNDEBUG', '-DESP_PLATFORM=1']
    includes = [PORT / 'tests/vendor_stub', PORT / 'tests', PORT, Path('platform/include'), Path('drivers/chips'), Path('boards')]
    flags += [f'-I{ROOT / p}' for p in includes]
    for index, (case, suite, path, old, new) in enumerate(CASES, 1):
        with tempfile.TemporaryDirectory(prefix='vendor-negative-', dir=args.out) as name:
            temp = Path(name)
            shutil.copytree(ROOT / VENDOR, temp / VENDOR)
            for rel in [PORT / 's31_board.c', PORT / 'hal_input_s31.c', PORT / 'hal_i2c_s31.c',
                        PORT / 'tests/vendor_board.c', PORT / 'tests/vendor_i2c.c', PORT / 'tests/vendor_irq.c', PORT / 's31_irq.c', *map(Path, CHIPS)]:
                target = temp / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / rel, target)
                if str(rel).startswith('drivers/chips/'):
                    for header in (ROOT / rel.parent).glob('*.h'):
                        shutil.copyfile(header, target.parent / header.name)
            target = temp / path
            text = target.read_text()
            if text.count(old) != 1:
                raise RuntimeError(f'{case}: mutation is not unique')
            target.write_text(text.replace(old, new))
            sources = I2C_SOURCES if suite == 'i2c' else IRQ_SOURCES if suite == 'irq' else BOARD_SOURCES
            sources = [str(temp / p) if (temp / p).exists() else str(ROOT / p) for p in sources]
            binary = temp / 'negative'
            build = subprocess.run([args.cc, *flags, *sources, '-o', str(binary)], text=True, capture_output=True)
            if build.returncode:
                raise RuntimeError(f'{case}: compile failed (not accepted as a reverse control)\n{build.stderr}')
            run = subprocess.run([str(binary), case], text=True, capture_output=True, timeout=10)
            if run.returncode == 0 or 'Assertion' not in run.stderr and 'assertion' not in run.stderr:
                raise RuntimeError(f'{case}: expected an assertion failure, exit={run.returncode}\n{run.stderr}')
            print(f'{index:02d} {suite}/{case}: compile=0, reverse={run.returncode}, assertion observed')
    print(f'{len(CASES)} reverse controls passed; working sources unchanged')


if __name__ == '__main__':
    main()
