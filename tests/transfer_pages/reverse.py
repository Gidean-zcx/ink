"""Remove one page behavior in an ignored source copy and require a test failure."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--case', choices=('inline-status', 'close', 'progress', 'child', 'bad-card', 'entry-layout', 'host', 'qr', 'no-host', 'status-host', 'child-start', 'review'))
a = p.parse_args()
build = a.build.resolve()
area = build / 'transfer-reverse'
area.mkdir(exist_ok=True)
source = ROOT / 'system/ui/shell/transfer_pages.c'
sources = {'ui': source, 'files': ROOT / 'system/apps/files/files.c', 'service': ROOT / 'system/services/file/transfer.c'}
originals = {unit: path.read_text() for unit, path in sources.items()}
before = {unit: hashlib.sha256(path.read_bytes()).hexdigest() for unit, path in sources.items()}
flags = (build / 'CMakeFiles/sim-transfer-pages.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
driver = (ROOT / 'tests/transfer_pages/device.c').read_text().replace(
    '#include "../../system/ui/shell/transfer_pages.c"', '#include "mutant.c"').replace(
    '#include "../services_finish/product.h"', f'#include "{ROOT}/tests/services_finish/product.h"')
cases = [('ui', *case) for case in [
    ('inline-status', '    P.pending = true;\n    if (ink_cap_call(cap, NULL, &opts)) P.pending = false;',
     '    P.pending = ink_cap_call(cap, NULL, &opts) == 0;', 'USB waits for adult confirmation'),
    ('close', '    if (!P.usb && !P.switching) {', '    if (false && !P.usb && !P.switching) {', 'back closes wireless service'),
    ('progress', '!strcmp(result, "文件已收到") ? 1 : 0', '0', 'completion returns to Transfer with actual size and count'),
    ('child', '.effect = INK_FX_NAVIGATE, .run = open_page', '.effect = INK_FX_NAVIGATE, .child_ok = true, .run = open_page', 'child direct wireless navigation is rejected'),
    ('bad-card', 'if (card) ink_ui_pop(); else show(USB_FAIL);', '(void)card; ink_ui_pop();', 'page TransferUsbFail is presented'),
    ('host', '"http://%s:%u/", host, (unsigned)(uint16_t)port', '"http://127.0.0.1:%u/", (unsigned)(uint16_t)port', 'visible address equals status host and port'),
    ('qr', 'lv_qrcode_update(qr, P.address, (uint32_t)strlen(P.address));', 'lv_qrcode_update(qr, "http://example.com/", 19);', 'QR bitmap encodes status host and port'),
    ('no-host', 'else if (P.active) text("还没连上 Wi-Fi", INK_FONT_LIST, middle);', 'else if (P.active) text("尚未开启", INK_FONT_LIST, middle);', 'missing HAL address is visible on page'),
]]
cases += [
    ('service', 'status-host', '"false", host, T.port', '"false", "127.0.0.1", T.port', 'status host equals read-only HAL address'),
    ('service', 'child-start', '.effect = INK_FX_LOCAL_WRITE, .permission = "file", .exec = INK_EXEC_BG, .run = start',
     '.effect = INK_FX_LOCAL_WRITE, .child_ok = true, .permission = "file", .exec = INK_EXEC_BG, .run = start', 'child AI cannot start wireless transfer'),
]
cases.append(('files', 'entry-layout',
    'INK_CONTROL;\n        button(4, screen, "从手机或电脑传入", "transfer.open", NULL, INK_MARGIN, bottom - INK_CONTROL',
    'INK_PRIMARY;\n        button(4, screen, "从手机或电脑传入", "transfer.open", NULL, INK_MARGIN, bottom - INK_PRIMARY',
    'transfer entry clears the file pagination'))
commands, summary = [], []


def run(cmd, **kwargs):
    commands.append([str(s) for s in cmd])
    return subprocess.run(cmd, cwd=build, capture_output=True, text=True, timeout=120, **kwargs)


for unit, name, old, new, expected in cases:
    if a.case and a.case != name and not (a.case == 'review' and name in ('host', 'qr', 'no-host', 'status-host', 'child-start')):
        continue
    original = originals[unit]
    assert old in original, name
    work = area / name
    work.mkdir(exist_ok=True)
    (work / 'mutant.c').write_text(original.replace(old, new))
    (work / 'driver.c').write_text(driver)
    link = shlex.split((build / 'CMakeFiles/sim-transfer-pages.dir/link.txt').read_text())
    obj = work / 'mutant.o'
    compiled = run([link[0], *options, '-I' + str(ROOT / 'system/services/file'), '-c', str(work / ('driver.c' if unit == 'ui' else 'mutant.c')), '-o', str(obj)])
    assert compiled.returncode == 0, compiled.stderr
    if unit == 'ui':
        link = [str(obj) if s.endswith('tests/transfer_pages/device.c.o') else s for s in link]
    else:
        link.insert(next(i for i, s in enumerate(link) if s.endswith('libink_system.a')), str(obj))
    binary = work / 'sim-transfer-pages'
    link[link.index('-o') + 1] = str(binary)
    linked = run(link)
    assert linked.returncode == 0, linked.stderr
    result = run([os.sys.executable, str(ROOT / 'tests/transfer_pages/run.py'), '--build', str(build), '--binary', str(binary)],
                 env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    output = result.stdout + result.stderr
    (work / 'run.log').write_text(output)
    assert result.returncode != 0 and expected in output, (name, output[-7000:])
    summary.append({'case': name, 'exit': result.returncode, 'expected_failure': expected})
    print(f'{name}: expected test failure (exit {result.returncode})', flush=True)
assert {unit: hashlib.sha256(path.read_bytes()).hexdigest() for unit, path in sources.items()} == before
suffix = '-' + a.case if a.case else ''
(area / ('commands' + suffix + '.json')).write_text(json.dumps(commands, indent=2) + '\n')
(area / ('summary' + suffix + '.json')).write_text(json.dumps(summary, indent=2) + '\n')
