"""Isolate storage changes in existing failing tests; manual control, no source edits."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out/storage-regression-baseline'
area.mkdir(parents=True, exist_ok=True)
source = ROOT / 'system/services/storage/storage_service.c'
copy = area / 'storage.c'
copy.write_text(subprocess.check_output(['git', 'show', '6171c2bf8d59238c95f2d548663f8b2ab3c6f43a:system/services/storage/storage_service.c'], cwd=ROOT, text=True))
entry = next(e for e in json.loads((build / 'compile_commands.json').read_text()) if Path(e['file']) == source)
obj = area / 'storage.o'
command = shlex.split(entry['command'])
command[command.index('-o') + 1] = str(obj)
command[command.index(str(source))] = str(copy)
command += ['-I' + str(source.parent)]
subprocess.run(command, cwd=entry['directory'], check=True)
results = []
for target in ('sim-races', 'sim-sms-multipart', 'sim-games2'):
    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    binary = area / (target + '-old-storage')
    link[link.index('-o') + 1] = str(binary)
    link.insert(link.index('libink_system.a'), str(obj))
    subprocess.run(link, cwd=build, check=True)
    if target == 'sim-races':
        out = area / 'races'
        command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                   '--budget', str(ROOT / 'tests/integration/races.budget.json'), '--script', str(ROOT / 'tests/integration/races.script'),
                   '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out), '--quiet', '--no-random-outages']
    else:
        folder = 'sms_multipart' if target == 'sim-sms-multipart' else 'games2'
        command = [sys.executable, str(ROOT / f'tests/{folder}/run.py'), '--build', str(build), '--binary', str(binary)]
        if target == 'sim-games2':
            command += ['--phases', 'main']
    r = subprocess.run(command, cwd=ROOT, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, capture_output=True, text=True, timeout=600)
    (area / (target + '.log')).write_text(r.stdout + r.stderr)
    results.append({'target': target, 'exit': r.returncode})
    print(f'baseline storage {target}: exit={r.returncode}', flush=True)
    if r.returncode:
        print(r.stdout[-3000:] + r.stderr[-3000:], flush=True)
(area / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
raise SystemExit(0 if all(r['exit'] == 0 for r in results) else 1)
