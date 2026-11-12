"""Withdraw four shared-control fixes without editing product sources or requiring Git history."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
args = parser.parse_args()
build = args.build.resolve()
area = build / 'design-lang-1/reverse'
area.mkdir(parents=True, exist_ok=True)
flags = {}
for line in (build / 'CMakeFiles/ink_system.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
mutations = [
    ('font-merge', 'theme.c',
     '    if (px == INK_FONT_LIST) px = INK_FONT_BODY;\n', ''),
    ('group-frame', 'theme.c',
     'lv_obj_set_style_border_width(obj, border ? ink_ui_px(INK_STROKE) : 0, 0);',
     'lv_obj_set_style_border_width(obj, border || surface == INK_SURFACE_STATIC ? ink_ui_px(INK_STROKE) : 0, 0);'),
    ('clear-background', 'theme.c',
     'lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);\n    bind(b, cb, ctx);',
     'lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);\n    bind(b, cb, ctx);'),
    ('field-focus', 'textfield.c',
     'lv_obj_set_style_border_width(f->area, ink_ui_px(INK_STROKE_FOCUS), LV_STATE_FOCUSED);',
     'lv_obj_set_style_border_width(f->area, ink_ui_px(INK_FIELD_STROKE), LV_STATE_FOCUSED);'),
]
results = []
for name, filename, fixed, withdrawn in mutations:
    original = (ROOT / 'system/ui' / filename).read_text()
    assert original.count(fixed) == 1, name
    source, obj, binary = area / f'{name}.c', area / f'{name}.o', area / name
    source.write_text(original.replace(fixed, withdrawn))
    link = shlex.split((build / 'CMakeFiles/sim-ui-regressions.dir/link.txt').read_text())
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-c', str(source), '-o', str(obj)], cwd=build, check=True)
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, part in enumerate(link) if part.endswith('.a')), str(obj))
    subprocess.run(link, cwd=build, check=True)
    out = area / f'{name}-out'
    run = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/ui_shell_regressions.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--quiet', '--no-random-outages'], env={**os.environ, 'INK_UI_REGRESSION': 'theme'},
        capture_output=True, text=True, timeout=120)
    (area / f'{name}.log').write_text(run.stdout + run.stderr)
    metrics = json.loads((out / 'metrics.json').read_text())['metrics']
    assert run.returncode and metrics.get('test.finished') == 1 and metrics.get('test.fail', 0), (name, metrics)
    results.append({'withdrawn': name, 'exit': run.returncode, 'failures': metrics['test.fail']})
(area / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
