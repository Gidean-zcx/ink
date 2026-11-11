"""Lock settings, event and renderer coverage with the real setting definitions."""
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
binary = a.binary.resolve() if a.binary else build / 'sim-lock-faces'
selected = os.environ.get('INK_LOCK_FACES_CASE')
for case in ([selected] if selected is not None else ['picker', 'tick', '']):
    area = build / 'test-out' / ('lock-faces-' + binary.name + ('-' + case if case else ''))
    shutil.rmtree(area, ignore_errors=True)
    area.mkdir(parents=True)
    r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(area),
        '--end', '900s', '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'),
        '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')],
        capture_output=True, text=True, timeout=180,
        env={**os.environ, 'INK_LOCK_FACES_CASE': case, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (area / 'test.log').write_text(r.stdout + r.stderr)
    data = json.loads((area / 'metrics.json').read_text()) if (area / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    ok = r.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and not data.get('warnings')
    print(f'lock-faces{("[" + case + "]") if case else ""}: exit={r.returncode}, pass={m.get("test.pass", 0)}, fail={m.get("test.fail", 0)}, finished={m.get("test.finished", 0)}', flush=True)
    if not ok:
        print(r.stderr[-10000:], data.get('warnings', []))
        raise SystemExit(1)
    if case == 'tick':
        from PIL import Image, ImageChops
        before, after, same = [Image.open(area / f'clean-minute-{name}.png').convert('L') for name in ('before', 'after', 'same')]
        box = ImageChops.difference(before, after).getbbox()
        # Probe samples are reported alongside counters in the metrics object.
        bounds = [m['test.minute.' + edge]['max'] for edge in ('x1', 'y1', 'x2', 'y2')]
        assert box and box[0] >= bounds[0] and box[1] >= bounds[1] and box[2] <= bounds[2] + 1 and box[3] <= bounds[3] + 1, (box, bounds)
        assert ImageChops.difference(after, same).getbbox() is None, 'same-minute pixels changed'
        print('lock-faces[tick]: pixels change only inside clock bounds; same minute unchanged', flush=True)
    if not os.environ.get('INK_KEEP_EVIDENCE'):
        shutil.rmtree(area)
