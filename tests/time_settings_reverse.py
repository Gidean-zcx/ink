"""Manual mutation controls; CTest never requires repository history."""
import argparse
import json
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
area = build / 'time-settings-reverse'
area.mkdir(exist_ok=True)
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
cases = (
    ('auto-apply', 'system/services/time/time_service.c',
     'if (v) { on_online(ctx); ink_hub_demand(); }', '(void)ctx; (void)v;',
     'switching back to auto immediately uses current network time'),
    ('network-age', 'system/services/hub/hub_link.c',
     '*unix_ms = H.server_ms + (int64_t)((osal_now_us() - H.hello_sent_us) / 1000u) - H.hello_rtt_ms;',
     '*unix_ms = H.server_ms;', 'switching back to auto immediately uses current network time'),
    ('network-zone', 'system/services/time/time_service.c',
     'if (ink_setting_get("time.zone_auto") && network >= 0) return network;',
     'if (network < -1) return network;', 'automatic and manual zone selections stay separate'),
    ('old-us-rule', 'system/services/time/time_service.c',
     'start = sunday_of(y, y < 2007 ? 4 : 3, y < 2007 ? 1 : 2)',
     'start = sunday_of(y, 3, 2)', '2006 uses April transition rather than the post-2007 March rule'),
    ('minute-redraw', 'system/ui/shell/time_settings.c',
     'if (!strcmp(id, "time.minute")) {', 'if (false) {', 'minute tick updates only the clock label'),
    ('page-entry', 'system/ui/shell/time_settings.c',
     'memset(&P.pos, 0, sizeof P.pos); ink_ui_push',
     'if (page == TIME) return;\n    memset(&P.pos, 0, sizeof P.pos); ink_ui_push',
     'touch opens Date & Time'),
)
summary = []
for name, path, old, new, expected in cases:
    source = (ROOT / path).read_text()
    assert source.count(old) == 1, name
    obj, binary = area / (name + '.o'), area / name
    link = shlex.split((build / 'CMakeFiles/sim-time-settings.dir/link.txt').read_text())
    subprocess.run([link[0], *options, '-iquote', str((ROOT / path).parent), '-x', 'c', '-', '-c', '-o', str(obj)],
                   input=source.replace(old, new), text=True, cwd=build, check=True, capture_output=True)
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, part in enumerate(link) if part.endswith('libink_system.a')), str(obj))
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    result = subprocess.run([sys.executable, '-B', str(ROOT / 'tests/time_settings.py'), '--build', str(build),
                             '--binary', str(binary)], cwd=ROOT, capture_output=True, text=True, timeout=200)
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode and expected in result.stdout + result.stderr, (name, result.stdout, result.stderr[-2000:])
    summary.append({'case': name, 'exit': result.returncode, 'expected_failure': expected})
    print(name + ': expected failure (' + expected + ')', flush=True)
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
