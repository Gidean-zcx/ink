"""Synthetic file operations and abrupt restart, with optional UI negative control."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--reverse', nargs='?', const='draft', choices=('draft', 'selection', 'save-status'))
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix='files-ops-', dir=build))
binary = a.binary.resolve() if a.binary else build / 'sim-files-ops'
if a.reverse:
    source = (ROOT / 'system/apps/files/files.c').read_text()
    old, replacement = ('F.ref = rec.id; F.draft = (uint32_t)draft; break;', 'F.ref = rec.id; F.draft = 0; break;') if a.reverse == 'draft' else (
        'if (i < F.marked_count) F.marked[i] = F.marked[--F.marked_count];', 'if (i < F.marked_count) (void)i;')
    if a.reverse == 'save-status':
        old, replacement = 'if (F.mode == EDIT && rc->status != INK_RC_DONE)', 'if (false && F.mode == EDIT && rc->status != INK_RC_DONE)'
    assert source.count(old) == 1
    (work / 'files.c').write_text(source.replace(old, replacement))
    driver = (ROOT / 'tests/files_ops/files_ops.c').read_text().replace(
        '#include "../../system/apps/files/files.c"', '#include "files.c"').replace(
        '#include "../../system/boot/main.c"', f'#include "{ROOT}/system/boot/main.c"').replace(
        '#include "../integration/whole_product.h"', f'#include "{ROOT}/tests/integration/whole_product.h"')
    (work / 'test.c').write_text(driver)
    flags = {}
    for line in (build / 'CMakeFiles/sim-files-ops.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / 'CMakeFiles/sim-files-ops.dir/link.txt').read_text())
    obj = work / 'test.o'
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'], '-c', str(work / 'test.c'), '-o', str(obj)], cwd=build, check=True)
    link = [str(obj) if s.endswith('tests/files_ops/files_ops.c.o') else s for s in link]
    binary = work / 'sim-files-ops'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
summary = []
for phase in ('write', 'recover'):
    out = work / phase
    r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/files_ops/files_ops.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--flash-image', str(work / 'flash.img'),
        '--sd-dir', str(work / 'sd'), '--out', str(out), '--quiet', '--no-random-outages'],
        env={**os.environ, 'INK_FILES_OPS_PHASE': phase}, capture_output=True, text=True, timeout=180)
    (work / f'{phase}.log').write_text(r.stdout + r.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    counts = data.get('metrics', {})
    ok = r.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail', 0)
    summary.append({'phase': phase, 'exit': r.returncode, 'passed': ok, 'checks': counts.get('test.pass', 0), 'failures': counts.get('test.fail', 0)})
    print(summary[-1], flush=True)
    if not ok:
        print(r.stderr[-6000:])
        break
(work / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
passed = len(summary) == 2 and all(s['passed'] for s in summary)
if passed and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(work)
else:
    print('Evidence:', work.relative_to(ROOT), flush=True)
raise SystemExit(0 if passed else 1)
