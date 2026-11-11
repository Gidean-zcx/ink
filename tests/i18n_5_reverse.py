"""Manual controls: remove display translations in isolated objects, leaving product sources intact."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', required=True, type=Path)
build = parser.parse_args().build.resolve()
area = build / 'i18n-5-reverse'
area.mkdir(exist_ok=True)
db = {Path(e['file']).resolve(): e for e in json.loads((build / 'compile_commands.json').read_text())}
cases = [
    ('ai-note', 'system/services/ai/ai_service.c', 'const char *cut = TR(INK_AI_ANSWER_CUT);',
     'const char *cut = INK_AI_ANSWER_CUT;', 'sim-ai-answer-long', 'the last page ends with the note'),
    ('icon-menu', 'system/ui/shell/navigation.c', 'const char *title = TR(app->title);',
     'const char *title = app->title;', 'sim-i18n-shell-remaining', 'icon menu translates its registered app title'),
    ('search-title', 'system/ui/search.c', 'title = TR(title);',
     '/* Keep the untranslated source title. */', 'sim-i18n-shell-remaining', 'search translated app name'),
]
commands, summary = [], []
for name, path, needle, replacement, target, expected in cases:
    source = ROOT / path
    original = source.read_bytes()
    before = hashlib.sha256(original).hexdigest()
    assert original.decode().count(needle) == 1
    changed, obj, binary = area / (name + '.c'), area / (name + '.o'), area / name
    changed.write_text(original.decode().replace(needle, replacement))
    compile_cmd = shlex.split(db[source]['command'])
    compile_cmd[compile_cmd.index('-o') + 1] = str(obj)
    compile_cmd[compile_cmd.index(str(source))] = str(changed)
    compile_cmd += ['-iquote', str(source.parent)]
    link_cmd = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    link_cmd[link_cmd.index('-o') + 1] = str(binary)
    link_cmd.insert(next(i for i, arg in enumerate(link_cmd) if arg.endswith('libink_system.a')), str(obj))
    for cmd in (compile_cmd, link_cmd):
        result = subprocess.run(cmd, cwd=build, text=True, capture_output=True, timeout=120)
        commands.append({'command': cmd, 'exit': result.returncode})
        assert result.returncode == 0, result.stderr
    out = area / ('out-' + name)
    shutil.rmtree(out, ignore_errors=True)
    long_answer = name == 'ai-note'
    cmd = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
           '--script', str(ROOT / ('tests/ai_answer_long/ai_answer_long.script' if long_answer else 'tests/i18n_lang.script')),
           '--budget', str(ROOT / ('tests/ai_answer_long/budget.json' if long_answer else 'tests/ui_scene.budget.json')),
           '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
           '--out', str(out), '--quiet', '--no-random-outages']
    if long_answer:
        cmd += ['--hub', str(ROOT / 'tests/ai_answer_long/hub.json')]
    env = {**os.environ, ('INK_I18N_ANSWER_EN' if long_answer else 'INK_I18N_EXTRAS'): '1'}
    result = subprocess.run(cmd, cwd=ROOT, env=env, text=True, capture_output=True, timeout=180)
    commands.append({'command': cmd, 'exit': result.returncode})
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text())
    assert result.returncode and any(expected in warning for warning in data['warnings']), data['warnings']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == before
    summary.append({'case': name, 'exit': result.returncode, 'failure': expected, 'source_unchanged': True})
    print(f'PASS reverse {name}: "{expected}" (exit={result.returncode}); product source unchanged', flush=True)
(area / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
