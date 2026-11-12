"""Manual pixel comparison with the task baseline; never requires Git history in CI."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--before', type=Path)
a = p.parse_args()
build, out = a.build.resolve(), a.out.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-ui-scenes'
out.mkdir(parents=True, exist_ok=True)
summary = {}
before = json.loads((a.before / 'summary.json').read_text()) if a.before else None
for scene in ('Main', 'Apps', 'Settings', 'Control', 'AiListen', 'PowerMenu'):
    target = out / scene
    r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                        '--script', str(ROOT / 'tests/ui_shell.script'), '--hub', str(ROOT / 'tests/ui_shell.hub.json'),
                        '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--assets', str(build / 'assets'),
                        '--content', str(build / 'test-content'), '--out', str(target), '--quiet', '--no-random-outages'],
                       env={**os.environ, 'INK_UI_SCENE': scene}, capture_output=True, text=True, timeout=90)
    (out / (scene + '.log')).write_text(r.stdout + r.stderr)
    report = json.loads((target / 'metrics.json').read_text())
    m = report['metrics']
    assert r.returncode == 0 and m.get('test.finished') == 1 and not report.get('warnings'), r.stderr[-2500:]
    image = Image.open(target / 'snap-scene.png').convert('L')
    row = {'pixels_sha256': hashlib.sha256(image.tobytes()).hexdigest(),
           'psram_peak_bytes': m['mem.external.total_bytes'] - m['mem.external.min_free_bytes'],
           'assets_live_bytes': m.get('mem.tag.assets.external.live_bytes', 0),
           'assets_peak_bytes': m.get('mem.tag.assets.external.peak_bytes', 0)}
    if before:
        assert row['pixels_sha256'] == before[scene]['pixels_sha256'], f'{scene}: pixels differ'
        row['psram_peak_delta_bytes'] = row['psram_peak_bytes'] - before[scene]['psram_peak_bytes']
    summary[scene] = row
    print(scene, row, flush=True)
(out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
