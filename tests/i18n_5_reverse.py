"""Manual control: remove only the long-answer note translation in an isolated object."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', required=True, type=Path)
build = parser.parse_args().build.resolve()
area = build / 'i18n-5-reverse'
area.mkdir(exist_ok=True)
source = ROOT / 'system/services/ai/ai_service.c'
original = source.read_bytes()
before = hashlib.sha256(original).hexdigest()
needle = 'const char *cut = TR(INK_AI_ANSWER_CUT);'
assert original.decode().count(needle) == 1
changed, obj, binary = area / 'ai_service.c', area / 'ai_service.o', area / 'sim-cut-untranslated'
changed.write_text(original.decode().replace(needle, 'const char *cut = INK_AI_ANSWER_CUT;'))
db = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in db if Path(e['file']).resolve() == source)
compile_cmd = shlex.split(entry['command'])
compile_cmd[compile_cmd.index('-o') + 1] = str(obj)
compile_cmd[compile_cmd.index(str(source))] = str(changed)
compile_cmd += ['-iquote', str(source.parent)]
link_cmd = shlex.split((build / 'CMakeFiles/sim-ai-answer-long.dir/link.txt').read_text())
link_cmd[link_cmd.index('-o') + 1] = str(binary)
link_cmd.insert(next(i for i, arg in enumerate(link_cmd) if arg.endswith('libink_system.a')), str(obj))
commands = []
for cmd in (compile_cmd, link_cmd):
    result = subprocess.run(cmd, cwd=build, text=True, capture_output=True, timeout=120)
    commands.append({'command': cmd, 'exit': result.returncode})
    assert result.returncode == 0, result.stderr
out = area / 'out'
cmd = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
       '--script', str(ROOT / 'tests/ai_answer_long/ai_answer_long.script'),
       '--hub', str(ROOT / 'tests/ai_answer_long/hub.json'),
       '--budget', str(ROOT / 'tests/ai_answer_long/budget.json'),
       '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
       '--out', str(out), '--quiet', '--no-random-outages']
result = subprocess.run(cmd, cwd=ROOT, env={**os.environ, 'INK_I18N_ANSWER_EN': '1'},
                        text=True, capture_output=True, timeout=180)
commands.append({'command': cmd, 'exit': result.returncode})
(area / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
(area / 'stderr.log').write_text(result.stderr)
data = json.loads((out / 'metrics.json').read_text())
expected = 'the last page ends with the note'
assert result.returncode and any(expected in warning for warning in data['warnings']), data['warnings']
assert hashlib.sha256(source.read_bytes()).hexdigest() == before
print(f'PASS reverse: untranslated note fails "{expected}" (exit={result.returncode}); product source unchanged')
