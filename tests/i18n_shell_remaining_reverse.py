"""Manual isolated controls for shell translations, layout and the corrected click driver."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', required=True, type=Path)
a = p.parse_args()
build = a.build.resolve()
area = build / 'i18n-2b-reverse'
area.mkdir(exist_ok=True)
db = {Path(e['file']).resolve(): e for e in json.loads((build / 'compile_commands.json').read_text())}
cases = [
    ('backup', 'system/ui/shell/backup_pages.c', 'TR("现在备份一次")', '"现在备份一次"', 'sim-i18n-shell-remaining', 'Backup-en has expected text: Back Up Now'),
    ('ai-width', 'system/ui/shell/ai_panel.c', '0, 0, chat_w, INK_CONTROL', '0, 0, 2 * INK_CONTROL, INK_CONTROL', 'sim-i18n-shell-remaining', 'AiAnswer-en layout:'),
    ('lock-date', 'system/ui/lock_mark.c', 'int date_y = LV_MAX(INK_STATUS_BAR + INK_BLOCK, INK_STATUS_BAR + INK_GAP + INK_CONTROL + INK_GAP_M);', 'int date_y = INK_STATUS_BAR + INK_BLOCK;', 'sim-i18n-shell', 'LockPage-en layout:'),
    ('power-height', 'system/ui/shell/power_menu.c', 'int text_h = LV_MAX(ink_ui_line_height(INK_FONT_LIST), ink_ui_text_height(TR("锁屏"), INK_FONT_LIST, INK_WEIGHT_MEDIUM, cell));', 'int text_h = ink_ui_line_height(INK_FONT_LIST);', 'sim-i18n-shell', 'power_menu-en layout:'),
    ('keyboard-icons', 'system/ui/shell/keyboard.c', 'ink_ui_text_width(i < 2 ? EDIT[i] : TR(EDIT[i]),', 'ink_ui_text_width(TR(EDIT[i]),', 'sim-i18n-shell', 'migrated paths have zero missing keys'),
    ('click', 'tests/i18n_shell.c', 'lv_obj_send_event(button, LV_EVENT_CLICKED, NULL);', 'lv_obj_send_event(button, LV_EVENT_SHORT_CLICKED, NULL);', 'sim-i18n-shell', 'dismissed card closes and settles'),
    ('battery-locator', 'tests/small_gaps/pages.c', 'test_lang ? "Battery Saver: lock after 1 min" : "省电：1 分钟锁屏"', '"省电：1 分钟锁屏"', 'sim-small-gaps-pages', 'battery controls match the current member'),
]
summary = []
for name, path, needle, replacement, target, expected in cases:
    source = ROOT / path
    original = source.read_text()
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    assert original.count(needle) == 1, name
    if target.startswith('sim-i18n-shell'):
        positive = build / 'test-out' / target.removeprefix('sim-') / 'metrics.json'
        assert expected not in '\n'.join(json.loads(positive.read_text()).get('warnings', [])), 'positive must pass the specific assertion'
    entry = db[source.resolve()]
    obj, binary, changed = area / (name + '.o'), area / name, area / (name + '.c')
    changed.write_text(original.replace(needle, replacement))
    command = shlex.split(entry['command'])
    old_object = command[command.index('-o') + 1]
    command[command.index('-o') + 1] = str(obj)
    command[command.index(str(source))] = str(changed)
    command += ['-iquote', str(source.parent)]
    compiled = subprocess.run(command, cwd=build, capture_output=True, text=True)
    assert not compiled.returncode, compiled.stderr
    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    link[link.index('-o') + 1] = str(binary)
    if path.startswith('tests/'):
        link[next(i for i, s in enumerate(link) if s.endswith(old_object))] = str(obj)
    else:
        link.insert(next(i for i, s in enumerate(link) if s.endswith('libink_system.a')), str(obj))
    linked = subprocess.run(link, cwd=build, capture_output=True, text=True)
    assert not linked.returncode, linked.stderr
    out = area / ('out-' + name)
    if name == 'battery-locator':
        driver = [str(ROOT / 'tests/small_gaps/run.py'), '--case', 'pages', '--language', 'en']
    else:
        driver = [str(ROOT / 'tests/i18n_shell.py'), '--out', str(out)]
    command = [os.sys.executable, *driver, '--build', str(build), '--binary', str(binary)]
    result = subprocess.run(command, cwd=ROOT, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, capture_output=True, text=True, timeout=240)
    log = result.stdout + result.stderr
    (area / (name + '.log')).write_text(log)
    assert result.returncode and expected in log, (name, log[-8000:])
    assert hashlib.sha256(source.read_bytes()).hexdigest() == before
    summary.append({'case': name, 'compile': compiled.returncode, 'link': linked.returncode, 'test': result.returncode, 'failure': expected})
    print(name, 'EXPECTED FAIL', flush=True)
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
