"""Compile narrow counterexamples without editing product sources."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'storage-reverse'
area.mkdir(parents=True, exist_ok=True)
path = ROOT / 'system/services/file/file_service.c'
before = hashlib.sha256(path.read_bytes()).hexdigest()
source = path.read_text()
flags = (build / 'CMakeFiles/sim-storage-usage.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
cases = [
    ('member-scope', 'if (index_under(e.path, own)) bytes[0][k]', 'if (true) bytes[0][k]', 'owner category'),
    ('user-directory', 'int k = cache ? 1 : ext && !strcmp(ext, TMP_SUFFIX) ? 0 : ext && !strcmp(ext, PART_SUFFIX) ? 2 : -1;',
     'int k = cache ? 1 : (ext ? 0 : 0);', 'ordinary user files, suffix directories and backups survive cleanup'),
]
summary = []
for name, needle, replacement, expected in cases:
    assert source.count(needle) == 1, name
    changed = area / (name + '.c')
    changed.write_text(source.replace(needle, replacement))
    obj, binary = area / (name + '.o'), area / name
    link = shlex.split((build / 'CMakeFiles/sim-storage-usage.dir/link.txt').read_text())
    cmd = [link[0], *options, '-iquote', str(path.parent), '-DINK_STORAGE_SOURCE=' + json.dumps(str(changed)),
           '-c', str(ROOT / 'tests/storage_usage/boundary.c'), '-o', str(obj)]
    subprocess.run(cmd, cwd=build, check=True, capture_output=True)
    boundary = next(i for i, part in enumerate(link) if part.endswith('/boundary.c.o'))
    link[boundary] = str(obj)
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    result = subprocess.run([os.sys.executable, str(ROOT / 'tests/storage_usage/run.py'), '--build', str(build), '--binary', str(binary)],
                            cwd=ROOT, capture_output=True, text=True, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, timeout=180)
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode and expected in result.stdout + result.stderr, (name, result.stdout, result.stderr)
    metrics = json.loads((build / 'test-out' / ('storage-usage-' + name) / 'device/metrics.json').read_text())['metrics']
    assert metrics.get('test.finished') == 1 and metrics.get('test.fail', 0) > 0, name
    summary.append({'case': name, 'exit': result.returncode, 'failures': metrics['test.fail'], 'expected': expected})
    print(f'{name}: expected failure (exit={result.returncode}, failures={metrics["test.fail"]})', flush=True)
assert before == hashlib.sha256(path.read_bytes()).hexdigest()
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
