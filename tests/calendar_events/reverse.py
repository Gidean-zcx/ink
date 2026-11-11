"""Manual mutation controls: recompile one provider outside the source tree; require decisive failures."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'calendar-events-reverse'
area.mkdir(exist_ok=True)
commands = {Path(e['file']).resolve(): e for e in json.loads((build / 'compile_commands.json').read_text())}
link = shlex.split((build / 'CMakeFiles/sim-calendar-events.dir/link.txt').read_text())
CASES = [
    ('silent', 'system/services/time/time_service.c',
     [('!a->enabled || a->silent || !a->next_s', '!a->enabled || !a->next_s')],
     'silent event emits no fired event'),
    ('month', 'system/apps/calendar/calendar.c',
     [('marks |= a.silent ? 1u : 2u', 'marks |= a.silent ? 2u : 1u')],
     'month distinguishes event and reminder markers'),
    ('cross-day', 'system/services/time/time_service.c',
     [('from_s - (alarm->duration_s > 0 ? alarm->duration_s - 1 : 0)', 'from_s')],
     'range includes overlapping silent event and exclusive end'),
    ('all-day', 'system/services/time/time_caps.c',
     [('if (a.all_day) a.hour = a.min = 0;', '')],
     'all-day is local midnight to next midnight'),
    ('member', 'system/services/time/time_service.c',
     [('T.a[i].kind == kind && !strcmp(T.a[i].owner, member)', 'T.a[i].kind == kind')],
     'child range excludes adult records'),
    ('ai-list', 'system/services/time/time_caps.c',
     [('bool ranged = !strcmp(c->cap->id, "calendar.list");', 'bool ranged = false;')],
     'range excludes finished event'),
    ('ai-add', 'system/services/time/time_caps.c',
     [('a.silent = !ring;', 'a.silent = false;')],
     'AI creates silent event with end'),
]
for name, relative, changes, expected in CASES:
    source_path = ROOT / relative
    source = source_path.read_text()
    for old, new in changes:
        assert source.count(old) == 1, (name, old)
        source = source.replace(old, new)
    mutated = area / f'{name}.c'
    mutated.write_text(source)
    entry = commands[source_path]
    cmd = shlex.split(entry['command']) if 'command' in entry else list(entry['arguments'])
    obj = area / f'{name}.o'
    cmd[cmd.index('-o') + 1] = str(obj)
    cmd[cmd.index(str(source_path))] = str(mutated)
    cmd += ['-I', str(source_path.parent)]
    compile_run = subprocess.run(cmd, cwd=entry.get('directory', build), capture_output=True, text=True)
    (area / f'{name}-compile.log').write_text(compile_run.stdout + compile_run.stderr)
    assert compile_run.returncode == 0, (name, compile_run.stderr[-2000:])
    binary = area / f'sim-{name}'
    cmd = list(link)
    cmd.insert(next(i for i, arg in enumerate(cmd) if arg.endswith('.a')), str(obj))
    cmd[cmd.index('-o') + 1] = str(binary)
    linked = subprocess.run(cmd, cwd=build, capture_output=True, text=True)
    assert linked.returncode == 0, linked.stderr
    result = subprocess.run([sys.executable, str(ROOT / 'tests/calendar_events/run.py'), '--build', str(build),
                             '--binary', str(binary)], env={**os.environ, 'INK_KEEP_EVIDENCE': '1'},
                            capture_output=True, text=True)
    (area / f'{name}-test.log').write_text(result.stdout + result.stderr)
    assert result.returncode != 0 and expected in result.stdout, (name, result.stdout[-2500:], result.stderr[-1000:])
    print(f'{name}: compile=0 link=0 test={result.returncode}; expected failure: {expected}', flush=True)
print('PASS: all seven controls fail at the required assertions; product sources and executable unchanged')
