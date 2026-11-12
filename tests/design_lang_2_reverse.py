"""Manual counterfactual: link one baseline presenter into the final screenshot probe.

Requires the explicitly supplied local baseline; never registered with ctest.
Product sources and the normal build's libraries are not changed.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline', required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'design-lang-2/reverse'
area.mkdir(parents=True, exist_ok=True)
flags = {}
for line in (build / 'CMakeFiles/ink_system.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
cases = [
    ('today', 'system/ui/shell/navigation.c', 'Today', 'Today: text cut off'),
    ('about', 'system/ui/shell/settings_pages.c', 'SetAbout', 'about has no orphan side frame'),
    ('contacts', 'system/apps/contacts/contacts.c', 'Contacts', 'contact list begins below'),
    ('sms', 'system/apps/sms/sms.c', 'SmsThread', 'complete field clears the footer'),
    ('dial', 'system/apps/phone/phone.c', 'Phone', 'dial pad begins below'),
    ('calendar', 'system/apps/calendar/calendar.c', 'Calendar', 'Calendar: text on text'),
    ('empty-pager', 'system/ui/ui_list.c', 'Sms', 'empty SMS page has no pager'),
]
results = []
for name, path, scene, expected in cases:
    source = subprocess.run(['git', 'show', f'{a.baseline}:{path}'], cwd=ROOT,
                            check=True, capture_output=True, text=True).stdout
    original = ROOT / path
    src, obj, binary = area / f'{name}.c', area / f'{name}.o', area / name
    src.write_text(source)
    link = shlex.split((build / 'CMakeFiles/sim-ui-scenes.dir/link.txt').read_text())
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-I', str(original.parent), '-c', str(src), '-o', str(obj)], cwd=build, check=True)
    driver = build / 'design-lang-1/after/driver.o'
    link = [str(driver) if word.endswith('/tests/ui_shell_scenes.c.o') else word for word in link]
    link.insert(next(i for i, word in enumerate(link) if word.endswith('.a')), str(obj))
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    out = area / f'{name}-out'
    run = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/ui_shell.script'), '--hub', str(ROOT / 'tests/ui_shell.hub.json'),
        '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--quiet', '--no-random-outages'], env={**os.environ, 'INK_UI_SCENE': scene, 'INK_UI_SCENE_TEXT': '3'},
        capture_output=True, text=True, timeout=120)
    (area / f'{name}.log').write_text(run.stdout + run.stderr)
    data = json.loads((out / 'metrics.json').read_text())
    metrics, warnings = data['metrics'], data.get('warnings', [])
    matched = [w.replace(str(ROOT), '.') for w in warnings if expected in w]
    assert run.returncode and metrics.get('test.finished') == 1 and matched, (name, run.returncode, warnings)
    results.append({'withdrawn': name, 'source': path, 'scene': scene, 'exit': run.returncode,
                    'failures': metrics.get('test.fail', 0), 'decisive': matched[0]})
(area / 'results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(results, ensure_ascii=False, indent=2))
