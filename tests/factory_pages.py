"""Verify destructive reset only against isolated synthetic flash, card and backup peer."""
import argparse
import hashlib
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
binary = a.binary.resolve() if a.binary else build / 'sim-factory-pages'
area = build / 'test-out' / ('factory-pages-' + binary.name)
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)


def files(root):
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.rglob('*') if path.is_file()}


for mode in ('keep', 'clear'):
    chain = area / mode
    peer = chain / 'hub'
    peer.mkdir(parents=True)
    before = None
    for phase in (mode, 'after-' + mode):
        out = chain / phase
        result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
            '--script', str(ROOT / 'tests/backup_pages.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
            '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
            '--flash-image', str(chain / 'user.img'), '--sd-dir', str(chain / 'sd'), '--quiet', '--no-random-outages'],
            cwd=ROOT, capture_output=True, text=True, timeout=150,
            env={**os.environ, 'INK_FACTORY_PHASE': phase, 'INK_SIM_BACKUP_DIR': str(peer), 'UBSAN_OPTIONS': 'halt_on_error=1'})
        out.mkdir(parents=True, exist_ok=True)
        (out / 'stdout.log').write_text(result.stdout)
        (out / 'stderr.log').write_text(result.stderr)
        data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
        counts = data.get('metrics', {})
        if result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or data.get('warnings'):
            raise SystemExit(f'{phase}: FAIL exit={result.returncode}\n{result.stderr[-9000:]}\n{data.get("warnings", [])}')
        card = chain / 'sd/content'
        if not phase.startswith('after-'):
            assert counts.get('power.reboot') == 1, 'correct password must reboot'
            before = {folder: files(card / folder) for folder in ('books', 'audio', 'other')}
            remote = files(peer)
            assert remote, 'backup must actually write the synthetic peer'
        else:
            for folder in ('books', 'audio'):
                assert files(card / folder) == ({} if mode == 'clear' else before[folder]), (mode, folder)
            assert files(card / 'other') == before['other'], 'unselected shared content must remain byte identical'
            assert files(peer) == remote, 'factory reset must preserve every remote backup byte'
        name = 'FactoryDone' if phase.startswith('after-') else 'FactoryReset'
        assert counts.get(f'test.snapshot.{name}') == 1, name
        snap = json.loads((out / f'{name}.json').read_text())
        expected = {'shell.back', 'factory.media', 'backup.now', 'system.factory_reset'} if name == 'FactoryReset' else {'factory.finish'}
        assert {action['capability'] for action in snap['actions']} == expected, 'every visible action is exposed to AI'
        assert snap and (out / f'snap-{name}.png').is_file(), name
        target = build / 'factory-compare' / name
        target.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(out / f'snap-{name}.png', target / 'snap-scene.png')
        shutil.copyfile(out / 'metrics.json', target / 'metrics.json')
        print(f'{phase}: PASS checks={counts.get("test.pass")}, real flash/card, snapshot and screenshot', flush=True)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
