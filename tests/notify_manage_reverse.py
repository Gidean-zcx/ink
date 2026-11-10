"""Manual negative controls; registered CTest checks do not require git history."""
import argparse
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
area = build / 'notify-manage-reverse'
area.mkdir(exist_ok=True)
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
cases = (
    ('dismiss-binding', 'system/ui/shell/navigation.c',
     'false, NULL, NULL), "notify.dismiss", N.notify_args[at]);',
     'false, NULL, NULL), "notify.read", N.notify_args[at]);',
     'touch deletes only the chosen notice'),
    ('channel-binding', 'system/ui/shell/navigation.c',
     '"notify.set_channel", args);', '"notify.channels", args);',
     'touch disables message channel'),
    ('child-policy', 'system/services/notify/notify_caps.c',
     '.child_ok = false,', '.child_ok = true,',
     'child channel edits rejected for AI'),
    ('read-refresh', 'system/ui/shell/navigation.c',
     'if (ink_cap_subscribe("notify.changed", notify_changed, NULL, INK_EXEC_UI)) return -1;',
     'if (ink_cap_subscribe("notify.never", notify_changed, NULL, INK_EXEC_UI)) return -1;',
     'read-notice deletion refreshes list without unread change'),
    ('page-bindings', 'system/ui/shell/navigation.c',
     'if (!(index % (size_t)N.notify_capacity)) memset(N.notify_ids, 0, sizeof N.notify_ids);',
     '(void)index;', 'returned notification page has no stale fields or actions'),
)
for name, path, old, new, expected in cases:
    source = (ROOT / path).read_text()
    assert source.count(old) == 1, name
    obj, binary = area / (name + '.o'), area / name
    link = shlex.split((build / 'CMakeFiles/sim-notify-manage.dir/link.txt').read_text())
    subprocess.run([link[0], *options, '-iquote', str((ROOT / path).parent), '-x', 'c', '-', '-c', '-o', str(obj)],
                   input=source.replace(old, new), text=True, cwd=build, capture_output=True, check=True)
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, part in enumerate(link) if part.endswith('libink_system.a')), str(obj))
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    result = subprocess.run([sys.executable, '-B', str(ROOT / 'tests/notify_manage.py'), '--build', str(build),
                             '--binary', str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=200)
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode and expected in result.stdout + result.stderr, (name, result.stdout, result.stderr[-3000:])
    print(name + ': expected failure (' + expected + ')', flush=True)
