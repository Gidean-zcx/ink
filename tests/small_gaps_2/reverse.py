"""Manual reverse controls. Requires the baseline commit; never registered in CTest."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline', default='e65bfc261b68e9fb063fc60da507122119ae4fc6')
a = p.parse_args()
build = a.build.resolve()
area = ROOT / 'work/small-gaps-2/reverse'
area.mkdir(parents=True, exist_ok=True)
db = {Path(e['file']).resolve(): e for e in json.loads((build / 'compile_commands.json').read_text())}
baseline = subprocess.run(['git', 'cat-file', '-e', a.baseline + '^{commit}'], cwd=ROOT, capture_output=True)
if baseline.returncode:
    raise SystemExit('Manual reverse control requires the baseline commit; no reverse checks ran.')


def original(path):
    return subprocess.check_output(['git', 'show', a.baseline + ':' + path], cwd=ROOT, text=True)


def link_variant(name, target, sources):
    objects = []
    for path, source in sources:
        entry = db[(ROOT / path).resolve()]
        command = shlex.split(entry['command'])
        options, skip = [], False
        for option in command[1:]:
            if skip:
                skip = False
            elif option in ('-o', '-MF', '-MT', '-MQ'):
                skip = True
            elif option not in ('-c', '-MD', '-MMD', entry['file']):
                options.append(option)
        obj = area / (name + '-' + Path(path).stem + '.o')
        result = subprocess.run([command[0], *options, '-iquote', str((ROOT / path).parent), '-x', 'c', '-', '-c', '-o', str(obj)],
                                input=source, cwd=entry['directory'], text=True, capture_output=True)
        (area / (obj.stem + '.log')).write_text(result.stdout + result.stderr)
        if result.returncode:
            raise SystemExit(result.stderr)
        objects.append(str(obj))
    command = shlex.split((build / 'CMakeFiles' / (target + '.dir') / 'link.txt').read_text())
    binary = area / name
    command[command.index('-o') + 1] = str(binary)
    at = next(i for i, part in enumerate(command) if part.endswith('libink_apps.a'))
    command[at:at] = objects
    result = subprocess.run(command, cwd=build, capture_output=True, text=True)
    (area / (name + '-link.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.stderr)
    return binary


summary = []
cases = {
    'icons': (['system/apps/clock/clock.c', 'system/apps/camera/camera.c', 'system/apps/gallery/gallery.c'], 'Apps tile uses its own packed icon'),
    'phone': (['system/apps/phone/phone.c'], 'notice does not overlap digit'),
    'picker': (['system/apps/calendar/calendar.c'], 'both pickers share button geometry'),
    'airplane': (['system/ui/shell/navigation.c'], 'control panel closes after disabling airplane mode'),
}
for name, (paths, expected) in cases.items():
    binary = link_variant(name, 'sim-small-gaps-2', [(path, original(path)) for path in paths])
    out = area / (name + '-out')
    result = subprocess.run([sys.executable, '-B', str(ROOT / 'tests/small_gaps_2/run.py'), '--build', str(build),
                             '--case', name, '--binary', str(binary), '--out', str(out)], cwd=ROOT, capture_output=True, text=True,
                            env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, timeout=200)
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    counts = json.loads((out / 'metrics.json').read_text())['metrics']
    assert result.returncode and counts.get('test.finished') == 1 and expected in result.stdout + result.stderr, name
    summary.append({'case': name, 'exit': result.returncode, 'failures': counts.get('test.fail'), 'expected': expected})
    print(name, 'expected failure:', counts.get('test.fail'), flush=True)

path = 'system/ui/theme.c'
source = (ROOT / path).read_text()
icon_line = '            lv_obj_center(ink_ui_icon(button, plus ? "plus" : "minus", INK_ICON_MEDIUM, INK_ICON_LINE, 0, 0, false));\n'
assert source.count(icon_line) == 1
source = source.replace(icon_line, '').replace('ink_ui_pill(parent, "", plus ?', 'ink_ui_pill(parent, plus ? "+" : "−", plus ?')
binary = link_variant('picker-icons', 'sim-small-gaps-2', [(path, source)])
out = area / 'picker-icons-out'
result = subprocess.run([sys.executable, '-B', str(ROOT / 'tests/small_gaps_2/run.py'), '--build', str(build), '--case', 'picker',
                         '--binary', str(binary), '--out', str(out)], cwd=ROOT, capture_output=True, text=True,
                        env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, timeout=200)
(area / 'picker-icons.log').write_text(result.stdout + result.stderr)
counts = json.loads((out / 'metrics.json').read_text())['metrics']
assert result.returncode and counts.get('test.finished') == 1 and 'picker step uses the packed plus/minus icon' in result.stdout + result.stderr
summary.append({'case': 'picker-icons', 'exit': result.returncode, 'failures': counts.get('test.fail')})
print('picker-icons expected failures:', counts.get('test.fail'), flush=True)

binary = link_variant('airplane-ai', 'sim-airplane-mode', [('system/ui/shell/navigation.c', original('system/ui/shell/navigation.c'))])
result = subprocess.run([sys.executable, '-B', str(ROOT / 'tests/airplane_mode.py'), '--build', str(build), '--binary', str(binary)],
                        cwd=ROOT, capture_output=True, text=True, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, timeout=250)
(area / 'airplane-ai.log').write_text(result.stdout + result.stderr)
assert result.returncode and 'the airplane error panel closes after the successful switch' in result.stdout + result.stderr
summary.append({'case': 'airplane-ai', 'exit': result.returncode, 'expected': 'AI airplane error panel stays open'})
print('airplane-ai expected failure', flush=True)

# The one reader (reader-one-1): its card follows storage.card through the watch in apps/mogo/mogo.c.
path = 'apps/mogo/mogo.c'
fixed = (ROOT / path).read_text()
watch = 'ink_state_watch("storage.card", ink_mogo_kick, NULL, INK_EXEC_UI) || '
assert fixed.count(watch) == 1
for name, source in (('reader-card', fixed.replace(watch, '')),):
    binary = link_variant(name, 'sim-reader-continue', [(path, source)])
    shutil.rmtree(area / (name + '-out'), ignore_errors=True)
    failures, warnings = 0, []
    for phase in ('read', 'restart'):
        out = area / (name + '-out') / phase
        result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
            '--script', str(ROOT / 'tests/kid_3.script'), '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
            '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--out', str(out), '--quiet', '--no-random-outages',
            '--flash-image', str(out.parent / 'user.img'), '--sd-dir', str(out.parent / 'sd')], cwd=ROOT, capture_output=True,
            text=True, timeout=200, env={**os.environ, 'INK_READER_CONTINUE_PHASE': phase})
        (out / 'run.log').write_text(result.stdout + result.stderr)
        data = json.loads((out / 'metrics.json').read_text())
        assert data['metrics'].get('test.finished') == 1
        failures += data['metrics'].get('test.fail', 0)
        warnings += data.get('warnings', [])
    expected = 'the home updates after card removal without navigation'
    assert failures and expected in str(warnings), (name, warnings)
    summary.append({'case': name, 'failures': failures, 'expected': expected})
    print(name, 'expected failures:', failures, flush=True)
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
