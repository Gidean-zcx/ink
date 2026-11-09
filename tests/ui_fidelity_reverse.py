"""Link the baseline presenters with today's assertions; never edit product files."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline', default='62b0e83da1d34f50629b5e576807db1381b26078')
p.add_argument('--capture-large', action='store_true', help='also capture baseline Settings at the maximum font')
a = p.parse_args()
build = a.build.resolve()
area = build / 'ui-fidelity/reverse'
area.mkdir(parents=True, exist_ok=True)
flags = {}
for line in (build / 'CMakeFiles/ink_system.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
results = []
for filename, target in (('navigation.c', 'sim-ui-scenes'), ('factory_pages.c', 'sim-factory-pages')):
    source = area / filename
    source.write_text(subprocess.check_output(
        ['git', 'show', f'{a.baseline}:system/ui/shell/{filename}'], cwd=ROOT, text=True))
    obj = source.with_suffix('.o')
    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-I', str(ROOT / 'system/ui/shell'), '-c', str(source), '-o', str(obj)], check=True)
    binary = area / (target + '-reverse')
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, part in enumerate(link) if part.endswith('.a')), str(obj))
    subprocess.run(link, cwd=build, check=True)
    if target == 'sim-ui-scenes':
        out = area / 'Settings'
        command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                   '--script', str(ROOT / 'tests/ui_shell.script'), '--hub', str(ROOT / 'tests/ui_shell.hub.json'),
                   '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--assets', str(build / 'assets'),
                   '--content', str(build / 'test-content'), '--out', str(out), '--no-random-outages', '--quiet']
        env = {**os.environ, 'INK_UI_SCENE': 'Settings'}
        metrics = out / 'metrics.json'
    else:
        command = [sys.executable, str(ROOT / 'tests/factory_pages.py'), '--build', str(build), '--binary', str(binary)]
        env = {**os.environ, 'INK_KEEP_EVIDENCE': '1'}
        metrics = build / 'test-out' / ('factory-pages-' + binary.name) / 'keep/keep/metrics.json'
    run = subprocess.run(command, env=env, capture_output=True, text=True, timeout=150)
    (area / (filename + '.log')).write_text(run.stdout + run.stderr)
    counters = json.loads(metrics.read_text())['metrics']
    assert run.returncode != 0 and counters.get('test.finished') == 1 and counters.get('test.fail', 0) > 0
    results.append({'presenter': filename, 'exit': run.returncode,
                    'failures': counters['test.fail'], 'finished': counters['test.finished']})
(area / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
if a.capture_large:
    output = build / 'ui-fidelity/baseline-large'
    output.mkdir(parents=True, exist_ok=True)
    source = output / 'ui_shell_scenes.c'
    driver = subprocess.check_output(['git', 'show', '88817c36:tests/ui_shell_scenes.c'], cwd=ROOT, text=True)
    driver = driver.replace('#include "../system/boot/main.c"', '#include "' + str(ROOT / 'system/boot/main.c') + '"')
    driver = driver.replace('#include "integration/whole_product.h"', '#include "' + str(ROOT / 'tests/integration/whole_product.h') + '"')
    source.write_text(driver)
    for line in (build / 'CMakeFiles/sim-ui-scenes.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / 'CMakeFiles/sim-ui-scenes.dir/link.txt').read_text())
    obj, binary = output / 'scenes.o', output / 'sim-baseline-large'
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-c', str(source), '-o', str(obj)], check=True)
    link = [str(obj) if part.endswith('tests/ui_shell_scenes.c.o') else part for part in link]
    link.insert(next(i for i, part in enumerate(link) if part.endswith('.a')), str(area / 'navigation.o'))
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '--script', str(ROOT / 'tests/ui_shell.script'), '--hub', str(ROOT / 'tests/ui_shell.hub.json'),
               '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--assets', str(build / 'assets'),
               '--content', str(build / 'test-content'), '--out', str(output), '--no-random-outages', '--quiet']
    with (output / 'run.log').open('w') as log:
        run = subprocess.run(command, env={**os.environ, 'INK_UI_SCENE': 'Settings', 'INK_UI_SCENE_TEXT': '3'},
                             stdout=log, stderr=subprocess.STDOUT, timeout=150)
    counters = json.loads((output / 'metrics.json').read_text())['metrics']
    assert run.returncode == 0 and counters.get('test.finished') == counters.get('test.snapshot.settled') == counters.get('test.snapshot.Settings') == 1 and not counters.get('test.fail', 0)
    print('Baseline Settings maximum font: PASS', counters.get('test.pass'))
