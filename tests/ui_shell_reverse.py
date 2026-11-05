#!/usr/bin/env python3
"""Build isolated review mutations in build/, never edit product files or budgets.

Run serially after the ordinary build. Requires CMake's Unix Makefiles generator.
Success means the positive snapshot fixture passes and every mutation is caught.
"""
import argparse
import json
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
REVIEW_HEAD = 'cab5c46'


def old(path):
    return subprocess.check_output(['git', 'show', f'{REVIEW_HEAD}:{path}'], cwd=ROOT, text=True)


def compile_variant(build, stem, source, source_target, base_target, replace_test=False):
    flags = []
    for line in (build / f'CMakeFiles/{source_target}.dir/flags.make').read_text().splitlines():
        if line.startswith(('C_DEFINES =', 'C_INCLUDES =', 'C_FLAGS =')):
            flags += shlex.split(line.split('=', 1)[1])
    link = shlex.split((build / f'CMakeFiles/{base_target}.dir/link.txt').read_text())
    obj, target = build / f'review-negative/{stem}.o', build / f'review-negative/{stem}'
    obj.parent.mkdir(exist_ok=True)
    subprocess.run([link[0], *flags, '-I' + str(ROOT / 'tests'), '-I' + str(ROOT / 'system/ui/shell'),
                    '-x', 'c', '-c', '-', '-o', str(obj)], input=source, text=True, check=True, cwd=ROOT)
    if replace_test:
        link = [str(obj) if x == f'CMakeFiles/{base_target}.dir/tests/ui_shell_scenes.c.o' else x for x in link]
    else:
        link.insert(1, str(obj))
    link = [str(target) if x == base_target else x for x in link]
    subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
    return target


def run(build, binary, case, scene=None, regression=None, expect_failure=True):
    folder = build / f'test-out/review-negative-{case}'
    env = dict(os.environ)
    if scene: env['INK_UI_SCENE'] = scene
    if regression: env['INK_UI_REGRESSION'] = regression
    script = 'ui_shell.script' if scene else 'ui_shell_regressions.script'
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests' / script), '--hub', str(ROOT / 'tests/ui_shell.hub.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
        '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--out', str(folder), '--quiet', '--no-random-outages'],
        cwd=ROOT, env=env, capture_output=True, text=True)
    data = json.loads((folder / 'metrics.json').read_text())
    failed = data['metrics'].get('test.fail', 0)
    print(f'{case}: exit={result.returncode}, test.fail={failed}, expected={"failure" if expect_failure else "pass"}')
    for warning in data.get('warnings', [])[:3]: print(' ', warning.split(') ', 1)[-1])
    if bool(result.returncode) != expect_failure or bool(failed) != expect_failure:
        raise SystemExit(f'{case}: reverse control did not produce the expected result')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build', type=Path, default=ROOT / 'build')
    a = ap.parse_args()
    build = a.build.resolve()
    # 002: the original checker fails the same permanent negative examples.
    original = {'__file__': str(ROOT / 'tools/ui_scale_check.py'), '__name__': 'original_checker'}
    exec(compile(old('tools/ui_scale_check.py'), 'original_checker', 'exec'), original)
    tests = runpy.run_path(str(ROOT / 'tests/ui_scale_check_test.py'))
    case = tests['ScaleCheck']
    case.check_source.__globals__['CHECK'] = original['check']
    result = unittest.TestResult()
    unittest.defaultTestLoader.loadTestsFromTestCase(case).run(result)
    print(f'002-original-checker: failures={len(result.failures)}, errors={len(result.errors)}')
    if not result.failures or result.errors: raise SystemExit('002: invalid reverse control')
    scenes = (ROOT / 'tests/ui_shell_scenes.c').read_text()
    missing = scenes.replace('static void prepare(void) {', 'static void prepare(void) {\n    if (!strcmp(scene, "Settings")) return;', 1)
    binary = compile_variant(build, '001-missing-settings', missing, 'sim-ui-scenes', 'sim-ui-scenes', True)
    run(build, binary, '001-missing-settings', scene='Settings')
    # An actual pending frame, rather than a longer sleep, tests the wait gate.
    pending = scenes.replace('static void snapshot_now(void) {',
        'static void pending_frame(void) { lv_obj_invalidate(lv_screen_active()); ink_ui_hint_frame(INK_REFRESH_FLASH_UI); }\nstatic void snapshot_now(void) {\n    on_ui(pending_frame);', 1)
    binary = compile_variant(build, '004-pending-positive', pending, 'sim-ui-scenes', 'sim-ui-scenes', True)
    run(build, binary, '004-pending-positive', scene='Apps', expect_failure=False)
    early = pending.replace('    if (!quiet()) return;\n    vtk_event_at', '    vtk_event_at', 1)
    binary = compile_variant(build, '004-without-wait', early, 'sim-ui-scenes', 'sim-ui-scenes', True)
    run(build, binary, '004-without-wait', scene='Apps')
    overlapping = (ROOT / 'system/ui/shell/confirm_card.c').read_text().replace('int extra = C.pages > 1 ? ink_ui_px(INK_GAP_XXL) : 0;', 'int extra = 0;', 1)
    binary = compile_variant(build, '003-overlapping-pager', overlapping, 'ink_system', 'sim-ui-regressions')
    run(build, binary, '003-overlapping-pager', regression='confirm')
    for name, path, regression in (
        ('003-original-card', 'system/ui/shell/confirm_card.c', 'confirm'),
        ('003-original-detail-limit', 'system/policy/confirm.c', 'confirm'),
        ('001-original-action-list', 'system/policy/cap.c', 'context'),
        ('005-original-dispatch', 'system/ui/ui_task.c', 'confirm'),
        ('006-original-mapping', 'system/ui/shell/navigation.c', 'slider'),
        ('006-original-touch-layer', 'system/ui/theme.c', 'slider'),
    ):
        binary = compile_variant(build, name, old(path), 'ink_system', 'sim-ui-regressions')
        run(build, binary, name, regression=regression)
    binary = compile_variant(build, '008-original-context', old('system/ui/shell/navigation.c'), 'ink_system', 'sim-ui-scenes')
    run(build, binary, '008-original-context', scene='SetDisplay')
    run(build, build / 'review-negative/006-original-mapping', '008-original-overview', regression='context')
    print('All review reverse controls were detected; product sources stayed unchanged.')


if __name__ == '__main__': main()
