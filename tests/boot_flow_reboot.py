"""Boot blank flash, power off at each setup step, then use the same flash on restart."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-boot-flow'
area = build / 'test-out' / ('boot-reboot-' + binary.name)
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
for phase in ('cold', 'wifi', 'password', 'failure', 'joining', 'offline', 'claim', 'confirm', 'restore', 'hello', 'complete', 'done', 'revoke', 'revoked'):
    out = area / phase
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/boot_flow.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages'],
        cwd=ROOT, capture_output=True, text=True, timeout=90,
        env={**os.environ, 'INK_BOOT_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    out.mkdir(parents=True, exist_ok=True)
    (out / 'stdout.log').write_text(result.stdout)
    (out / 'stderr.log').write_text(result.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    counts = data.get('metrics', {})
    if result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or data.get('warnings'):
        raise SystemExit(f'{phase}: FAIL exit={result.returncode}\n{result.stderr[-4000:]}\n{data.get("warnings", [])}')
    for path in out.rglob('*'):
        if path.is_file() and path.suffix in ('.json', '.log', '.txt', '.ndjson'):
            if any(secret in path.read_bytes() for secret in (b'test-secret-123', b'test-wrong-secret', b'WIFI:T:WPA')):
                raise SystemExit(f'{phase}: credential in {path.name}')
    print(f'{phase}: PASS checks={counts.get("test.pass")}', flush=True)
