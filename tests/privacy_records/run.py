"""Verify summary wiring, member isolation, confirmation and settled screenshots."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-privacy-records'
area = build / 'test-out' / ('privacy-records-' + binary.name)
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
script = area / 'case.script'
script.write_text('@0 net wifi\n@180s end\n')
result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--script', str(script), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(area),
    '--no-random-outages', '--quiet'], cwd=ROOT, capture_output=True, text=True,
    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'}, timeout=180)
(area / 'run.log').write_text(result.stdout + result.stderr)
data = json.loads((area / 'metrics.json').read_text()) if (area / 'metrics.json').exists() else {}
counts = data.get('metrics', {})
ok = result.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail', 0)
print('PASS' if ok else 'FAIL', 'exit', result.returncode, 'checks', counts.get('test.pass', 0),
      'failures', counts.get('test.fail', 0), flush=True)
if ok:
    for scene in ('Records', 'SetPrivacy', 'AiLog', 'AiMemory'):
        target = build / 'privacy-records-compare' / scene
        target.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(area / f'snap-{scene}.png', target / 'snap-scene.png')
        proof = {'test.snapshot.settled': counts[f'test.snapshot.{scene}'],
                 f'test.snapshot.{scene}': counts[f'test.snapshot.{scene}'],
                 'test.finished': 1, 'test.fail': 0}
        (target / 'metrics.json').write_text(json.dumps({'metrics': proof}) + '\n')
else:
    print(result.stderr[-8000:])
    print(data.get('warnings', []))
if ok and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
raise SystemExit(0 if ok else 1)
