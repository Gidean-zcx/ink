"""settings-1 reverse checks (run by hand): take one piece of the feature out in a replacement object, link it before
the system library, and the settings-1 walk must fail. Product sources are never written."""
import argparse
import hashlib
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
area = build / 'settings-1-reverse'
area.mkdir(parents=True, exist_ok=True)
REGISTRY, BODY = 'system/ui/shell/settings_registry.c', 'system/ui/shell/settings_body.c'
cases = [
    ('search-skips-one', REGISTRY,
     'if (s == INK_SR_UNSEARCHABLE || s == INK_SR_NOT_NOW || s == INK_SR_UNSUPPORTED) continue;',
     'if (s == INK_SR_UNSEARCHABLE || s == INK_SR_NOT_NOW || s == INK_SR_UNSUPPORTED || !strcmp(it->id, "display.warmth")) continue;',
     '暖光 is on its page but not found by search: one declaration'),
    ('page-only-row', BODY,
     'if ((!H.query[0] && !(it->where & INK_SR_HOME)) || !home_line(it, text, sizeof text, &gray)) continue;',
     'if ((!H.query[0] && !(it->where & INK_SR_HOME) && strcmp(it->id, "net.airplane")) || !home_line(it, text, sizeof text, &gray)) continue;',
     'a row drawn on the home that the registry does not declare there'),
    ('description-skips-one', BODY,
     '        ink_jw_str(&w, ink_sr_at(i)->id);',
     '        ink_jw_str(&w, strcmp(ink_sr_at(i)->id, "display.warmth") ? ink_sr_at(i)->id : "display");',
     "settings.open's description no longer names 暖光"),
    ('whole-redraw', BODY, '        if (home_update()) return;', '        if (!home_update()) {}',
     'a changed value redraws the whole page, not its row'),
    ('child-sees-values', REGISTRY, '    if (it->adult && child()) return why(INK_SR_PARENT, out, cap, TR("家长管理"));', '',
     "a child's home and search show the parents' rows and values"),
    ('not-located', BODY, '        ink_sr_target = it; /* its page frames the row while it is built */', '',
     'settings.open lands on the page but not on its row'),
    ('cell-not-live', BODY, 'ink_state_watch("cell.", cell_changed, NULL, INK_EXEC_UI)',
     'ink_state_watch("cell.nothing.", cell_changed, NULL, INK_EXEC_UI)',
     "the home's 移动网络 row keeps the old words when the modem registers"),
]
before = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for _, path, *_ in cases}
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
failed_as_expected = []
for name, path, needle, replacement, what in cases:
    source = (ROOT / path).read_text()
    assert source.count(needle) == 1, name
    link = shlex.split((build / 'CMakeFiles/sim-settings-1.dir/link.txt').read_text())
    obj, binary = area / (name + '.o'), area / name
    compiled = subprocess.run([link[0], *options, '-iquote', str((ROOT / path).parent), '-x', 'c', '-', '-c', '-o', str(obj)],
                              input=source.replace(needle, replacement), cwd=build, capture_output=True, text=True, timeout=180)
    assert compiled.returncode == 0, compiled.stderr
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, part in enumerate(link) if part.endswith('libink_system.a')), str(obj))
    linked = subprocess.run(link, cwd=build, capture_output=True, text=True, timeout=180)
    assert linked.returncode == 0, linked.stderr
    result = subprocess.run([os.sys.executable, '-B', str(ROOT / 'tests/settings_1.py'), '--build', str(build), '--binary',
                             str(binary), '--sizes', '1'], cwd=ROOT, capture_output=True, text=True, timeout=900)
    caught = result.returncode != 0
    failed_as_expected.append(caught)
    reasons = [line.split(') ', 1)[-1] for line in result.stdout.splitlines() if 'CHECK FAILED' in line][:2]
    refresh = [line for line in result.stdout.splitlines() if 'redraws only its row: False' in line]
    print(f'{name}: {"FAILS as expected" if caught else "STILL PASSES"} ({what}); {reasons or refresh}', flush=True)
after = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in before}
assert before == after, 'a product source changed'
print(f'{sum(failed_as_expected)}/{len(cases)} reverse checks fail as they must; sources unchanged', flush=True)
raise SystemExit(0 if all(failed_as_expected) else 1)
