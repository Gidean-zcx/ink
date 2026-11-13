"""lock-1: the lock cover at the default and the largest text size, and a see-through copy of it that must fail.

Manual: --baseline OLD_LOCK_MARK_C runs the same scenes on an older lock source; with --compare DIR it also writes
side-by-side pictures (left before, right after) for the report. Neither is part of ctest (CI has one commit).
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline', type=Path)
p.add_argument('--compare', type=Path)
a = p.parse_args()
build = a.build.resolve()
out = build / 'test-out'


def simulate(binary, size, mode, area):
    shutil.rmtree(area, ignore_errors=True)
    area.mkdir(parents=True)
    r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(area),
        '--end', '900s', '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'),
        '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')],
        capture_output=True, text=True, timeout=240,
        env={**os.environ, 'INK_LOCK_COVER_MODE': mode, 'INK_LOCK_COVER_SIZE': str(size), 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (area / 'test.log').write_text(r.stdout + r.stderr)
    data = json.loads((area / 'metrics.json').read_text(errors='replace')) if (area / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    ok = r.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0) and not data.get('warnings')
    print(f'lock-cover[{mode or "checks"}, text size {size}]: exit={r.returncode}, pass={m.get("test.pass", 0)}, '
          f'fail={m.get("test.fail", 0)}, finished={m.get("test.finished", 0)}', flush=True)
    return ok, r.stderr + json.dumps(data.get('warnings', []), ensure_ascii=False), m


def clean(area, name):
    return Image.open(area / f'clean-{name}.png').convert('L')


def only_inside(area, before, after, m, probe):
    """Pixels that changed between two frames lie inside the object the driver measured."""
    box = ImageChops.difference(clean(area, before), clean(area, after)).getbbox()
    bounds = [m[f'test.{probe}.{edge}']['max'] for edge in ('x1', 'y1', 'x2', 'y2')]
    assert box and box[0] >= bounds[0] and box[1] >= bounds[1] and box[2] <= bounds[2] + 1 and box[3] <= bounds[3] + 1, \
        (before, after, box, bounds)


def driver(name, lock_source, defines):
    """The test driver compiled again against another lock source, linked like sim-lock-cover."""
    work = build / 'lock-cover-variants'
    work.mkdir(exist_ok=True)
    db = json.loads((build / 'compile_commands.json').read_text())
    unit = ROOT / 'tests/lock_cover.c'
    entry = next(item for item in db if Path(item['file']) == unit)
    cmd = shlex.split(entry['command'])
    obj = work / f'{name}.o'
    original = cmd[cmd.index('-o') + 1]
    cmd[cmd.index('-o') + 1] = str(obj)
    cmd += ['-I' + str(ROOT / 'system/ui'), '-DINK_TEST_LOCK_SOURCE=' + json.dumps(str(lock_source))] + defines
    subprocess.run(cmd, cwd=entry['directory'], check=True, capture_output=True, text=True)
    link = shlex.split((build / 'CMakeFiles/sim-lock-cover.dir/link.txt').read_text())
    link = [str(obj) if part == original or part.endswith('/tests/lock_cover.c.o') else part for part in link]
    binary = work / f'sim-lock-cover-{name}'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
    return binary


if a.baseline:
    binary = driver('baseline', a.baseline.resolve(), ['-DINK_LOCK_COVER_BASELINE'])
    for size in (1, 3):
        ok, why, _ = simulate(binary, size, 'baseline', out / f'lock-cover-baseline-{size}')
        assert ok, why[-4000:]
    if a.compare:
        from PIL import ImageDraw, ImageFont
        font = ImageFont.truetype(str(Path(os.environ['INK_DEPS']) / 'build-deps/fonts/NotoSansCJK-Regular.ttc'), 28, index=2)
        scenes = {'full': '有通知和下一件事', 'empty': '什么都没有', 'pin': '输密码页', 'child': '孩子的锁屏'}
        a.compare.mkdir(parents=True, exist_ok=True)
        for size, label in ((1, '默认字号'), (3, '最大字号')):
            for scene, title in scenes.items():
                pair = Image.new('L', (1384, 1264), 255)
                draw = ImageDraw.Draw(pair)
                for i, (side, folder) in enumerate((('改前', f'lock-cover-baseline-{size}'), ('改后', f'lock-cover-{size}'))):
                    draw.text((16 + i * 700, 4), f'{title} · {label} · {side}', font=font, fill=0)
                    with Image.open(out / folder / f'clean-{scene}.png') as frame:
                        assert frame.size == (684, 1216)
                        pair.paste(frame.convert('L'), (i * 700, 48))
                path = a.compare / f'{scene}-{size}.png'
                pair.save(path, optimize=True)
                assert path.stat().st_size < 300_000, path
        print('side-by-side pictures written to', a.compare, flush=True)
    raise SystemExit(0)

binary = build / 'sim-lock-cover'
for size in (1, 3):
    area = out / f'lock-cover-{size}'
    ok, why, m = simulate(binary, size, '', area)
    if not ok:
        raise SystemExit(why[-8000:])
    assert clean(area, 'full').tobytes() == clean(area, 'full-home').tobytes(), 'the cover over two pages must be one picture'
    only_inside(area, 'minute-before', 'minute-after', m, 'clock')
    only_inside(area, 'count-before', 'count-after', m, 'number')
    only_inside(area, 'grace', 'closed', m, 'next')
    print(f'lock-cover[text size {size}]: the same picture over two pages; a minute changes only the clock, a message only '
          'its number, the closed space only the next thing', flush=True)

# Reverse: the same driver on a copy whose cover lets the page show through must fail at the cover's own checks.
source = (ROOT / 'system/ui/lock_mark.c').read_text()
before = '    if (!clock) { /* the default, and a wallpaper not ready: never the page under it */\n'
assert source.count(before) == 1
seethrough = build / 'lock-cover-variants'
seethrough.mkdir(exist_ok=True)
(seethrough / 'lock_mark_seethrough.c').write_text(source.replace(before, before + '        lv_obj_set_style_bg_opa(g_mark, LV_OPA_TRANSP, 0);\n'))
binary = driver('reverse', seethrough / 'lock_mark_seethrough.c', [])
area = out / 'lock-cover-reverse'
ok, why, m = simulate(binary, 1, 'reverse', area)
assert not ok and m.get('test.finished') == 1 and 'the cover is opaque, fills the panel and is the top surface' in why, why[-4000:]
assert clean(area, 'full').tobytes() != clean(area, 'full-home').tobytes(), 'a see-through cover shows the page under it'
print('lock-cover[reverse]: a see-through cover fails the cover check and shows the page under it', flush=True)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    for area in [out / 'lock-cover-1', out / 'lock-cover-3', out / 'lock-cover-reverse', build / 'lock-cover-variants']:
        shutil.rmtree(area, ignore_errors=True)
