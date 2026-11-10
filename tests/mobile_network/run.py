"""Mobile network acceptance; only synthetic credentials, frames and an isolated flash image."""
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
binary = (a.binary or build / 'sim-mobile-network').resolve()
area = build / 'test-out' / ('mobile-network' if not a.binary else 'mobile-network-' + binary.name)
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
for phase in ('main', 'restart', 'cleared', 'no-modem'):
    out = area / phase
    profile = ROOT / 'simulator/profiles/metalio-plus.json'
    if phase == 'no-modem':
        data = json.loads(profile.read_text())
        data['network'].pop('lte')  # Keep the board's modem declaration, like the S31 with its transport absent.
        profile = area / 'no-modem.json'
        profile.write_text(json.dumps(data))
    result = subprocess.run([str(binary), '--profile', str(profile),
        '--script', str(ROOT / 'tests/mobile_network/mobile_network.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages'],
        cwd=ROOT, capture_output=True, text=True, timeout=240,
        env={**os.environ, 'INK_MOBILE_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (area / (phase + '.log')).write_text(result.stdout + result.stderr)
    metrics = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    counts = metrics.get('metrics', {})
    if result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or metrics.get('warnings'):
        raise SystemExit(f'{phase}: FAIL exit={result.returncode}\n{result.stderr[-6500:]}\n{metrics.get("warnings", [])}')
    for file in out.rglob('*'):
        if file.suffix in ('.json', '.log'):
            assert 'SYNTH_APN_SECRET_42' not in file.read_text(errors='replace'), f'password leaked to {file.name}'
    if phase == 'main':
        for name in ('mobile-zh', 'mobile-en', 'apn-zh', 'apn-en'):
            assert (out / f'snap-{name}.png').exists(), name
        log = (area / 'user.img.logs').read_bytes()
        assert b'SYNTH_APN_SECRET_42' not in log, 'password leaked to the persistent log'
        for capability in ('net.apn_set', 'net.roaming_set', 'net.usage', 'net.usage_reset'):
            assert ('cap call ' + capability + ' ').encode() in log, capability
    if phase == 'no-modem':
        for language in ('zh', 'en'):
            assert (out / f'snap-mobile-no-modem-{language}.png').exists(), language
    print(f'{phase}: PASS checks={counts.get("test.pass", 0)}', flush=True)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
