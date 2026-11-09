"""Compile two replacement objects; verify the acceptance test rejects each regression."""
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
area = build / 'bluetooth-reverse'
area.mkdir(parents=True, exist_ok=True)
source_path = ROOT / 'system/services/bt/bt_service.c'
before = hashlib.sha256(source_path.read_bytes()).hexdigest()
source = source_path.read_text()
cases = [
    ('enabled-gate', 'if (!ink_setting_get("bt.enabled")) {', 'if (false) {',
     'disabled gate rejects scanning'),
    ('pair-connect', 'if (ink_args_get_bool(&c->args, "connect", &connect) && connect) cap_connect(c);',
     '(void)connect;', 'a discovered row pairs and connects'),
]
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
commands, summary = [], []


def run(args, **kwargs):
    commands.append([str(item) for item in args])
    return subprocess.run(args, cwd=build, capture_output=True, text=True, timeout=180, **kwargs)


for name, needle, replacement, expected in cases:
    assert source.count(needle) == 1, name
    link = shlex.split((build / 'CMakeFiles/sim-bluetooth.dir/link.txt').read_text())
    obj, binary = area / (name + '.o'), area / name
    result = run([link[0], *options, '-iquote', str(source_path.parent), '-x', 'c', '-', '-c', '-o', str(obj)],
                 input=source.replace(needle, replacement))
    assert result.returncode == 0, result.stderr
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, part in enumerate(link) if part.endswith('libink_system.a')), str(obj))
    result = run(link)
    assert result.returncode == 0, result.stderr
    result = run([os.sys.executable, str(ROOT / 'tests/bluetooth.py'), '--build', str(build), '--binary', str(binary)],
                 env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    path = build / 'test-out' / ('bluetooth-' + name) / 'write/metrics.json'
    data = json.loads(path.read_text())
    counts = data['metrics']
    assert result.returncode != 0 and counts.get('test.finished') == 1 and counts.get('test.fail', 0) > 0
    assert expected in json.dumps(data['warnings'], ensure_ascii=False), (name, data['warnings'])
    summary.append({'case': name, 'exit': result.returncode, 'failures': counts['test.fail'], 'evidence': expected})
    print(f'{name}: rejected (exit={result.returncode}, failures={counts["test.fail"]})', flush=True)
assert before == hashlib.sha256(source_path.read_bytes()).hexdigest()
(area / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
