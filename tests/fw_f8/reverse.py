"""Link replacement navigation objects; never edit product sources for negative controls."""
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
area = build / 'fw-f8-reverse'
area.mkdir(parents=True, exist_ok=True)
source_path = ROOT / 'system/ui/shell/navigation.c'
source = source_path.read_text()
digest = hashlib.sha256(source_path.read_bytes()).hexdigest()
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
cases = [
    ('ignore-order', 'if (!T.saved || !slot) return', 'if (true) return', 'four selected cards retain their exact order'),
    ('shared-member', 'ink_db_open_member(&TODAY_DB, ink_member_current()->id)', 'ink_db_open_member(&TODAY_DB, "owner")',
     'adult choices and adult card data do not affect the child default Today'),
    ('swallow-short', 'LV_EVENT_SHORT_CLICKED, (void *)id', 'LV_EVENT_LONG_PRESSED_REPEAT, (void *)id', 'short press still opens the application'),
    ('replace-shortcut', 'ink_shell_bind(resume, card->action, NULL)', 'ink_shell_bind(resume, "shell.close", NULL)',
     'menu shortcut uses the app capability and dismisses the menu'),
]
summary = []
for name, needle, replacement, expected in cases:
    assert needle in source, name
    link = shlex.split((build / 'CMakeFiles/sim-fw-f8.dir/link.txt').read_text())
    obj, binary = area / (name + '.o'), area / name
    compile_cmd = [link[0], *options, '-iquote', str(source_path.parent), '-x', 'c', '-', '-c', '-o', str(obj)]
    compiled = subprocess.run(compile_cmd, input=source.replace(needle, replacement), cwd=build, capture_output=True, text=True, timeout=90)
    assert compiled.returncode == 0, compiled.stderr
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, part in enumerate(link) if part.endswith('libink_system.a')), str(obj))
    linked = subprocess.run(link, cwd=build, capture_output=True, text=True, timeout=90)
    assert linked.returncode == 0, linked.stderr
    command = [os.sys.executable, str(ROOT / 'tests/fw_f8/run.py'), '--build', str(build), '--binary', str(binary)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=140, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    output = result.stdout + result.stderr
    (area / (name + '.log')).write_text(output)
    assert result.returncode != 0 and expected in output, (name, output)
    metrics = json.loads((build / 'test-out' / ('fw-f8-' + name) / 'write/metrics.json').read_text())['metrics']
    assert metrics.get('test.finished') == 1 and metrics.get('test.fail', 0) > 0
    summary.append({'case': name, 'exit': result.returncode, 'failures': metrics['test.fail'], 'expected': expected})
    print(f'{name}: expected failure ({int(metrics["test.fail"])} checks)', flush=True)
assert hashlib.sha256(source_path.read_bytes()).hexdigest() == digest
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
