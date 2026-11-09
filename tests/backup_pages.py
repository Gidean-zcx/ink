"""Exercise real backup pages against an isolated synthetic peer and retain comparison frames."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SCENES = 'Backup BackupFail Restore RestoreRun RestoreDone RestoreFail'.split()
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-backup-pages'
area = build / 'test-out' / ('backup-pages-' + binary.name)
if area.exists():
    shutil.rmtree(area)
peer = area / 'hub'
peer.mkdir(parents=True)
out = area / 'device'
result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--script', str(ROOT / 'tests/backup_pages.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
    '--no-random-outages', '--quiet'], cwd=ROOT, capture_output=True, text=True, timeout=150,
    env={**os.environ, 'INK_SIM_BACKUP_DIR': str(peer), 'UBSAN_OPTIONS': 'halt_on_error=1'})
out.mkdir(parents=True, exist_ok=True)
(out / 'stdout.log').write_text(result.stdout)
(out / 'stderr.log').write_text(result.stderr)
data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
counts = data.get('metrics', {})
if result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or data.get('warnings'):
    raise SystemExit(f'FAIL exit={result.returncode}\n{result.stderr[-8000:]}\n{data.get("warnings", [])}')
for name in SCENES:
    assert counts.get(f'test.snapshot.{name}') == 1 and (out / f'snap-{name}.png').is_file(), name
    target = build / 'backup-compare' / name
    target.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(out / f'snap-{name}.png', target / 'snap-scene.png')
    shutil.copyfile(out / 'metrics.json', target / 'metrics.json')
print(f'PASS checks={counts.get("test.pass")} six verified frames, actual file backup and restore')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
