"""Remove each policy check in replacement objects; leave product sources intact."""
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
area = build / 'settings-a-reverse'
area.mkdir(parents=True, exist_ok=True)
cases = [
    ('page-guard', 'system/ui/shell/settings_pages.c',
     '    if (ink_member_current()->role == INK_ROLE_CHILD || !ink_member_space_open()) return;', '',
     'child cannot open a private settings page through the direct presenter path'),
    ('setting-guard', 'system/policy/settings.c',
     'if (ink_member_current()->role == INK_ROLE_CHILD && (d->system || !d->child_may_change)) {', 'if (false) {',
     'child cannot write the adult voice setting via AI')]
before = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for _, path, *_ in cases}
commands, summary = [], []

def run(args, **kwargs):
    commands.append([str(item) for item in args])
    return subprocess.run(args, cwd=build, capture_output=True, text=True, timeout=180, **kwargs)

flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
for name, path, needle, replacement, expected in cases:
    source = (ROOT / path).read_text()
    assert source.count(needle) == 1, name
    link = shlex.split((build / 'CMakeFiles/sim-settings-a.dir/link.txt').read_text())
    obj, binary = area / (name + '.o'), area / name
    compiled = run([link[0], *options, '-iquote', str((ROOT / path).parent), '-x', 'c', '-', '-c', '-o', str(obj)],
                   input=source.replace(needle, replacement))
    assert compiled.returncode == 0, compiled.stderr
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, part in enumerate(link) if part.endswith('libink_system.a')), str(obj))
    linked = run(link)
    assert linked.returncode == 0, linked.stderr
    result = run([os.sys.executable, str(ROOT / 'tests/settings_a.py'), '--build', str(build), '--binary', str(binary)],
                 env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode != 0 and expected in result.stdout + result.stderr, (name, result.stdout, result.stderr)
    summary.append({'case': name, 'exit': result.returncode, 'expected_failure': expected})
    print(f'{name}: rejected (exit={result.returncode})', flush=True)
assert before == {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for _, path, *_ in cases}
(area / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
