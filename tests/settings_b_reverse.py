"""Remove the adult-page check in a replacement object, leaving sources intact."""
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
area = build / 'settings-b-reverse'
area.mkdir(parents=True, exist_ok=True)
source = ROOT / 'system/ui/shell/settings_family.c'
original = source.read_bytes()
needle = '    if (ink_member_current()->role == INK_ROLE_CHILD || !ink_member_space_open()) return;'
assert original.decode().count(needle) == 1
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
link = shlex.split((build / 'CMakeFiles/sim-settings-b.dir/link.txt').read_text())
commands = []

def run(command, **kwargs):
    commands.append([str(x) for x in command])
    return subprocess.run(command, cwd=build, capture_output=True, text=True, timeout=240, **kwargs)

text = original.decode()
epoch = text.replace('uint32_t generation;', 'uint32_t epoch, generation;').replace(
    'S.body && ink_ui_page_current()', 'S.body && S.epoch == ink_member_epoch()').replace(
    'S.page = (int)(intptr_t)arg; S.generation++;', 'S.page = (int)(intptr_t)arg; S.epoch = ink_member_epoch(); S.generation++;')
assert epoch != text and 'S.epoch = ink_member_epoch()' in epoch
summary = []
for name, changed, expected, failures in [
    ('page-guard', text.replace(needle, ''), 'child direct page guard denies all five pages', 5),
    ('pin-epoch', epoch, 'page has an AI snapshot', 2),
]:
    obj, binary = area / (name + '.o'), area / name
    compiled = run([link[0], *options, '-iquote', str(source.parent), '-x', 'c', '-', '-c', '-o', str(obj)], input=changed)
    assert compiled.returncode == 0, compiled.stderr
    command = link.copy()
    command[command.index('-o') + 1] = str(binary)
    command.insert(next(i for i, part in enumerate(command) if part.endswith('libink_system.a')), str(obj))
    linked = run(command)
    assert linked.returncode == 0, linked.stderr
    result = run([os.sys.executable, str(ROOT / 'tests/settings_b.py'), '--build', str(build), '--binary', str(binary)],
                 env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode != 0 and expected in result.stdout + result.stderr
    metrics = json.loads((build / 'test-out' / ('settings-b-' + name) / 'write/metrics.json').read_text())['metrics']
    assert metrics.get('test.finished') == 1 and metrics.get('test.fail', 0) >= failures
    summary.append({'case': name, 'exit': result.returncode, 'failures': metrics['test.fail'], 'finished': metrics['test.finished']})
    print(f'PASS: {name} causes {metrics["test.fail"]} failures; production source unchanged', flush=True)
assert original == source.read_bytes()
(area / 'summary.json').write_text(json.dumps({'cases': summary, 'source_sha256': hashlib.sha256(original).hexdigest()}, indent=2) + '\n')
(area / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
