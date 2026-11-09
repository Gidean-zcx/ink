#!/usr/bin/env python3
"""Check stack accounting with the configured compiler, including sanitizer flags."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', default='build')
p.add_argument('--baseline')
a = p.parse_args()
build = (ROOT / a.build).resolve()
work = Path(tempfile.mkdtemp(prefix='services-stack-', dir=build))
flags = {}
for line in (build / 'CMakeFiles/ink_sim_core.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        k, v = line.split(' = ', 1)
        flags[k] = shlex.split(v)
compiler = shlex.split((build / 'CMakeFiles/sha256-test.dir/link.txt').read_text())[0]
defines = []
if a.baseline:
    source = work / 'vtk.c'
    source.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':simulator/kernel/vtk/vtk.c'], cwd=ROOT))
    defines = ['-DINK_TEST_VTK_SOURCE=' + json.dumps(str(source))]
binary = work / 'stack-meter'
subprocess.run([compiler, *flags['C_DEFINES'], *defines, *flags['C_INCLUDES'], *flags['C_FLAGS'],
                str(ROOT / 'tests/services_finish/stack_meter.c'), '-pthread', '-lm', '-o', str(binary)],
               cwd=build, check=True)
r = subprocess.run([str(binary)], env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'}, check=False)
if r.returncode == 0 and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(work, ignore_errors=True)  # a pass leaves nothing behind; INK_KEEP_EVIDENCE=1 keeps it
else:
    print('Evidence directory:', work.relative_to(ROOT), flush=True)
raise SystemExit(r.returncode)
