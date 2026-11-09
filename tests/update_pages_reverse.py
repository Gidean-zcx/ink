"""Link the baseline presenter in a separate executable; do not edit product files."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'update-pages-reverse'
area.mkdir(exist_ok=True)
path = ROOT / 'system/ui/shell/update_pages.c'
before = hashlib.sha256(path.read_bytes()).hexdigest()
source = subprocess.run(['git', 'show', '3fce2beea07d9e25a814cd24c64e8d5305ea9222:system/ui/shell/update_card.c'],
                        cwd=ROOT, check=True, capture_output=True, text=True).stdout
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
link = shlex.split((build / 'CMakeFiles/sim-update-pages.dir/link.txt').read_text())
obj, binary = area / 'baseline.o', area / 'baseline'
subprocess.run([link[0], *options, '-iquote', str(ROOT / 'system/ui/shell'), '-x', 'c', '-', '-c', '-o', str(obj)],
               cwd=build, input=source, text=True, check=True, capture_output=True)
link[link.index('-o') + 1] = str(binary)
link.insert(next(i for i, arg in enumerate(link) if arg.endswith('libink_system.a')), str(obj))
subprocess.run(link, cwd=build, check=True, capture_output=True)
result = subprocess.run([os.sys.executable, str(ROOT / 'tests/update_pages.py'), '--build', str(build),
                         '--binary', str(binary), '--mode', 'UpdateReady'], cwd=ROOT,
                        env=dict(os.environ, INK_KEEP_EVIDENCE='1'), capture_output=True, text=True, timeout=90)
(area / 'baseline.log').write_text(result.stdout + result.stderr)
data = json.loads((build / 'test-out/update-pages-baseline/UpdateReady/metrics.json').read_text())
counts = data['metrics']
assert result.returncode != 0 and counts.get('test.finished') == 1 and counts.get('test.fail', 0) > 0
assert 'AI snapshot names the visible update page' in result.stderr
assert hashlib.sha256(path.read_bytes()).hexdigest() == before
summary = {'baseline_exit': result.returncode, 'test_fail': counts['test.fail'], 'test_finished': counts['test.finished']}
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(summary)
