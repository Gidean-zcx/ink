"""Manual reverse check: remove the composer's draft handoff in a temporary copy, never in product sources."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
args = parser.parse_args()
build = args.build.resolve()
area = build / 'test-out' / 'sms-rework-negative'
area.mkdir(parents=True, exist_ok=True)
source = ROOT / 'tests/sms_multipart/sms_multipart.c'
app = (ROOT / 'system/apps/sms/sms.c').read_text()
old = 'ink_args_str(a, "page", COMPOSE.name); ink_args_int(a, "draft", draft); request("message.send", a);'
new = 'ink_args_str(a, "text", ink_textfield_text(S.field)); request("message.send", a);'
assert app.count(old) == 1, 'draft handoff moved: review the mutation'
mutant = area / 'sms.c'
mutant.write_text(app.replace(old, new))
test = source.read_text().replace('"../../system/apps/sms/sms.c"', f'"{mutant}"')
test = test.replace('"../../system/boot/main.c"', f'"{ROOT}/system/boot/main.c"')
test = test.replace('"../integration/whole_product.h"', f'"{ROOT}/tests/integration/whole_product.h"')
copy = area / 'sms_multipart.c'
copy.write_text(test)
entry = next(e for e in json.loads((build / 'compile_commands.json').read_text()) if Path(e['file']) == source)
command = shlex.split(entry['command']) if 'command' in entry else entry['arguments']
obj = area / 'sms_multipart.o'
compile_args = []
skip = False
for arg in command:
    if skip:
        skip = False
        continue
    if arg in ('-o', '-MF', '-MT', '-MQ'):
        skip = True
    elif arg not in ('-MD', '-MMD'):
        compile_args.append(str(copy) if arg == str(source) else arg)
subprocess.run([*compile_args, '-o', str(obj)], cwd=entry['directory'], check=True)
link = shlex.split((build / 'CMakeFiles/sim-sms-multipart.dir/link.txt').read_text())
link = [str(obj) if arg.endswith('tests/sms_multipart/sms_multipart.c.o') else arg for arg in link]
binary = area / 'sim-sms-mutant'
link[link.index('-o') + 1] = str(binary)
subprocess.run(link, cwd=build, check=True)
out = area / 'compose'
result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--script', str(ROOT / 'tests/sms_multipart/short.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
    '--quiet', '--no-random-outages'], env={**os.environ, 'INK_SMS_PHASE': 'compose'}, capture_output=True, text=True, timeout=300)
(area / 'process.log').write_text(result.stdout + result.stderr)
data = json.loads((out / 'metrics.json').read_text())
failed = data.get('metrics', {}).get('test.fail', 0)
assert result.returncode != 0 and failed >= 3, f'reverse check did not expose the regression: exit={result.returncode}, failures={failed}'
for expected in ('physical send loads the full draft', "no part before the person's yes", 'after the yes the composer sends'):
    assert any(expected in warning for warning in data.get('warnings', [])), f'missing regression evidence: {expected}'
print(f'reverse check: PASS (mutant exit={result.returncode}, failed checks={failed}; product sources unchanged)')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
