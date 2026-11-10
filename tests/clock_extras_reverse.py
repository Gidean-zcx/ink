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
a = p.parse_args()
build = a.build.resolve()
area = build / 'clock-extras-reverse'
area.mkdir(exist_ok=True)
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
cases = (
    ('elapsed', 'osal_now_us() - s->since', 's->since - s->since', 'running elapsed agrees with virtual clock'),
    ('lap', 's->laps[s->count++] = us', 's->laps[s->count++] = 0', 'lap agrees with virtual clock'),
    ('seconds-redraw', '(void)t; extra_update();', '(void)t; ink_ui_redraw();', 'second ticks update only the digits'),
    ('minutes-redraw', 'if (X.world) extra_update();', 'if (X.world) ink_ui_redraw();', 'minute tick preserves world-clock controls'),
    ('member', 'static stopwatch_t *my_watch(void) {', 'static stopwatch_t *my_watch(void) { return &watches[0];', 'child gets an independent stopwatch'),
    ('world-member', '.title = TR_NOOP("世界时钟"), .scope = INK_DB_MEMBER', '.title = TR_NOOP("世界时钟"), .scope = INK_DB_DEVICE', 'child does not see adult cities'),
    ('world-zone', 'ink_time_to_zone(z, utc, &there)', 'ink_time_to_zone(z * 0, utc, &there)', 'before DST:'),
    ('clock-change', '(void)a; extra_changed(ctx, 0, id);', '(void)a; (void)ctx; (void)id;', 'same device minute still refreshes the changed date'),
    ('minute-deadline', '    minute_cb(NULL);\n    ink_cap_emit("time.changed", NULL);', '    if (!T.minute_at.armed) minute_cb(NULL);\n    ink_cap_emit("time.changed", NULL);', 'after DST:'),
)
summary = []
for name, old, new, expected in cases:
    path = 'system/services/time/time_service.c' if name == 'minute-deadline' else 'system/apps/clock/clock.c'
    source = (ROOT / path).read_text()
    assert source.count(old) == 1, name
    obj, binary = area / (name + '.o'), area / name
    link = shlex.split((build / 'CMakeFiles/sim-clock-extras.dir/link.txt').read_text())
    subprocess.run([link[0], *options, '-iquote', str((ROOT / path).parent), '-x', 'c', '-', '-c', '-o', str(obj)],
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
