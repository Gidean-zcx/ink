"""Manual reversed implementations, compiled separately; no history dependency."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--case', action='append', help='only these cases (default: all)')
a = p.parse_args()
build = a.build.resolve()
area = build / 'clock-extras-reverse'
area.mkdir(exist_ok=True)
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
cxx_options = []
for key in ('CXX_DEFINES', 'CXX_INCLUDES', 'CXX_FLAGS'):
    cxx_options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
cases = (
    ('elapsed', 'uint32_t ck_watch_ms(int w) { return W(w).GetElapsedTime(); }', 'uint32_t ck_watch_ms(int w) { return W(w).GetElapsedTime() * 0; }', 'running elapsed agrees with virtual clock'),
    ('lap', 'return lap ? lap->timeSinceStart : 0;', 'return lap ? 0 : 0;', 'lap agrees with virtual clock'),
    ('seconds-redraw', 'if (ck_me->tab == TAB_WATCH) ck_watch_tick();', 'if (ck_me->tab == TAB_WATCH) ink_ui_redraw();', 'second ticks update only the digits'),
    ('minutes-redraw', 'if (ck_me->tab == TAB_WORLD) ck_world_minute();', 'if (ck_me->tab == TAB_WORLD) ink_ui_redraw();', 'minute tick preserves world-clock controls'),
    ('member', 'const char *id = ink_member_current()->id; int free = -1;', 'const char *id = members[0].id[0] ? members[0].id : ink_member_current()->id; int free = -1;', 'child gets an independent stopwatch'),
    ('world-member', '.title = TR_NOOP("世界时钟"), .scope = INK_DB_MEMBER', '.title = TR_NOOP("世界时钟"), .scope = INK_DB_DEVICE', 'child does not see adult cities'),
    ('world-zone', 'ink_time_to_zone(zone, utc, &there)', 'ink_time_to_zone(zone * 0, utc, &there)', 'before DST:'),
    ('clock-change', '(void)a; time_changed(ctx, 0, id);', '(void)a; (void)ctx; (void)id;', 'same device minute still refreshes the changed date'),
    ('minute-deadline', '    minute_cb(NULL);\n    ink_cap_emit("time.changed", NULL);', '    if (!T.minute_at.armed) minute_cb(NULL);\n    ink_cap_emit("time.changed", NULL);', 'after DST:'),
)
summary = []
for name, old, new, expected in cases:
    if a.case and name not in a.case:
        continue
    path = {'minute-deadline': 'system/services/time/time_service.c', 'elapsed': 'system/apps/clock/clock_infinitime.cpp',
            'lap': 'system/apps/clock/clock_infinitime.cpp', 'world-member': 'system/apps/clock/clock_world.c',
            'world-zone': 'system/apps/clock/clock_world.c'}.get(name, 'system/apps/clock/clock.c')
    source = (ROOT / path).read_text()
    assert source.count(old) == 1, name
    obj, binary = area / (name + '.o'), area / name
    link = shlex.split((build / 'CMakeFiles/sim-clock-extras.dir/link.txt').read_text())
    language = ['-x', 'c'] if path.endswith('.c') else ['-x', 'c++', '-std=gnu++20', '-fno-exceptions', '-fno-rtti',  # clock-3
                '-I', str(ROOT / 'system/apps/clock/vendor/infinitime/port'), '-I', str(ROOT / 'system/apps/clock/vendor/infinitime/src')]
    subprocess.run([link[0], *language, *(cxx_options if path.endswith('.cpp') else options), '-iquote', str((ROOT / path).parent), '-', '-c', '-o', str(obj)],
                   input=source.replace(old, new), text=True, cwd=build, check=True, capture_output=True)
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, part in enumerate(link) if part.endswith('libink_system.a')), str(obj))
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    r = subprocess.run([sys.executable, '-B', str(ROOT / 'tests/clock_extras.py'), '--build', str(build), '--binary', str(binary)],
                       env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, capture_output=True, text=True, timeout=240)
    (area / (name + '.log')).write_text(r.stdout + r.stderr)
    metrics = json.loads((build / 'clock-extras-evidence' / name / 'create/metrics.json').read_text())['metrics']
    assert r.returncode and expected in r.stdout + r.stderr and metrics.get('test.finished') == 1, (name, r.stdout, r.stderr[-3000:])
    summary.append({'case': name, 'compile': 0, 'exit': r.returncode, 'fail': metrics.get('test.fail'), 'finished': 1, 'expected': expected})
    print(f'{name}: rejected, fail={metrics.get("test.fail")}, finished=1', flush=True)
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
