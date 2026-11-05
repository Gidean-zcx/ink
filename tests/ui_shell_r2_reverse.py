#!/usr/bin/env python3
"""Check round-two fixes with isolated, serial build-directory mutations."""
import argparse
from pathlib import Path
import runpy
import subprocess
import unittest
from ui_shell_reverse import ROOT, compile_variant, run

BASE = '024c800'


def old(path):
    return subprocess.check_output(['git', 'show', f'{BASE}:{path}'], cwd=ROOT, text=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build', type=Path, default=ROOT / 'build')
    a = ap.parse_args()
    build = a.build.resolve()
    original = {'__file__': str(ROOT / 'tools/ui_scale_check.py'), '__name__': 'r2_original'}
    exec(compile(old('tools/ui_scale_check.py'), 'r2_original', 'exec'), original)
    case = runpy.run_path(str(ROOT / 'tests/ui_scale_check_test.py'))['ScaleCheck']
    case.check_source.__globals__['CHECK'] = original['check']
    result = unittest.TestResult()
    unittest.defaultTestLoader.loadTestsFromTestCase(case).run(result)
    print(f'r2-001-original-checker: failures={len(result.failures)}, errors={len(result.errors)}')
    if not result.failures or result.errors: raise SystemExit('invalid checker reverse control')
    for stem, path, mode in (
        ('r2-002-original-state', 'system/ui/shell/navigation.c', 'privacy'),
        ('r2-003-original-pages', 'system/ui/shell/ai_panel.c', 'answer'),
    ):
        binary = compile_variant(build, stem, old(path), 'ink_system', 'sim-ui-regressions')
        run(build, binary, stem, regression=mode)
    nav = (ROOT / 'system/ui/shell/navigation.c').read_text()
    guard = '    if (!current_session((uint32_t)(request >> 1))) return;'
    if guard not in nav: raise SystemExit('missing receipt guard mutation target')
    binary = compile_variant(build, 'r2-002-late-receipt', nav.replace(guard, '', 1), 'ink_system', 'sim-ui-regressions')
    run(build, binary, 'r2-002-late-receipt', regression='privacy')
    print('All round-two reverse controls were detected; product files stayed unchanged.')


if __name__ == '__main__': main()
