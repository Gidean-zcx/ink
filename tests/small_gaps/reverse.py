"""Remove each fix in temporary objects; no Git history or source-tree mutation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, required=True)
args = parser.parse_args()
build = args.build.resolve()
area = build / 'small-gaps-reverse'
area.mkdir(exist_ok=True)
db = {Path(e['file']).resolve(): e for e in json.loads((build / 'compile_commands.json').read_text())}
cases = [
    ('key-gate', 'voice', 'system/services/ai/ai_service.c',
     'if (speaking && !ink_setting_get("voice.interrupt")) return;', '', 'disabled key and speech leave the answer playing'),
    ('speech-gate', 'voice', 'system/services/audio/audio_service.c',
     ' && ink_setting_get("voice.interrupt")', '', 'disabled key and speech leave the answer playing'),
    ('voice-toggle', 'pages', 'system/ui/shell/settings_pages.c',
     '"说话能打断回答", NULL, "voice.interrupt",', '"说话能打断回答", NULL, NULL,', 'real toggle uses settings.set'),
    ('device-values', 'pages', 'system/ui/shell/settings_pages.c',
     'info.display_name ? info.display_name : ""', '"wrong synthetic model"', 'model is the HAL value'),
    ('license-paging', 'pages', 'system/ui/shell/settings_pages.c',
     'ink_ui_list_turn(L.list, !strcmp(c->cap->id, "shell.settings_next") ? 1 : -1);', '(void)0;', 'physical next key turns the legal text'),
    ('call-minutes', 'phone', 'system/apps/phone/phone.c',
     'if (minutes == P.minutes) return;', 'if (true) return;', 'elapsed minutes are visible to user and AI: 1'),
    ('battery-member', 'pages', 'system/ui/shell/power_pages.c',
     '           ink_state_watch("member.", changed, NULL, INK_EXEC_UI) ||\n', '', 'battery controls match the current member'),
]
before = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for _, _, p, *_ in cases}
summary = []
for name, case, path, needle, replacement, expected in cases:
    source = root / path
    original = source.read_text()
    assert original.count(needle) == 1, name
    changed = area / (name + '.c')
    changed.write_text(original.replace(needle, replacement))
    included = source.name in ('phone.c', 'settings_pages.c')
    unit = root / 'tests/small_gaps' / (case + '.c') if included else source
    entry = db[unit]
    command = shlex.split(entry['command']) if 'command' in entry else list(entry['arguments'])
    obj, binary = area / (name + '.o'), area / name
    command[command.index('-o') + 1] = str(obj)
    command += ['-iquote', str(source.parent)]
    if included:
        macro = 'INK_GAPS_PHONE_SOURCE' if source.name == 'phone.c' else 'INK_GAPS_SETTINGS_SOURCE'
        command += ['-D' + macro + '="' + str(changed) + '"']
    else:
        command[command.index(str(source))] = str(changed)
    compiled = subprocess.run(command, cwd=entry['directory'], capture_output=True, text=True)
    assert compiled.returncode == 0, compiled.stderr
    link = shlex.split((build / f'CMakeFiles/sim-small-gaps-{case}.dir/link.txt').read_text())
    link[link.index('-o') + 1] = str(binary)
    if included:
        link[next(i for i, part in enumerate(link) if part.endswith(f'tests/small_gaps/{case}.c.o'))] = str(obj)
    else:
        link.insert(next(i for i, part in enumerate(link) if part.endswith('libink_system.a')), str(obj))
    linked = subprocess.run(link, cwd=build, capture_output=True, text=True)
    assert linked.returncode == 0, linked.stderr
    result = subprocess.run([os.sys.executable, str(root / 'tests/small_gaps/run.py'), '--build', str(build),
                             '--case', case, '--language', 'zh', '--binary', str(binary)],
                            env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, capture_output=True, text=True, timeout=180)
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode != 0 and expected in result.stdout + result.stderr, (name, result.stdout, result.stderr)
    summary.append({'case': name, 'compile': compiled.returncode, 'link': linked.returncode,
                    'test': result.returncode, 'expected_failure': expected})
    print(name, 'EXPECTED FAIL', flush=True)
assert all(hashlib.sha256((root / path).read_bytes()).hexdigest() == value for path, value in before.items())
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
