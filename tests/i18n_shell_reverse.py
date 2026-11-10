"""Manual reverse controls: require new, specific failures beyond the recorded positive run."""
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
area = build / 'i18n-shell-reverse'
area.mkdir(parents=True, exist_ok=True)
positive = json.loads((build / 'test-out/i18n-shell/metrics.json').read_text())
positive_warnings = '\n'.join(positive.get('warnings', []))
source = ROOT / 'system/ui/shell/settings_pages.c'
original = source.read_text()
before = hashlib.sha256(source.read_bytes()).hexdigest()
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
cases = [
    ('title', 'ink_shell_topbar(screen, TR(PAGES[S.page].title))',
     'ink_shell_topbar(screen, PAGES[S.page].title)', 'SetVoice-en-1 has expected text: Voice & AI'),
    ('license-line', 'if (S.page == LICENSES) lv_obj_set_y(text, ink_ui_px(y + ink_ui_text_height(caption, INK_FONT_LIST, INK_WEIGHT_MEDIUM, w - 2 * INK_GAP_XL) + INK_GAP_XS));',
     '', 'Licenses-en-3 layout:'),
]
commands, results = [], []


def run(command, **kwargs):
    commands.append([str(x) for x in command])
    return subprocess.run(command, cwd=build, capture_output=True, text=True, timeout=180, **kwargs)


for name, needle, replacement, expected in cases:
    assert original.count(needle) == 1, name
    assert expected not in positive_warnings, 'the positive run must pass the exact reverse assertion'
    obj = area / f'{name}.o'
    binary = area / name
    link = shlex.split((build / 'CMakeFiles/sim-i18n-shell.dir/link.txt').read_text())
    compiled = run([link[0], *options, '-iquote', str(source.parent), '-x', 'c', '-', '-c', '-o', str(obj)],
                   input=original.replace(needle, replacement))
    assert compiled.returncode == 0, compiled.stderr
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, x in enumerate(link) if x.endswith('libink_system.a')), str(obj))
    linked = run(link)
    assert linked.returncode == 0, linked.stderr
    out = area / ('out-' + name)
    result = run([os.sys.executable, str(ROOT / 'tests/i18n_shell.py'), '--build', str(build),
                  '--binary', str(binary), '--out', str(out)], env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    (area / f'{name}.log').write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text())
    warnings = '\n'.join(data.get('warnings', []))
    assert result.returncode and expected in warnings, (name, result.stdout, result.stderr)
    assert hashlib.sha256(source.read_bytes()).hexdigest() == before, 'product source must stay unchanged'
    results.append({'case': name, 'exit': result.returncode, 'new_failure': expected})
    print(f'{name}: expected new failure, exit={result.returncode}', flush=True)
(area / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
(area / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
