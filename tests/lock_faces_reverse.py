"""Manual reverse controls; compile mutations in the build tree without editing product files."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--case', choices=['minute', 'day', 'member', 'reading', 'solar', 'enum', 'events'])
a = p.parse_args()
build = a.build.resolve()
area = build / 'lock-faces-reverse'
area.mkdir(exist_ok=True)
db = json.loads((build / 'compile_commands.json').read_text())
faces = ROOT / 'system/ui/lock_faces/faces.c'
lock = ROOT / 'system/ui/lock_mark.c'
cases = [
    ('enum', ROOT / 'system/ui/shell/navigation.c', 'TR_NOOP("壁纸"), TR_NOOP("简约时钟"), TR_NOOP("待机时钟"), TR_NOOP("月历"), NULL};',
     'TR_NOOP("壁纸"), NULL};', 'live settings accept the new enum through AI'),
    ('solar', ROOT / 'system/services/time/lunar.c', 'return TR(names[i]);', 'return NULL;',
     'same minute on a new date updates through time.changed'),
    ('events', lock, 'ink_cap_subscribe("time.changed", on_time, NULL, INK_EXEC_UI)',
     'ink_cap_subscribe("time.changed.unused", on_time, NULL, INK_EXEC_UI)',
     'same minute on a new date updates through time.changed'),
    ('minute', faces, 'if (!F.clock || !strcmp(lv_label_get_text(F.clock), time)) return false;',
     'if (!F.clock) return false;', 'same minute does not refresh'),
    ('day', faces, 'if (day != F.day) {', 'if (day == -1) {', 'cross-day rebuild'),
    ('member', lock, '    else if (g_mark) { hide(); show(); }\n    ink_lock_face_resume();', '    ink_lock_face_resume();',
     'child pixels contain no adult memo label'),
    ('reading', faces, 'ink_call_opts_t opts = {.src = INK_SRC_TOUCH}; ink_cap_call("reader.continue", NULL, &opts);',
     '(void)0;', 'unlock resumes the reader'),
]
for name, source, before, after, expected in cases:
    if a.case and a.case != name:
        continue
    original = source.read_text()
    assert original.count(before) == 1, name
    replacement = area / f'{name}.c'
    replacement.write_text(original.replace(before, after))
    unit = ROOT / 'tests/lock_faces.c' if source == lock else source
    entry = next(item for item in db if Path(item['file']) == unit)
    cmd = shlex.split(entry['command'])
    obj = area / f'{name}.o'
    cmd[cmd.index('-o') + 1] = str(obj)
    cmd += ['-I' + str(ROOT / 'system/ui'), '-I' + str(faces.parent), '-I' + str(source.parent)]
    if source == lock:
        cmd += ['-DINK_TEST_LOCK_SOURCE=' + json.dumps(str(replacement))]
    else:
        cmd[cmd.index(str(unit))] = str(replacement)
    subprocess.run(cmd, cwd=entry['directory'], check=True, capture_output=True, text=True)
    link = shlex.split((build / 'CMakeFiles/sim-lock-faces.dir/link.txt').read_text())
    binary = area / f'sim-lock-faces-{name}'
    link[link.index('-o') + 1] = str(binary)
    if source == lock:
        index = next(i for i, part in enumerate(link) if part.endswith('/tests/lock_faces.c.o'))
        link[index] = str(obj)
    else:
        link.insert(next(i for i, part in enumerate(link) if part.endswith('.o')), str(obj))
    subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
    r = subprocess.run(['python3', str(ROOT / 'tests/lock_faces.py'), '--build', str(build), '--binary', str(binary)],
                       capture_output=True, text=True, env={**os.environ, 'INK_LOCK_FACES_CASE': name})
    (area / f'{name}.log').write_text(r.stdout + r.stderr)
    assert r.returncode != 0 and expected in r.stdout, f'{name}: {r.stdout[-2000:]} {r.stderr[-1000:]}'
    print(f'{name}: reverse failed at the expected assertion', flush=True)
