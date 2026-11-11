"""Build and run the four private UI fixtures; --reverse additionally uses baseline objects, without editing sources."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
BASELINE = '7bb9386a0330eb6be71301a06579e7f26aad73a2'
FILES = {'settings': 'settings_pages.c', 'time': 'time_settings.c',
         'members': 'member_pages.c', 'bluetooth': 'bluetooth_page.c'}
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--reverse', action='store_true', help='manual historical comparison; never required by CTest')
p.add_argument('--screenshots', type=Path)
p.add_argument('--case', choices=FILES, action='append')
a = p.parse_args()
build = a.build.resolve()
area = build / 'ui-fix-b'
area.mkdir(exist_ok=True)
flags = (build / 'CMakeFiles/sim-time-settings.dir/flags.make').read_text()
opts = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    opts += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
template = shlex.split((build / 'CMakeFiles/sim-time-settings.dir/link.txt').read_text())
results = []
for mode in (('after', 'before') if a.reverse else ('after',)):
    for case in a.case or FILES:
        defines = []
        if mode == 'before':
            for macro, name in [('INK_TEST_SOURCE', FILES[case])] + ([('INK_TEST_PINPAD', 'pinpad.c')] if case == 'members' else []):
                target = area / ('baseline-' + name)
                source = subprocess.run(['git', 'show', BASELINE + ':system/ui/shell/' + name],
                                        cwd=ROOT, capture_output=True, check=True).stdout
                target.write_bytes(source)
                defines.append(f'-D{macro}="{target}"')
            defines.append('-DINK_UI_FIX_BASELINE=1')
        obj, binary = area / f'{case}-{mode}.o', area / f'{case}-{mode}'
        compile_cmd = [template[0], *opts, *defines, '-iquote', str(ROOT / 'system/ui/shell'),
                       '-c', str(ROOT / 'tests/ui_fix_b' / (case + '.c')), '-o', str(obj)]
        r = subprocess.run(compile_cmd, cwd=build, capture_output=True, text=True)
        (area / f'{case}-{mode}-compile.log').write_text(r.stdout + r.stderr)
        if r.returncode:
            raise SystemExit(r.stderr)
        link = [str(obj) if v.endswith('/tests/time_settings.c.o') else v for v in template]
        link[link.index('-o') + 1] = str(binary)
        subprocess.run(link, cwd=build, capture_output=True, check=True)
        for font in ('100', '120'):
            out = area / f'{case}-{mode}-{font}'
            out.mkdir(exist_ok=True)
            flash = out / 'user.img'
            flash.unlink(missing_ok=True)
            cmd = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                   '--script', str(ROOT / 'tests/ui_fix_b/scene.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
                   '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
                   '--flash-image', str(flash), '--sd-dir', str(out / 'sd'), '--quiet', '--no-random-outages']
            r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=180,
                               env={**os.environ, 'INK_UI_FIX_FONT': font, 'UBSAN_OPTIONS': 'halt_on_error=1'})
            (out / 'run.log').write_text(r.stdout + r.stderr)
            data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
            counts = data.get('metrics', {})
            failures = counts.get('test.fail', 0)
            okay = counts.get('test.finished') == 1 and ((not r.returncode and not failures and not data.get('warnings'))
                   if mode == 'after' else failures > 0 and any('UX' in w for w in data.get('warnings', [])))
            results.append({'case': case, 'mode': mode, 'font': font, 'exit': r.returncode,
                            'pass': counts.get('test.pass', 0), 'fail': failures, 'accepted': okay})
            print(json.dumps(results[-1]), flush=True)
            if not okay:
                raise SystemExit(r.stderr[-6000:] + '\n' + '\n'.join(data.get('warnings', [])))
            if a.screenshots:
                a.screenshots.mkdir(parents=True, exist_ok=True)
                for path in out.glob('snap-*.png'):
                    dest = a.screenshots / f'{case}-{font}-{mode}-{path.stem[5:]}.png'
                    Image.open(path).convert('L').save(dest, optimize=True)
                    assert dest.stat().st_size <= 300 * 1024, dest.name
(area / 'summary.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')
