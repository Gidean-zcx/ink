#!/usr/bin/env python3
"""Run round-three reverse controls serially, using ignored copies only."""
import argparse
import contextlib
import io
import json
import os
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
BASE = '332cab0'


def previous(path):
    return subprocess.check_output(['git', 'show', f'{BASE}:{path}'], cwd=ROOT, text=True)


def gate(code, source):
    path = ROOT / 'system/ui/shell/navigation.c'
    original = Path.read_text
    def read(self, *args, **kwargs):
        return source if self.resolve() == path else original(self, *args, **kwargs)
    output = io.StringIO()
    env = {'__file__': str(ROOT / 'tools/icongen/check.py'), '__name__': '__main__'}
    with patch.object(Path, 'read_text', read), patch.object(sys, 'argv', ['icon-check']), contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
        try: exec(compile(code, env['__file__'], 'exec'), env)
        except SystemExit as e: return int(e.code), output.getvalue()
    raise AssertionError('checker did not exit')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build', type=Path, default=ROOT / 'build')
    ap.add_argument('--components', action='store_true')
    a = ap.parse_args()
    build = a.build.resolve()
    old = {'__file__': str(ROOT / 'tools/ui_scale_check.py'), '__name__': 'r3_previous'}
    exec(compile(previous('tools/ui_scale_check.py'), 'r3_previous', 'exec'), old)
    case = runpy.run_path(str(ROOT / 'tests/ui_scale_check_test.py'))['ScaleCheck']
    case.check_source.__globals__['CHECK'] = old['check']
    result = unittest.TestResult()
    unittest.defaultTestLoader.loadTestsFromTestCase(case).run(result)
    print(f'r3-001 previous checker: failures={len(result.failures)}, errors={len(result.errors)}')
    assert result.failures and not result.errors

    nav = (ROOT / 'system/ui/shell/navigation.c').read_text()
    needle = 'APP_ICONS[] = {INK_UI_ICON(headphones)'
    assert nav.count(needle) == 1
    mutant = nav.replace(needle, 'APP_ICONS[] = {INK_UI_ICON(missingReviewIcon)', 1)
    current = (ROOT / 'tools/icongen/check.py').read_text()
    for label, code, expected in (('previous', previous('tools/icongen/check.py'), 0), ('fixed', current, 1)):
        rc, output = gate(code, mutant)
        print(f'r3-002 {label} checker, actual APP_ICONS mutation: exit={rc}')
        assert rc == expected
        if expected: assert 'unknown required icon missingReviewIcon' in output

    # Use the exact build target, with a copied checker and the actual mutated
    # navigation source. Other inputs are read-only symlinks to the repository.
    fixture = build / 'review-negative/r3-icon-build'
    fixture.mkdir(parents=True, exist_ok=True)
    for parent in ('assets', 'apps'):
        target = fixture / parent
        if not target.exists(): target.symlink_to(ROOT / parent, target_is_directory=True)
    (fixture / 'tools/icongen').mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROOT / 'tools/icongen/check.py', fixture / 'tools/icongen/check.py')
    (fixture / 'system/ui/shell').mkdir(parents=True, exist_ok=True)
    include = fixture / 'system/include'
    if not include.exists(): include.symlink_to(ROOT / 'system/include', target_is_directory=True)
    # All source files remain visible to the checker, including helper signatures.
    for path in (ROOT / 'system/ui').rglob('*'):
        target = fixture / path.relative_to(ROOT)
        if path.is_dir(): target.mkdir(parents=True, exist_ok=True)
        elif path.name != 'navigation.c' and not target.exists(): target.symlink_to(path)
    copied_nav = fixture / 'system/ui/shell/navigation.c'
    copied_nav.write_text(mutant)
    cmake = (ROOT / 'CMakeLists.txt').read_text()
    block = re.search(r'add_custom_target\(ink_ui_resource_check ALL\n.*?COMMENT "Check required design icon geometry"\)', cmake, re.S)
    assert block
    (fixture / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.16)\nproject(icon_gate NONE)\n' + block[0] + '\n')
    subprocess.run(['cmake', '-S', str(fixture), '-B', str(fixture / 'build'), '-DPython3_EXECUTABLE=' + sys.executable], check=True, capture_output=True, text=True)
    command = ['cmake', '--build', str(fixture / 'build'), '--target', 'ink_ui_resource_check', '-j1']
    failed = subprocess.run(command, capture_output=True, text=True)
    print(f'r3-002 CMake target, missing APP_ICONS entry: exit={failed.returncode}')
    assert failed.returncode and 'unknown required icon missingReviewIcon' in failed.stderr
    copied_nav.write_text(nav)
    passed = subprocess.run(command, capture_output=True, text=True)
    print(f'r3-002 CMake target, restored APP_ICONS entry: exit={passed.returncode}')
    assert not passed.returncode
    # The generated registry rejects a missing token even without the checker.
    compiler = os.environ.get('CC', 'cc')
    for name, expected in (('headphones', False), ('missingReviewIcon', True)):
        source = '#include "ink/ui_icon_names.h"\nconst char *icon=INK_UI_ICON(' + name + ');\n'
        rc = subprocess.run([compiler, '-I' + str(fixture / 'build/generated'), '-x', 'c', '-c', '-', '-o', str(fixture / 'probe.o')], input=source, text=True, capture_output=True)
        print(f'r3-002 compiler registry, {name}: exit={rc.returncode}')
        assert bool(rc.returncode) == expected
    if a.components:
        from ui_shell_reverse import compile_variant
        for stem, path, mode, profile in (
            ('r3-segment-border', 'system/ui/theme.c', 'slider', 'metalio-plus'),
            ('r3-search-icon', 'system/ui/textfield.c', 'snapshot', 'metalio-plus'),
            ('r3-icon-scale', 'system/ui/icons.c', 'snapshot', 'desktop-lcd'),
            ('r3-keyboard-height', 'system/ui/shell/keyboard.c', 'snapshot', 'metalio-plus'),
        ):
            binary = compile_variant(build, stem, previous(path), 'ink_system', 'sim-ui-regressions')
            folder = build / f'test-out/review-negative-{stem}'
            metrics = folder / 'metrics.json'
            metrics.unlink(missing_ok=True)
            command = [str(binary), '--profile', str(ROOT / f'simulator/profiles/{profile}.json'),
                       '--script', str(ROOT / 'tests/ui_shell_regressions.script'),
                       '--hub', str(ROOT / 'tests/ui_shell.hub.json'), '--assets', str(build / 'assets'),
                       '--content', str(build / 'test-content'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
                       '--out', str(folder), '--quiet', '--no-random-outages']
            rc = subprocess.run(command, env=dict(os.environ, INK_UI_REGRESSION=mode), capture_output=True, text=True, timeout=90)
            data = json.loads(metrics.read_text())
            failures = data['metrics'].get('test.fail', 0)
            print(f'{stem}, {profile}: exit={rc.returncode}, test.fail={failures}')
            for warning in data.get('warnings', [])[:3]: print(' ', warning.split(') ', 1)[-1])
            assert rc.returncode == 1 and failures and data['metrics'].get('test.finished') == 1
    print('All round-three gate controls behaved as expected; product sources stayed unchanged.')


if __name__ == '__main__': main()
