"""Recompile one-place regressions against the actual vendor/HAL host target; no Git history or device."""
import argparse
import json
from pathlib import Path
import resource
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('build', type=Path)
p.add_argument('--cc', default=None)
p.add_argument('--sanitize', action='store_true')
p.add_argument('--case', action='append')
a = p.parse_args()
build = a.build.resolve()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
db = {Path(e['file']).resolve(): e for e in json.loads((build / 'compile_commands.json').read_text())}
vendor = ROOT / 'vendor/uart-eth-modem'
port = ROOT / 'ports/esp32s31'
modem = vendor / 'uart_eth_modem.cc'
bridge = port / 's31_uhci.cc'
life = vendor / 'nt26_lifecycle.cc'
log = vendor / 'ink_modem_log.h'
header = vendor / 'uart_eth_modem.h'
cases = [
    ('detect-rebuild', bridge, 'modem->Stopped() && ink_cell.retry_needed_', 'false && modem->Stopped() && ink_cell.retry_needed_', 'detect'),
    ('frame-checksum', header, '(sum >> 8) ^ sum ^ 0x03', '(sum >> 8) ^ sum ^ 0x04', 'protocol'),
    ('force-wake', modem, '            EnterActiveState();\n        }\n    } else if (state == WorkingState::Active)',
     '            return;\n        }\n    } else if (state == WorkingState::Active)', 'protocol'),
    ('control-not-data', modem, 'frame.length >= 18 &&', 'frame.length >= 4 &&', 'protocol'),
    ('vendor-policy', modem, 'if (!ink_s31_at_allowed(cmd.c_str())) {', 'if (false && !ink_s31_at_allowed(cmd.c_str())) {', 'nat'),
    ('response-chunks', modem, 'else at_command_response_ += "\\r\\n" + line;', 'else at_command_response_ = "\\r\\n" + line;', 'protocol'),
    ('channel-urc', modem,
     'if (response == "+ECNETDEVCTL: 0" || response == "+ECNETDEVCTL: 1" || response == "ECRDY" || response == "+ECRDY") {',
     'if (false && (response == "+ECNETDEVCTL: 0" || response == "+ECNETDEVCTL: 1" || response == "ECRDY" || response == "+ECRDY")) {', 'protocol'),
    ('dhcp-rebuild', bridge,
     'if (modem && modem->Netif() && !modem->HasIp() && esp_timer_get_time() - ink_cell.last_start_us_ >= 60000000)\n            ink_cell.retry_needed_ = true;',
     'if (modem && modem->Netif() && !modem->HasIp() && esp_timer_get_time() - ink_cell.last_start_us_ >= 60000000)\n            ink_cell.retry_needed_ = false;', 'dhcp'),
    ('dma-return', modem, '// Return buffer to pool\n    uart_uhci_.ReturnBuffer(buffer);', '// Return buffer to pool\n    (void)buffer;', 'protocol'),
    ('rx-gap', modem, '            at_line_.clear();\n            at_desynced_ = true;', '            (void)at_line_;\n            at_desynced_ = true;', 'protocol'),
    ('rebuild-throttle', life, 'esp_timer_get_time() - last_start_us_.load() < 5000000', 'esp_timer_get_time() - last_start_us_.load() < 0', 'throttle'),
    ('radio-once', modem, 'state == 1 && !ink_radio_restarted.exchange(true)', 'state == 1', 'silent'),
    ('vendor-log', log, '#define ESP_LOGI(tag, event, ...) ink_s31_vendor_event(tag, event, ##__VA_ARGS__)',
     '#define ESP_LOGI(tag, event, ...) ink_test_log(event, ##__VA_ARGS__)', 'protocol'),
]
if a.case:
    cases = [c for c in cases if c[0] in a.case]
    assert len(cases) == len(a.case)
link_dir = build / 'CMakeFiles/test-s31-cell.dir'
link = shlex.split((link_dir / 'link.txt').read_text())
if a.sanitize:
    assert any('-fsanitize=' in arg for arg in link), 'configure the host target with INK_SANITIZE=ON first'


def run(command, **kw):
    result = subprocess.run(command, capture_output=True, text=True, **kw)
    return result


def baseline(mode):
    result = run([str(build / 'test-s31-cell'), mode], cwd=build, timeout=60)
    if result.returncode:
        raise SystemExit(f'baseline-{mode}: test={result.returncode}\n{result.stdout}\n{result.stderr}')
    print(f'baseline-{mode}: test=0', flush=True)


for mode in dict.fromkeys(c[-1] for c in cases):
    baseline(mode)
with tempfile.TemporaryDirectory(prefix='cell-negative-', dir=build) as temp:
    area = Path(temp)
    for name, source, old, new, mode in cases:
        current = area / name
        current.mkdir()
        copy_vendor = current / 'vendor'
        shutil.copytree(vendor, copy_vendor)
        target = copy_vendor / source.name if source.parent == vendor else current / source.name
        text = source.read_text()
        assert text.count(old) == 1, name
        target.write_text(text.replace(old, new))
        units = [source]
        if source == header:
            units = [modem]
        if source == log:
            units = [modem, ROOT / 'vendor/iot_eth/iot_eth.c', ROOT / 'vendor/iot_eth/iot_eth_netif_glue.c']
        replacements = {}
        for index, unit in enumerate(units):
            e = db[unit.resolve()]
            args = shlex.split(e['command'])
            if a.cc and unit.suffix == '.c':
                args[0] = a.cc
            original_object = Path(e['directory']) / args[args.index('-o') + 1]
            output = current / f'{index}.o'
            copied = copy_vendor / unit.name if unit.parent == vendor else target if unit == source else unit
            args = [str(copied) if x == str(unit) else x for x in args]
            args[args.index('-o') + 1] = str(output)
            args[1:1] = ['-I', str(copy_vendor)]
            result = run(args, cwd=e['directory'], timeout=60)
            if result.returncode:
                raise SystemExit(f'{name}: compile={result.returncode}\n{result.stderr}')
            replacements[original_object.resolve()] = output
        binary = current / 'test'
        args = list(link)
        for i, arg in enumerate(args):
            candidate = (build / arg).resolve()
            if candidate in replacements:
                args[i] = str(replacements[candidate])
        args[args.index('-o') + 1] = str(binary)
        result = run(args, cwd=build, timeout=60)
        if result.returncode:
            raise SystemExit(f'{name}: link={result.returncode}\n{result.stderr}')
        result = run([str(binary), mode], cwd=current, timeout=60)
        if not result.returncode:
            raise SystemExit(f'{name}: regression was not detected')
        evidence = result.stderr.strip().splitlines()
        print(f'{name}: compile=0 link=0 test={result.returncode}; {evidence[-1] if evidence else "no stderr"}', flush=True)
print(f'cell reverse controls: {len(cases)}/{len(cases)} rejected; all baselines passed')
