"""Run enrollment UI with synthetic services; retain every frame and check logs for secrets."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SCENES = 'FirstBoot BootWifi BootWifiPass BootJoining BootJoinFail BootClaim BootNoHub BootConfirm BootRestore BootHello BootRevoked'.split()
p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
for mode in ('fresh', 'restore'):
    out = build / 'test-out' / f'boot-flow-{mode}'
    peer = build / 'test-out' / f'boot-flow-{mode}-hub'
    if out.exists(): shutil.rmtree(out)
    if peer.exists(): shutil.rmtree(peer)
    peer.mkdir(parents=True)
    cmd = [str(build / 'sim-boot-flow'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
           '--script', str(ROOT / 'tests/boot_flow.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
           '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out), '--quiet', '--no-random-outages']
    run = subprocess.run(cmd, cwd=ROOT, env={**os.environ, 'INK_BOOT_MODE': mode, 'INK_SIM_BACKUP_DIR': str(peer),
        'UBSAN_OPTIONS': 'halt_on_error=1'}, capture_output=True, text=True, timeout=90)
    out.mkdir(parents=True, exist_ok=True)
    (out / 'stdout.log').write_text(run.stdout)
    (out / 'stderr.log').write_text(run.stderr)
    if not (out / 'metrics.json').exists():
        raise SystemExit(f'{mode}: simulator exited {run.returncode} before metrics\n{run.stderr}')
    metrics = json.loads((out / 'metrics.json').read_text())
    counts = metrics.get('metrics', {})
    if run.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or metrics.get('warnings'):
        raise SystemExit(f'{mode}: FAIL exit={run.returncode}\n{run.stderr[-6000:]}\n{metrics.get("warnings", [])}')
    for file in out.rglob('*'):
        if file.is_file() and file.suffix in ('.json', '.log', '.txt', '.ndjson'):
            data = file.read_bytes()
            for secret in (b'test-secret-123', b'test-wrong-secret', b'test-only-pending-token', b'WIFI:T:WPA'):
                if secret in data: raise SystemExit(f'credential appeared in {file.name}')
    for scene in SCENES:
        if counts.get(f'test.snapshot.{scene}') != 1 or not (out / f'snap-{scene}.png').is_file():
            raise SystemExit(f'{mode}: missing verified frame for {scene}')
        if mode == 'fresh':
            target = build / 'boot-compare' / scene
            target.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(out / f'snap-{scene}.png', target / 'snap-scene.png')
            shutil.copyfile(out / 'metrics.json', target / 'metrics.json')
    print(f'{mode}: PASS checks={counts.get("test.pass")} all 11 frames verified, logs credential-free', flush=True)
