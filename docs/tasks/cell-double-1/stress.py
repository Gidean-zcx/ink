"""Bounded CPU-only load and the existing positive cell tests; no device or protocol changes."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--tag', required=True)
p.add_argument('--ctest', default='ctest')
p.add_argument('--workers', type=int, default=os.cpu_count() or 2)
p.add_argument('--repeat', type=int, default=30)
p.add_argument('--timeout', type=int, default=2100)
a = p.parse_args()
build = a.build.resolve()
area = build / ('cell-stress-' + a.tag)
area.mkdir(exist_ok=True)
loads = []
command = [a.ctest, '--test-dir', str(build), '-R', '^unit-s31-cell($|-(nat|baud|started|silent|dhcp|throttle|detect|privacy)$)',
           '--repeat', f'until-fail:{a.repeat}', '--output-on-failure', '-j2']
try:
    for _ in range(a.workers):
        worker = subprocess.Popen([sys.executable, '-u', '-c', 'print("ready", flush=True)\nwhile True: pass'], stdout=subprocess.PIPE, text=True)
        loads.append(worker)
        assert worker.stdout.readline().strip() == 'ready'
    print(f'CPU workers={len(loads)}; repeat={a.repeat}; ctest -j2', flush=True)
    with (area / 'ctest.log').open('w') as log:
        try:
            code = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=a.timeout).returncode
        except subprocess.TimeoutExpired:
            code = 124
finally:
    for worker in loads:
        worker.terminate()
    for worker in loads:
        worker.wait(timeout=10)
log = (area / 'ctest.log').read_text()
passes = len(re.findall(r'Test\s+#\d+:.*?\bPassed\b', log))
fails = len(re.findall(r'Test\s+#\d+:.*?(?:\*\*\*|\bFailed\b)', log))
summary = {'tag': a.tag, 'workers': a.workers, 'repeat': a.repeat, 'exit': code,
           'passes': passes, 'failures': fails, 'attempts': passes + fails}
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary), flush=True)
print(log[-2500:], flush=True)
raise SystemExit(code)
