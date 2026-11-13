"""Keyboard acceptance and optional isolated reverse controls; no Git history required."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--reverse', action='store_true')
parser.add_argument('--pages', action='store_true')
parser.add_argument('--baseline-pictures', action='store_true')
parser.add_argument('--key-height108', action='store_true')
parser.add_argument('--refresh-reverse', action='store_true')
parser.add_argument('--switch-reverse', action='store_true')
args = parser.parse_args()
build = args.build.resolve()


def exercise(binary, tag, expected=None, verify=False, state=None):
    out = build / 'test-out' / f'ime-keyboard1-{tag}'
    shutil.rmtree(out, ignore_errors=True)
    state = state or build / 'test-out' / f'ime-keyboard1-state-{tag}'
    if not verify:
        shutil.rmtree(state, ignore_errors=True)
    (state / 'sd').mkdir(parents=True, exist_ok=True)
    command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '--script', str(ROOT / 'tests/ime_keyboard1.script'),
               '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--assets', str(build / 'assets'),
               '--content', str(build / 'test-content'), '--flash-image', str(state / 'user.img'),
               '--sd-dir', str(state / 'sd'), '--out', str(out), '--quiet', '--no-random-outages']
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=150,
                            env={**os.environ, **({'INK_IME_LAYOUT_VERIFY': '1'} if verify else {}), **({'INK_KEYBOARD_PAGES': '1'} if args.pages else {})})
    out.mkdir(parents=True, exist_ok=True)
    (out / 'test.log').write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    metrics = data.get('metrics', {})
    frames = list(out.glob('snap-*.png'))
    if expected:
        ok = result.returncode != 0 and metrics.get('test.fail', 0) > 0 and any(expected in w for w in data.get('warnings', []))
    else:
        ok = result.returncode == 0 and metrics.get('test.finished') == 1 and not metrics.get('test.fail') and not data.get('warnings') and len(frames) == (0 if verify or os.environ.get("INK_IME_SWITCH_ONLY") else 28 if args.pages else 22)
    summary = {'case': tag, 'exit': result.returncode, 'passes': metrics.get('test.pass', 0),
               'failures': metrics.get('test.fail', 0), 'frames': len(frames), 'expected_result': ok}
    summary['query_us'] = [list(map(float, line.split()[1:])) for line in result.stdout.splitlines() if line.startswith('NINE_BENCH ')]
    (out / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(summary, ensure_ascii=False), flush=True)
    if not ok:
        print(result.stderr[-6000:], data.get('warnings', []), flush=True)
    if ok and not expected and not os.environ.get('INK_KEEP_EVIDENCE'):
        shutil.rmtree(out)
    return ok


if args.baseline_pictures or args.key_height108 or args.refresh_reverse:
    area = build / 'test-out' / ('keyboard-baseline' if args.baseline_pictures else 'keyboard-108' if args.key_height108 else 'keyboard-refresh-reverse')
    area.mkdir(parents=True, exist_ok=True)
    source = ROOT / 'tests/ime_keyboard1.c'
    contents = source.read_text()
    keyboard_source = ROOT / 'system/ui/shell/keyboard.c'
    if args.baseline_pictures:
        def baseline(path):
            return subprocess.run(['git', 'show', 'd7bc63aeee29ce9280cb15bc9c866d5e400ef4ca:' + path], cwd=ROOT, capture_output=True, text=True, check=True).stdout
        contents = baseline('tests/ime_keyboard1.c')
        keyboard_source = area / 'keyboard.c'
        keyboard_source.write_text(baseline('system/ui/shell/keyboard.c'))
        contents = contents.replace('for (int kind = 0; kind < 6; kind++)', 'for (int kind = 0; kind < 7; kind++)').replace('"Nine", "Password"}', '"Nine", "Password", "Expanded"}')
        contents = contents.replace('    if (screenshot_kind == 0) chinese_scene();', '    if (screenshot_kind == 6) { chinese_scene(); ink_call_t call = {0}; candidates(&call); }\n    if (screenshot_kind == 0) chinese_scene();')
        contents = contents.replace('    lv_obj_update_layout(K.root);\n    lv_area_t root;', '    if (K.expanded) return;\n    lv_obj_update_layout(K.root);\n    lv_area_t root;')
    elif args.refresh_reverse:
        keyboard_source = area / 'keyboard.c'
        original = (ROOT / 'system/ui/shell/keyboard.c').read_text()
        before = '    ink_ui_hint_obj(K.composing, INK_REFRESH_FAST);'
        assert original.count(before) == 1
        keyboard_source.write_text(original.replace(before, before + '\n    lv_obj_invalidate(K.keys);'))
        args.pages = True
    replacement = area / 'ime_keyboard1.c'
    contents = contents.replace('"../system/ui/shell/keyboard.c"', '"' + str(keyboard_source) + '"')
    replacement.write_text(contents)
    entry = next(e for e in json.loads((build / 'compile_commands.json').read_text()) if Path(e['file']) == source)
    command = shlex.split(entry['command'])
    obj = area / 'ime_keyboard1.o'
    command[command.index('-o') + 1] = str(obj)
    command[command.index(str(source))] = str(replacement)
    command += ['-I' + str(source.parent), '-I' + str(ROOT / 'system/ui/shell')]
    if args.key_height108:
        command += ['-DINK_KBD_KEY_HEIGHT=108']
    subprocess.run(command, cwd=entry['directory'], check=True)
    link = shlex.split((build / 'CMakeFiles/sim-ime-keyboard1.dir/link.txt').read_text())
    binary = area / 'sim-ime-keyboard1'
    link[link.index('-o') + 1] = str(binary)
    link[link.index('CMakeFiles/sim-ime-keyboard1.dir/tests/ime_keyboard1.c.o')] = str(obj)
    subprocess.run(link, cwd=build, check=True)
    expected = 'letters only refresh composition and candidates' if args.refresh_reverse else None
    raise SystemExit(0 if exercise(binary, area.name, expected=expected) else 1)

if args.switch_reverse:
    # Manual baseline control: CI does not need repository history.
    source = ROOT / 'tests/ime_keyboard1.c'
    area = build / 'test-out/ime-switch-baseline'
    area.mkdir(parents=True, exist_ok=True)
    old = subprocess.run(['git', 'show', '6171c2bf8d59238c95f2d548663f8b2ab3c6f43a:system/ui/shell/keyboard.c'],
                         cwd=ROOT, capture_output=True, text=True, check=True).stdout
    keyboard = area / 'keyboard.c'
    keyboard.write_text(old)
    replacement = area / 'ime_keyboard1.c'
    replacement.write_text(source.read_text().replace('"../system/ui/shell/keyboard.c"', '"' + str(keyboard) + '"'))
    entry = next(e for e in json.loads((build / 'compile_commands.json').read_text()) if Path(e['file']) == source)
    command = shlex.split(entry['command'])
    obj = area / 'ime_keyboard1.o'
    command[command.index('-o') + 1] = str(obj)
    command[command.index(str(source))] = str(replacement)
    command += ['-I' + str(source.parent)]
    subprocess.run(command, cwd=entry['directory'], check=True)
    link = shlex.split((build / 'CMakeFiles/sim-ime-keyboard1.dir/link.txt').read_text())
    binary = area / 'sim-ime-keyboard1'
    link[link.index('-o') + 1] = str(binary)
    link[link.index('CMakeFiles/sim-ime-keyboard1.dir/tests/ime_keyboard1.c.o')] = str(obj)
    subprocess.run(link, cwd=build, check=True)
    os.environ['INK_IME_SWITCH_ONLY'] = '1'
    raise SystemExit(0 if exercise(binary, 'switch-baseline', expected='switch does not request a flashing keyboard rebuild') else 1)

if args.pages:
    raise SystemExit(0 if exercise(build / 'sim-ime-keyboard1', 'pages') else 1)

if not args.reverse:
    state = build / 'test-out/ime-keyboard1-state-current'
    passed = exercise(build / 'sim-ime-keyboard1', 'current', state=state)
    passed = exercise(build / 'sim-ime-keyboard1', 'verify', verify=True, state=state) and passed
    if passed and not os.environ.get('INK_KEEP_EVIDENCE'):
        shutil.rmtree(state)
    raise SystemExit(0 if passed else 1)

source = ROOT / 'system/services/text/text_service.c'
db = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in db if Path(e['file']) == source)
mutations = [
    ('no-nine-matching', 'ink_text_nine() ? py_lookup_nine(TS.composing, raw, MAX_CANDS) : py_lookup(TS.composing, raw, MAX_CANDS)',
     'py_lookup(TS.composing, raw, MAX_CANDS)', '94664 offers the phrase 中国'),
    ('english-pinyin', "} else if (!TS.english && (ink_text_nine() ? c >= '2' && c <= '9' : c >= 'a' && c <= 'z')) {", "} else if (c >= 'a' && c <= 'z') {",
     'English letters commit directly and space inserts a space'),
    ('lost-composing', 'if (c == INK_TEXT_KEY_LANGUAGE) {\n            flush_composing();',
     'if (c == INK_TEXT_KEY_LANGUAGE) {\n            TS.composing[0] = 0;', 'switch commits pending pinyin verbatim'),
    ('same-punctuation', 'if (TS.type != INK_TEXT_PLAIN || TS.english) return NULL;',
     'if (TS.type != INK_TEXT_PLAIN) return NULL;', 'English punctuation is half width'),
]
passed = True
for name, before, after, expected in mutations:
    original = source.read_text()
    assert original.count(before) == 1, name
    area = build / 'test-out' / f'ime-keyboard1-mutation-{name}'
    area.mkdir(parents=True, exist_ok=True)
    replacement = area / 'text_service.c'
    replacement.write_text(original.replace(before, after))
    obj = area / 'text_service.o'
    command = shlex.split(entry['command'])
    command[command.index('-o') + 1] = str(obj)
    command[command.index(str(source))] = str(replacement)
    command += ['-I' + str(source.parent)]
    subprocess.run(command, cwd=entry['directory'], check=True)
    link = shlex.split((build / 'CMakeFiles/sim-ime-keyboard1.dir/link.txt').read_text())
    binary = area / 'sim-ime-keyboard1'
    link[link.index('-o') + 1] = str(binary)
    link.insert(link.index('libink_system.a'), str(obj))
    subprocess.run(link, cwd=build, check=True)
    passed = exercise(binary, name, expected) and passed
raise SystemExit(0 if passed else 1)
