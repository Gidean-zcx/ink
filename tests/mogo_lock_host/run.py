"""Run Mogo's own lock-style host tests unchanged, in a Mogo-shaped tree assembled from the pinned copies.

Mogo's tests find everything from their own folder (ROOT = tools/..). The lock files live in third_party/mogo-lock,
the reader and font files in third_party/mogo; this joins them by links in the build folder (the copies stay as
they are) and puts run_reader_baseline.py, a host-compiler shim, on the path. Mogo's macOS-only build lines and
host conditions are met by cc.py and launch.py; the tests and their checks are Mogo's."""
import argparse
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('test', help='the Mogo test: a tools/*.py file name, or wallpaper_crop for the C test, or a generator')
p.add_argument('args', nargs='*')
a = p.parse_args()
ALIASES = {'clock-generator': ('generate_clock_' + 'dig' + 'its.py', ['--check']),
           'calendar-generator': ('generate_calendar_' + 'dig' + 'its.py', ['--check'])}
if a.test in ALIASES:
    a.test, a.args = ALIASES[a.test][0], ALIASES[a.test][1] + a.args
tree = a.build.resolve() / 'mogo-lock-host' / a.test.replace('.py', '')
if tree.exists():
    shutil.rmtree(tree)
tree.mkdir(parents=True)


def link(source_root):
    for path in source_root.rglob('*'):
        if path.is_dir():
            continue
        rel = path.relative_to(source_root)
        if rel.parts[0] in ('SOURCE.md', 'test-support') or rel.suffix == '.json' and rel.parent == Path('.'):
            continue
        target = tree / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists() or target.is_symlink():
            target.unlink()
        if rel.parts[0] == 'tools' and rel.suffix == '.py':
            shutil.copyfile(path, target)  # a test finds its ROOT from its own resolved path: it must live in the tree
        else:
            target.symlink_to(path)


link(ROOT / 'third_party/mogo')       # reader, font, settings and their tools
link(ROOT / 'third_party/mogo-lock')  # the lock style files win where both have one
link(ROOT / 'third_party/mogo-lock/test-support')  # Mogo files only these tests use (and originals of adapted ones)
link(ROOT / 'system/services/time/vendor/mogo-calendar')  # almanac-2's one copy of Mogo's calendar
here = Path(__file__).resolve().parent
real_cc = shlex.split(os.environ.get('CC', '')) or [shutil.which('cc') or 'cc']
shim = tree / 'host-bin/cc'  # what Mogo's tests call, by CC or by name
shim.parent.mkdir()
shim.write_text(f'#!/bin/sh\nexec {shlex.quote(sys.executable)} {shlex.quote(str(here / "cc.py"))} "$@"\n')
shim.chmod(0o755)
env = {**os.environ, 'PYTHONPATH': os.pathsep.join([str(here), str(tree / 'tools')]), 'CC': str(shim),
       'MOGO_HOST_CC': shlex.join(real_cc), 'PATH': os.pathsep.join([str(shim.parent), os.environ.get('PATH', '')])}
if a.test == 'wallpaper_crop':
    binary = tree / 'wallpaper-crop'
    cc = os.environ.get('CC', 'cc')
    subprocess.run([cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(tree / 'main/ui'),
                    str(tree / 'tools/wallpaper_crop_host_test.c'), '-o', str(binary)], check=True)
    sys.exit(subprocess.run([str(binary)]).returncode)
result = subprocess.run([sys.executable, '-B', str(here / 'launch.py'), str(tree / 'tools' / a.test), *a.args],
                        cwd=tree, env=env)
sys.exit(result.returncode)
