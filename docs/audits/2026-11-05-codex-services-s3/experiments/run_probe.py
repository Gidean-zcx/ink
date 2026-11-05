#!/usr/bin/env python3
"""Build an audit-only translation unit with the existing simulator's exact flags/libraries."""
import argparse
import json
import pathlib
import shlex
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('source')
p.add_argument('--template', default='sim-data')
p.add_argument('--build', default='build-audit')
p.add_argument('--end', default='30s')
p.add_argument('--extra', action='append', default=[], help='audit-only replacement translation unit')
p.add_argument('--script')
p.add_argument('--budget')
p.add_argument('--flash-image')
p.add_argument('--sd-dir')
p.add_argument('--profile', default='simulator/profiles/metalio-plus.json')
a = p.parse_args()
root = pathlib.Path(__file__).resolve().parents[4]
build = (root / a.build).resolve()
source = (root / a.source).resolve()
dest = build / 'audit-probes' / source.stem
dest.mkdir(parents=True, exist_ok=True)
flags = {}
for line in (build / 'CMakeFiles' / (a.template + '.dir') / 'flags.make').read_text().splitlines():
    if ' = ' in line:
        k, v = line.split(' = ', 1)
        flags[k] = shlex.split(v)
link = shlex.split((build / 'CMakeFiles' / (a.template + '.dir') / 'link.txt').read_text())
obj = dest / 'probe.o'
cmd = [link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
       '-Wno-unused-function', '-c', str(source), '-o', str(obj)]
subprocess.run(cmd, cwd=build, check=True)
extras = []
for i, filename in enumerate(a.extra):
    extra = dest / ('extra%d.o' % i)
    extra_cmd = cmd.copy()
    extra_cmd[extra_cmd.index('-c') + 1] = str((root / filename).resolve())
    extra_cmd[extra_cmd.index('-o') + 1] = str(extra)
    subprocess.run(extra_cmd, cwd=build, check=True)
    extras.append(str(extra))
for i, word in enumerate(link):
    if '/tests/integration/' in word and word.endswith('.o'):
        link[i] = str(obj)
        break  # Preserve extra wrapper objects in targets such as sim-races.
link[link.index('-o') + 1] = str(dest / 'probe')
first_library = next(i for i, word in enumerate(link) if word.endswith('.a'))
link[first_library:first_library] = extras
subprocess.run(link, cwd=build, check=True)
out = pathlib.Path(tempfile.mkdtemp(prefix='run-', dir=dest))
(dest / 'last-run.txt').write_text(str(out) + '\n')
cmd = [str(dest / 'probe'), '--profile', str((root / a.profile).resolve()),
       '--out', str(out), '--quiet', '--no-random-outages']
if not a.script:
    cmd += ['--end', a.end]
for option in ('script', 'budget', 'flash_image', 'sd_dir'):
    value = getattr(a, option)
    if value:
        cmd += ['--' + option.replace('_', '-'), str((root / value).resolve())]
if (build / 'assets').exists():
    cmd += ['--assets', str(build / 'assets'), '--content', str(build / 'test-content')]
print('RUN', shlex.join(cmd), flush=True)
subprocess.run(cmd, cwd=root, check=True)
metrics = json.loads((out / 'metrics.json').read_text())
assert metrics['metrics'].get('test.finished') == 1, 'audit did not finish'
assert metrics['metrics'].get('test.fail', 0) == 0, metrics.get('warnings', [])
