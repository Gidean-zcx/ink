"""Withdraw shared-control fixes without editing product sources or requiring Git history."""
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
    ('digit-weight', 'theme.c',
     '    if (px > INK_FONT_HEADING && weight == INK_WEIGHT_REGULAR) weight = INK_WEIGHT_MEDIUM;\n', ''),
    ('double-row', 'theme.c',
     'int value_y = two_lines ? title_y + ink_ui_line_height(INK_FONT_LIST) : (h - ink_ui_line_height(INK_FONT_NOTE)) / 2;',
     'int value_y = (h - ink_ui_line_height(INK_FONT_NOTE)) / 2;'),
    ('placeholder-ink', 'textfield.c',
     'lv_obj_set_style_text_color(f->area, ink_ui_ink(), LV_PART_TEXTAREA_PLACEHOLDER);',
     'lv_obj_set_style_text_color(f->area, ink_ui_secondary(), LV_PART_TEXTAREA_PLACEHOLDER);'),
    ('late-icon-target', 'theme.c',
     '    if (size < INK_CONTROL) lv_obj_set_ext_click_area(b, ink_ui_px((INK_CONTROL - size + 1) / 2));\n', ''),
    ('negative-rounding', 'theme.c',
     '    int sign = px < 0 ? -1 : 1;\n'
     '    if (w * INK_CANVAS_H <= h * INK_CANVAS_W) return (px * w + sign * INK_CANVAS_W / 2) / INK_CANVAS_W;\n'
     '    return (px * h + sign * INK_CANVAS_H / 2) / INK_CANVAS_H;',
     '    if (w * INK_CANVAS_H <= h * INK_CANVAS_W) return (px * w + INK_CANVAS_W / 2) / INK_CANVAS_W;\n'
     '    return (px * h + INK_CANVAS_H / 2) / INK_CANVAS_H;'),
    ('font-merge', 'theme.c',
     '    if (px == INK_FONT_LIST) px = INK_FONT_BODY;\n', ''),
    ('group-frame', 'theme.c',
     'lv_obj_set_style_border_width(obj, border ? ink_ui_px(INK_STROKE) : 0, 0);',
     'lv_obj_set_style_border_width(obj, border || surface == INK_SURFACE_STATIC ? ink_ui_px(INK_STROKE) : 0, 0);'),
    ('clear-background', 'theme.c',
     'lv_obj_t *b = ink_ui_box(parent, x, y, size, size, 0, INK_SURFACE_WHITE, false);\n    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);',
     'lv_obj_t *b = ink_ui_box(parent, x, y, size, size, 0, INK_SURFACE_WHITE, false);\n    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);'),
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
