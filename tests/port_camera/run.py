"""Check the real no-camera app and retain its bilingual screenshots on request."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
work = Path(tempfile.mkdtemp(prefix='port-camera-absent-', dir=build))
run = subprocess.run([str(build / 'sim-camera-absent'), '--profile', str(root / 'simulator/profiles/metalio-plus.json'),
    '--script', str(root / 'tests/port_camera/absent.script'), '--assets', str(build / 'assets'),
    '--content', str(build / 'test-content'), '--out', str(work), '--budget', str(root / 'tests/ui_scene.budget.json'),
    '--quiet', '--no-random-outages'], capture_output=True, text=True, timeout=150)
(work / 'stdout.log').write_text(run.stdout)
(work / 'stderr.log').write_text(run.stderr)
result = json.loads((work / 'metrics.json').read_text()) if (work / 'metrics.json').exists() else {}
m = result.get('metrics', {})
print('camera-absent:', 'exit', run.returncode, 'checks', m.get('test.pass', 0), 'failures', m.get('test.fail', 0))
assert not run.returncode and m.get('test.finished') == 1 and not m.get('test.fail') and not result.get('warnings'), run.stderr[-9000:]
for lang in ('zh', 'en'):
    assert (work / f'snap-camera-absent-{lang}.png').exists(), lang
if os.environ.get('INK_KEEP_EVIDENCE'):
    print('Evidence:', work.relative_to(root))
else:
    shutil.rmtree(work)
