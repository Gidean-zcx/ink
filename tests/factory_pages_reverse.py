"""Compile replacement objects and prove the reset regressions fail without each change."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'factory-pages-reverse'
area.mkdir(parents=True, exist_ok=True)
cases = [
    ('card-choice', 'system/policy/system_actions.c',
     '    ink_args_get_bool(&c->args, "clear_media", &r.media);\n', '',
     'file /family/books/test-only.txt present=0'),
    ('about-entry', 'system/ui/shell/settings_pages.c',
     'NULL, "factory.open", NULL, NULL, NULL}};',
     'NULL, "system.factory_reset", NULL, NULL, NULL}};',
     'about page opens review instead of destructive backend'),
    ('done-flow', 'system/ui/shell/boot_flow.c',
     '    if (ink_factory_reset_completed()) { ink_factory_done_open(); return; }\n', '',
     'page FactoryDone is presented'),
    ('unbacked', 'system/services/backup/backup_service.c',
     '    int pending = changes ? unbacked() : -1;',
     '    int pending = changes ? (unbacked(), 0) : -1;',
     'page reports actual unbacked changes'),
    ('password-policy', 'system/policy/system_actions.c',
     '.ask_by_hand = true, .parent_pin = true, .child_ok = false',
     '.ask_by_hand = true, .parent_pin = false, .child_ok = false',
     'reset uses existing parent password confirmation'),
]
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
commands, summary = [], []


def run(cmd, **kwargs):
    commands.append([str(part) for part in cmd])
    return subprocess.run(cmd, cwd=build, capture_output=True, text=True, timeout=180, **kwargs)


for name, unit, needle, replacement, expected in cases:
    source = ROOT / unit
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    original = source.read_text()
    assert original.count(needle) == 1, name
    obj, binary = area / (name + '.o'), area / name
    link = shlex.split((build / 'CMakeFiles/sim-factory-pages.dir/link.txt').read_text())
    compiled = run([link[0], *options, '-iquote', str(source.parent), '-x', 'c', '-', '-c', '-o', str(obj)],
                   input=original.replace(needle, replacement))
    assert compiled.returncode == 0, compiled.stderr
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, arg in enumerate(link) if arg.endswith('libink_system.a')), str(obj))
    linked = run(link)
    assert linked.returncode == 0, linked.stderr
    result = run([os.sys.executable, str(ROOT / 'tests/factory_pages.py'), '--build', str(build), '--binary', str(binary)],
                 env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode != 0 and expected in result.stdout + result.stderr, (name, result.stdout, result.stderr)
    assert hashlib.sha256(source.read_bytes()).hexdigest() == before, 'product sources must remain untouched'
    summary.append({'case': name, 'exit': result.returncode, 'expected': expected})
    print(f'{name}: expected failure, exit={result.returncode}', flush=True)
    if not os.environ.get('INK_KEEP_EVIDENCE'):
        shutil.rmtree(build / 'test-out' / ('factory-pages-' + name))
        obj.unlink()
        binary.unlink()
(area / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
