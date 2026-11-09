"""Revert review fixes in replacement objects, leaving production sources intact."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'settings-b-reverse'
area.mkdir(parents=True, exist_ok=True)
source = ROOT / 'system/ui/shell/settings_family.c'
original = source.read_bytes()
sources = {name: ROOT / name for name in (
    'system/ui/shell/settings_family.c', 'system/ui/shell/navigation.c', 'system/ui/shell/settings_pages.c',
    'system/ui/shell/wallpaper.c', 'system/ui/lock_mark.c', 'system/policy/members.c', 'system/ui/ink/ui_shell.h')}
originals = {name: path.read_bytes() for name, path in sources.items()}
needle = '    if (ink_member_current()->role == INK_ROLE_CHILD || !ink_member_space_open()) return;'
assert original.decode().count(needle) == 1
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
link = shlex.split((build / 'CMakeFiles/sim-settings-b.dir/link.txt').read_text())
commands = []

def run(command, **kwargs):
    commands.append([str(x) for x in command])
    return subprocess.run(command, cwd=build, capture_output=True, text=True, timeout=240, **kwargs)

text = original.decode()
epoch = text.replace('uint32_t generation;', 'uint32_t epoch, generation;').replace(
    'S.body && ink_ui_page_current()', 'S.body && S.epoch == ink_member_epoch()').replace(
    'S.page = (int)(intptr_t)arg; S.generation++;', 'S.page = (int)(intptr_t)arg; S.epoch = ink_member_epoch(); S.generation++;')
assert epoch != text and 'S.epoch = ink_member_epoch()' in epoch
family = 'system/ui/shell/settings_family.c'
navigation = 'system/ui/shell/navigation.c'
voice = 'system/ui/shell/settings_pages.c'
wallpaper = 'system/ui/shell/wallpaper.c'
lock = 'system/ui/lock_mark.c'
members = 'system/policy/members.c'
decoded = {name: data.decode() for name, data in originals.items()}
old_lock = subprocess.run(['git', 'show', 'f9ca3924:' + lock], cwd=ROOT, check=True, capture_output=True, text=True).stdout
summary = []
for name, filename, changed, expected, failures in [
    ('page-guard', family, text.replace(needle, ''), 'child direct page guard denies all five pages', 5),
    ('pin-epoch', family, epoch, 'page has an AI snapshot', 2),
    ('SB-M1', family, text.replace('"shell.pin_set"', '"family.pin"'), 'password row opens shared PinSet', 1),
    ('SB-M2-settings', navigation, decoded[navigation].replace('ink_shell_bind(who, "shell.family", NULL);', 'ink_shell_bind(who, "shell.members", NULL);').replace('lock_after, 0, "shell.wallpaper"', '"这一页", 0, "shell.display"'), 'settings family card opens SetMembers', 1),
    ('SB-M2-voice', voice, decoded[voice].replace('"shell.records", "list", "AI"', 'NULL, "list", "AI"').replace('"shell.ai_memory", "ai"', 'NULL, "ai"'), 'voice settings open AiMemory', 1),
    ('SB-M3-lock', lock, old_lock, 'three styles keep clock and wallpaper separate', 1),
    ('SB-M3-cover', wallpaper, decoded[wallpaper].replace('LV_IMAGE_ALIGN_COVER', 'LV_IMAGE_ALIGN_STRETCH'), 'wallpaper covers without stretching', 1),
    ('SB-M3-home', navigation, decoded[navigation].replace('    ink_shell_dock(screen);\n    draw_home_content();', '    ink_shell_dock(screen);\n    draw_home_content();\n    if (ink_wallpaper_ready()) { lv_obj_set_style_bg_opa(N.content, LV_OPA_TRANSP, 0); ink_wallpaper_draw(screen); }'), 'selecting a wallpaper must leave home unchanged', 0),
    ('SB-M4', family, text.replace('\\"limit\\":6', '\\"limit\\":20').replace('S.count < 6', 'S.count < 20'), 'records parse within the JSON token limit', 1),
    ('SB-L1-policy', family, text.replace('kid ? apps[0] ? apps :', 'kid ? apps[0] ? "阅读、AI；电话和短信不限制" :'), 'child page lists only the actual allow policy', 1),
    ('SB-L1-rename', members, decoded[members].replace('.id = "member.rename"', '.id = "sb.missing_rename"'), 'member startup registers rename', 1),
]:
    assert changed != decoded[filename], name
    obj, binary = area / (name + '.o'), area / name
    compiled = run([link[0], *options, '-iquote', str(sources[filename].parent), '-x', 'c', '-', '-c', '-o', str(obj)], input=changed)
    assert compiled.returncode == 0, compiled.stderr
    command = link.copy()
    command[command.index('-o') + 1] = str(binary)
    command.insert(next(i for i, part in enumerate(command) if part.endswith('libink_system.a')), str(obj))
    linked = run(command)
    assert linked.returncode == 0, linked.stderr
    result = run([os.sys.executable, str(ROOT / 'tests/settings_b.py'), '--build', str(build), '--binary', str(binary)],
                 env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode != 0 and expected in result.stdout + result.stderr
    metrics = json.loads((build / 'test-out' / ('settings-b-' + name) / 'write/metrics.json').read_text())['metrics']
    assert metrics.get('test.finished') == 1 and metrics.get('test.fail', 0) >= failures
    summary.append({'case': name, 'exit': result.returncode, 'failures': metrics.get('test.fail', 0), 'finished': metrics['test.finished']})
    print(f'PASS: {name} rejected (C checks={metrics.get("test.fail", 0)}); production source unchanged', flush=True)
# Reproduce the header defect against the actual double-include compile check.
header = area / 'header/ink/ui_shell.h'
header.parent.mkdir(parents=True, exist_ok=True)
header_text = decoded['system/ui/ink/ui_shell.h']
header.write_text(header_text.rsplit('#endif', 1)[0].replace('int ink_settings_family_init(void);', '#endif\nint ink_settings_family_init(void);'))
test_flags = (build / 'CMakeFiles/sim-settings-b.dir/flags.make').read_text()
test_options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    test_options += shlex.split(re.search(r'^' + key + r' = (.*)$', test_flags, re.M)[1])
compiled = run([link[0], '-I' + str(header.parent.parent), *test_options, '-c', str(ROOT / 'tests/settings_b.c'), '-o', str(area / 'header.o')])
assert compiled.returncode != 0 and 'ink_settings_family_header_guard' in compiled.stderr
(area / 'SB-L1-header.log').write_text(compiled.stderr)
summary.append({'case': 'SB-L1-header', 'compile_exit': compiled.returncode})
for filename, path in sources.items():
    assert originals[filename] == path.read_bytes(), filename
(area / 'summary.json').write_text(json.dumps({'cases': summary, 'source_sha256': {name: hashlib.sha256(data).hexdigest() for name, data in originals.items()}}, indent=2) + '\n')
(area / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
