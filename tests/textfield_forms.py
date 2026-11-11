"""Compile the focused form driver with the existing build and retain PNG evidence."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--label', required=True)
p.add_argument('--baseline', help='Manual comparison only; compile replacement objects from this commit')
a = p.parse_args()
build = a.build.resolve()
area = build / 'textfield-forms' / a.label
area.mkdir(parents=True, exist_ok=True)
flags = {}
for line in (build / 'CMakeFiles/sim-ui-input-host.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
link = shlex.split((build / 'CMakeFiles/sim-ui-input-host.dir/link.txt').read_text())
obj = area / 'forms.o'
defines = []
objects = []
if a.baseline:
    defines.append('-DINK_TEST_BASELINE=1')
    paths = subprocess.check_output(['git', 'diff', '--name-only', a.baseline, '--', 'system'], cwd=ROOT, text=True).splitlines()
    for path in paths:
        if not path.endswith('.c'):
            continue
        source = area / Path(path).name
        source.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':' + path], cwd=ROOT))
        if path in ('system/ui/textfield.c', 'system/apps/remote/remote.c'):
            macro = 'INK_TEST_TEXTFIELD_SOURCE' if path.endswith('textfield.c') else 'INK_TEST_REMOTE_SOURCE'
            defines.append('-D' + macro + '=' + json.dumps(str(source)))
        else:
            item = source.with_suffix('.o')
            subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                '-I' + str(ROOT / Path(path).parent), '-c', str(source), '-o', str(item)], cwd=build, check=True)
            objects.append(str(item))
subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                *defines, '-c', str(ROOT / 'tests/textfield_forms.c'), '-o', str(obj)], cwd=build, check=True)
link = [str(obj) if word.endswith('/tests/ui_input_host.c.o') else word for word in link]
link[1:1] = objects
binary = area / 'forms'
link[link.index('-o') + 1] = str(binary)
subprocess.run(link, cwd=build, check=True)
for size in ('100', '120'):
    out = area / size
    sd = area / (size + '-sd')
    for folder in (out, sd):
        if folder.exists():
            shutil.rmtree(folder)
    shutil.copytree(ROOT / 'examples/units', sd / 'content/apps/units', dirs_exist_ok=True)
    env = {**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'}
    if size == '120':
        env['INK_FIELD_BIG'] = '1'
    run = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/i18n_apps.script'), '--assets', str(build / 'assets'),
        '--content', str(build / 'test-content'), '--sd-dir', str(sd), '--out', str(out), '--quiet', '--no-random-outages'],
        cwd=ROOT, env=env, capture_output=True, text=True, timeout=240)
    (area / (size + '.log')).write_text(run.stdout + run.stderr)
    data = json.loads((out / 'metrics.json').read_text())
    metrics = data['metrics']
    print(a.label, size, 'exit', run.returncode, 'checks', metrics.get('test.pass', 0),
          'failures', metrics.get('test.fail', 0), flush=True)
    if run.returncode or metrics.get('test.fail', 0) or metrics.get('test.finished') != 1:
        raise SystemExit(run.stderr[-6000:] + str(data.get('warnings', [])))
