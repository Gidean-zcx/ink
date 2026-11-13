"""lock-styles-1: Mogo's lock styles on the real lock screen, case by case, with local books and the card font."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--case', action='append')
p.add_argument('--expect-fail', action='store_true', help='reverse controls: the case must fail')
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-lock-styles'
spec = importlib.util.spec_from_file_location('mogo_page', ROOT / 'tests/graft_mogo_page/run.py')
page = importlib.util.module_from_spec(spec)
spec.loader.exec_module(page)


def content(kind, case):
    full = page.content(build)  # the Mogo page test's books and card font
    root = build / ('lock-styles-content-' + case + ('-' + binary.name if a.expect_fail else ''))
    if root.exists():
        shutil.rmtree(root)
    member = root / '.m/owner'
    member.mkdir(parents=True)
    if kind != 'nofont':
        (member / 'fonts').mkdir()
        shutil.copyfile(full / '.m/owner/fonts/InkSC.ttf', member / 'fonts/InkSC.ttf')
        (root / 'fonts').mkdir()  # the family copy for members without their own (the child)
        shutil.copyfile(full / '.m/owner/fonts/InkSC.ttf', root / 'fonts/InkSC.ttf')
    if kind == 'full':
        shutil.copytree(full / '.m/owner/books', member / 'books')
        shutil.copyfile(ROOT / 'docs/tasks/replan-reader/reference-images/crossink-reading.jpg', root / 'SYNTH_WALL.jpg')
    return root


cases = a.case or ['styles', 'almanac', 'refresh', 'privacy', 'entry', 'upgrade', 'empty', 'nofont']
for case in cases:
    kind = 'empty' if case == 'empty' else 'nofont' if case == 'nofont' else 'full'
    root = content(kind, case)
    area = build / 'test-out' / ('lock-styles-' + binary.name + '-' + case)
    shutil.rmtree(area, ignore_errors=True)
    area.mkdir(parents=True)
    profile = ROOT / 'simulator/profiles/metalio-plus.json'
    r = subprocess.run([str(binary), '--profile', str(profile),
        '--assets', str(build / 'assets'), '--content', str(root), '--out', str(area),
        '--end', '1800s', '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'),
        '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')],
        capture_output=True, text=True, timeout=590,
        env={**os.environ, 'INK_LOCK_STYLES_CASE': case, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (area / 'test.log').write_text(r.stdout + r.stderr)
    data = json.loads((area / 'metrics.json').read_text(errors='replace')) if (area / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    ok = r.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and not data.get('warnings')
    print(f'lock-styles[{case}]: exit={r.returncode}, pass={m.get("test.pass", 0)}, fail={m.get("test.fail", 0)}, '
          f'finished={m.get("test.finished", 0)}', flush=True)
    if a.expect_fail:
        if ok:
            raise SystemExit(f'reverse control: case {case} passed although the product was changed')
        print(f'lock-styles[{case}]: failed as the reverse control requires', flush=True)
        continue
    if not ok:
        print(r.stderr[-12000:], data.get('warnings', []))
        raise SystemExit(1)
    if case == 'refresh':
        from PIL import Image, ImageChops
        before, after = [Image.open(area / f'clean-minute-{n}.png').convert('L') for n in ('before', 'after')]
        box = ImageChops.difference(before, after).getbbox()
        # the clock's canvas area (160,288)-(544,960), as drawn.c places the canvas under the 108 px bar, scaled 1108/1216
        x0, y0, num, den = (684 - 684 * 1108 // 1216) // 2, 108, 1108, 1216
        bound = (x0 + 160 * num // den - 3, y0 + 288 * num // den - 3, x0 + 544 * num // den + 3, y0 + 960 * num // den + 3)
        assert box and box[0] >= bound[0] and box[1] >= bound[1] and box[2] <= bound[2] and box[3] <= bound[3], (box, bound)
        print(f'lock-styles[refresh]: the minute changes pixels only inside the clock area {box}', flush=True)
    if case == 'styles':  # the collage drawn a second time comes from its cache: still the whole frame
        from PIL import Image
        for size in (1, 3):
            low = Image.open(area / f'clean-style-7-size-{size}.png').convert('L').crop((40, 800, 640, 1150))
            assert low.getextrema()[0] < 128, f'collage at size {size} is blank below the middle (a half-cached frame)'
        print('lock-styles[styles]: the collage is whole when drawn and when read back from its cache', flush=True)
    keep = os.environ.get('INK_KEEP_EVIDENCE')
    if keep:
        target = Path(keep) / case
        target.mkdir(parents=True, exist_ok=True)
        for png in area.glob('clean-*.png'):
            shutil.copyfile(png, target / png.name)
    shutil.rmtree(area)
