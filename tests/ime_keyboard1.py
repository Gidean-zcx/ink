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
args = parser.parse_args()
build = args.build.resolve()


def exercise(binary, tag, expected=None):
    out = build / 'test-out' / f'ime-keyboard1-{tag}'
    shutil.rmtree(out, ignore_errors=True)
    command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '--script', str(ROOT / 'tests/semantics_shell/scenes.script'),
               '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--assets', str(build / 'assets'),
               '--content', str(build / 'test-content'), '--out', str(out), '--quiet', '--no-random-outages']
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=150)
    out.mkdir(parents=True, exist_ok=True)
    (out / 'test.log').write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    metrics = data.get('metrics', {})
    frames = list(out.glob('snap-*.png'))
    if expected:
        ok = result.returncode != 0 and metrics.get('test.fail', 0) > 0 and any(expected in w for w in data.get('warnings', []))
    else:
        ok = result.returncode == 0 and metrics.get('test.finished') == 1 and not metrics.get('test.fail') and not data.get('warnings') and len(frames) == 6
    summary = {'case': tag, 'exit': result.returncode, 'passes': metrics.get('test.pass', 0),
               'failures': metrics.get('test.fail', 0), 'frames': len(frames), 'expected_result': ok}
    (out / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(summary, ensure_ascii=False), flush=True)
    if not ok:
        print(result.stderr[-6000:], data.get('warnings', []), flush=True)
    if ok and not expected and not os.environ.get('INK_KEEP_EVIDENCE'):
        shutil.rmtree(out)
    return ok


if not args.reverse:
    raise SystemExit(0 if exercise(build / 'sim-ime-keyboard1', 'current') else 1)

source = ROOT / 'system/services/text/text_service.c'
db = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in db if Path(e['file']) == source)
mutations = [
    ('english-pinyin', "} else if (!TS.english && c >= 'a' && c <= 'z') {", "} else if (c >= 'a' && c <= 'z') {",
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
