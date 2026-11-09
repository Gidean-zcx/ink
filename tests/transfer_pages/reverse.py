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
a = p.parse_args()
build = a.build.resolve()
area = build / 'transfer-reverse'
area.mkdir(exist_ok=True)
source = ROOT / 'system/ui/shell/transfer_pages.c'
original = source.read_text()
before = hashlib.sha256(source.read_bytes()).hexdigest()
flags = (build / 'CMakeFiles/sim-transfer-pages.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
driver = (ROOT / 'tests/transfer_pages/device.c').read_text().replace(
    '#include "../../system/ui/shell/transfer_pages.c"', '#include "mutant.c"').replace(
    '#include "../services_finish/product.h"', f'#include "{ROOT}/tests/services_finish/product.h"')
cases = [
    ('inline-status', '    P.pending = true;\n    if (ink_cap_call(cap, NULL, &opts)) P.pending = false;',
     '    P.pending = ink_cap_call(cap, NULL, &opts) == 0;', 'USB waits for adult confirmation'),
    ('close', '    if (!P.usb && !P.switching) {', '    if (false && !P.usb && !P.switching) {', 'back closes wireless service'),
    ('progress', 'ink_json_int(j, ink_json_find(j, 0, "received"), &received);', '(void)received;', 'live page shows bytes actually received'),
    ('child', '.effect = INK_FX_NAVIGATE, .run = open_page', '.effect = INK_FX_NAVIGATE, .child_ok = true, .run = open_page', 'child direct wireless navigation is rejected'),
    ('bad-card', 'if (card) ink_ui_pop(); else show(USB_FAIL);', '(void)card; ink_ui_pop();', 'page TransferUsbFail is presented'),
]
commands, summary = [], []


def run(cmd, **kwargs):
    commands.append([str(s) for s in cmd])
    return subprocess.run(cmd, cwd=build, capture_output=True, text=True, timeout=120, **kwargs)


for name, old, new, expected in cases:
    assert old in original, name
    work = area / name
    work.mkdir(exist_ok=True)
    (work / 'mutant.c').write_text(original.replace(old, new))
    (work / 'driver.c').write_text(driver)
    link = shlex.split((build / 'CMakeFiles/sim-transfer-pages.dir/link.txt').read_text())
    obj = work / 'mutant.o'
    compiled = run([link[0], *options, '-c', str(work / 'driver.c'), '-o', str(obj)])
    assert compiled.returncode == 0, compiled.stderr
    link = [str(obj) if s.endswith('tests/transfer_pages/device.c.o') else s for s in link]
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
assert hashlib.sha256(source.read_bytes()).hexdigest() == before
(area / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
