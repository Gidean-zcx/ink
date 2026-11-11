"""Withdraw each shared-style fix in an ignored object; product files stay intact."""
import argparse
import json
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'ui-fidelity/recheck-reverse'
area.mkdir(parents=True, exist_ok=True)
flags = {}
for line in (build / 'CMakeFiles/ink_system.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
mutations = [
    ('static-gray-fill', 'theme.c',
     '    lv_obj_set_style_bg_color(obj, surface == INK_SURFACE_BLACK ? ink_ui_ink() : ink_ui_paper(), 0);',
     '    lv_obj_set_style_bg_color(obj, surface == INK_SURFACE_BLACK ? ink_ui_ink() : surface == INK_SURFACE_STATIC ? lv_color_hex(0x888888) : ink_ui_paper(), 0);'),
    ('decorative-outline', 'theme.c', 'surface == INK_SURFACE_WHITE && cb', 'surface == INK_SURFACE_WHITE'),
    ('toggle-tile', 'theme.c', '    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);\n', ''),
    ('list-icon-circle', 'theme.c',
     '    ink_ui_circle(r, icon, INK_GAP, (h - INK_CIRCLE_SMALL) / 2, INK_CIRCLE_SMALL, INK_ICON_MEDIUM,\n'
     '                  INK_SURFACE_WHITE, NULL, NULL);',
     '    ink_ui_icon(r, icon, INK_ICON_MEDIUM, INK_ICON_LINE, INK_GAP + (INK_CIRCLE_SMALL - INK_ICON_MEDIUM) / 2, (h - INK_ICON_MEDIUM) / 2, false);'),
    ('list-divider', 'theme.c',
     '    if (y && ink_ui_surface_kind(parent) == INK_SURFACE_STATIC) ink_ui_group_divider(r, 0, w);\n', ''),
    ('late-button-outline', 'shell/navigation.c',
     '    if (lv_obj_get_style_radius(obj, 0) == LV_RADIUS_CIRCLE &&\n'
     '        lv_obj_get_style_width(obj, 0) == lv_obj_get_style_height(obj, 0) &&\n'
     '        ink_ui_surface_kind(obj) == INK_SURFACE_WHITE)\n'
     '        lv_obj_set_style_border_width(obj, ink_ui_px(INK_STROKE), 0);\n', ''),
    ('settings-divider', 'shell/settings_pages.c', '            ink_ui_group_divider(bg, 0, w);\n', ''),
    ('settings-row-gap', 'shell/settings_pages.c', 'S.y += h + (join_next ? 0 : INK_STROKE);', 'S.y += h + INK_STROKE;'),
]
results = []
for name, filename, fixed, previous in mutations:
    original = (ROOT / 'system/ui' / filename).read_text()
    assert original.count(fixed) == 1, (name, 'fix must match exactly once')
    source = area / (name + '.c')
    source.write_text(original.replace(fixed, previous))
    obj = source.with_suffix('.o')
    target = 'sim-settings-a' if name.startswith('settings-') else 'sim-ui-regressions'
    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-I', str(ROOT / 'system/ui/shell'), '-c', str(source), '-o', str(obj)], check=True)
    binary = area / name
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, part in enumerate(link) if part.endswith('.a')), str(obj))
    subprocess.run(link, cwd=build, check=True)
    out = area / (name + '-out')
    command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '--script', str(ROOT / 'tests/ui_shell_regressions.script'),
               '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--assets', str(build / 'assets'),
               '--content', str(build / 'test-content'), '--out', str(out), '--no-random-outages', '--quiet']
    if name.startswith('settings-'):
        command = [sys.executable, str(ROOT / 'tests/settings_a.py'), '--build', str(build), '--binary', str(binary)]
        out = build / 'test-out' / ('settings-a-' + binary.name) / 'write'
    run = subprocess.run(command, env={**os.environ, 'INK_UI_REGRESSION': 'theme'},
                         capture_output=True, text=True, timeout=150)
    (area / (name + '.log')).write_text(run.stdout + run.stderr)
    counters = json.loads((out / 'metrics.json').read_text())['metrics']
    assert run.returncode != 0 and counters.get('test.finished') == 1 and counters.get('test.fail', 0) > 0, name
    results.append({'withdrawn_fix': name, 'exit': run.returncode,
                    'failures': counters['test.fail'], 'finished': counters['test.finished']})
driver = runpy.run_path(str(ROOT / 'tests/ui_compare_test.py'))
original_run = runpy.run_path
def older_collector(path, *args, **kwargs):
    if Path(path) != ROOT / 'tests/ui_fidelity.py':
        return original_run(path, *args, **kwargs)
    source = Path(path).read_text()
    fixed = '    -path.stat().st_mtime_ns, str(path)))'
    assert source.count(fixed) == 1
    scope = {'__file__': str(path), '__name__': '<run_path>'}
    exec(compile(source.replace(fixed, '    str(path)))'), str(path), 'exec'), scope)
    return scope
with patch.object(runpy, 'run_path', older_collector):
    result = unittest.TestResult()
    driver['CompareTest']('test_fidelity_collects_the_latest_verified_frame').run(result)
assert len(result.failures) == 1 and not result.errors, (result.failures, result.errors)
results.append({'withdrawn_fix': 'latest-frame-selection', 'failures': len(result.failures)})
(area / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
