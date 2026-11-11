"""Manual capacity experiment: compile a copy, leave the shared registry untouched."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'games2-capacity'
area.mkdir(exist_ok=True)
source = ROOT / 'system/policy/cap.c'
digest = hashlib.sha256(source.read_bytes()).hexdigest()
original = source.read_text()
assert original.count('#define MAX_CAPS 512') == 1
copy = area / 'cap.c'
copy.write_text(original.replace('#define MAX_CAPS 512', '#define MAX_CAPS 544'))
entry = next(e for e in json.loads((build / 'compile_commands.json').read_text()) if Path(e['file']) == source)
cmd = shlex.split(entry['command']) if 'command' in entry else entry['arguments'].copy()
obj = area / 'cap.o'
cmd[cmd.index('-o') + 1] = str(obj)
cmd = [str(copy) if v == str(source) else v for v in cmd] + ['-I' + str(source.parent)]
subprocess.run(cmd, cwd=entry['directory'], check=True)
link = shlex.split((build / 'CMakeFiles/sim-settings-b.dir/link.txt').read_text())
binary = area / 'sim-settings-b'
link[link.index('-o') + 1] = str(binary)
link.insert(next(i for i, v in enumerate(link) if v.endswith('libink_system.a')), str(obj))
subprocess.run(link, cwd=build, check=True)
result = subprocess.run([sys.executable, str(ROOT / 'tests/settings_b.py'), '--build', str(build), '--binary', str(binary)],
    cwd=ROOT, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, capture_output=True, text=True, timeout=360)
(area / 'run.log').write_text(result.stdout + result.stderr)
print(result.stdout + result.stderr)
assert hashlib.sha256(source.read_bytes()).hexdigest() == digest, 'shared source changed'
assert result.returncode == 0, 'capacity-only copy must pass the unchanged settings test'
print('capacity-only copy: exit=0, shared source unchanged')
