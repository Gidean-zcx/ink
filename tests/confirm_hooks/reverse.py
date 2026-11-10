"""Disable the confirmation hook in a separate binary; leave product files untouched."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
build = parser.parse_args().build.resolve()
area = build / 'confirm-hooks-reverse'
area.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'system/policy/confirm.c').read_text()
old = 'ink_rc_t prepared = c->prepare_confirm(&h->args, rc);'
assert source.count(old) == 1, 'the hook call must be unique'
mutant = area / 'confirm.c'
mutant.write_text(source.replace(old, 'ink_rc_t prepared = INK_RC_DONE;', 1))
flags = {}
for line in (build / 'CMakeFiles/ink_system.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
link = shlex.split((build / 'CMakeFiles/sim-confirm.dir/link.txt').read_text())
obj, binary = area / 'confirm.o', area / 'sim-confirm-no-hook'
subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                '-c', str(mutant), '-o', str(obj)], cwd=build, check=True)
link[link.index('-o') + 1] = str(binary)
link.insert(next(i for i, part in enumerate(link) if part.endswith('.a')), str(obj))
subprocess.run(link, cwd=build, check=True)
ctest = str(Path(sys.executable).with_name('ctest'))
catalogue = json.loads(subprocess.check_output(
    [ctest, '--test-dir', str(build), '--show-only=json-v1'], text=True))
command = next(test['command'] for test in catalogue['tests'] if test['name'] == 'integration-confirm')
command[0] = str(binary)
out = area / 'out'
command[command.index('--out') + 1] = str(out)
result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
(area / 'test.log').write_text(result.stdout + result.stderr)
data = json.loads((out / 'metrics.json').read_text())
expected = ["the card uses the hook's context and explanation",
            'execution checks the original binding without preparing it again',
            'a failed hook returns its own status and reason',
            'a failed hook neither waits nor runs nor emits a confirmation result']
assert result.returncode != 0 and data['metrics'].get('test.finished') == 1
for message in expected:
    assert any(message in warning for warning in data.get('warnings', [])), message
summary = {'compile_exit': 0, 'link_exit': 0, 'test_exit': result.returncode,
           'failures': data['metrics'].get('test.fail', 0), 'expected_failures': expected}
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(summary, ensure_ascii=False, indent=2))
