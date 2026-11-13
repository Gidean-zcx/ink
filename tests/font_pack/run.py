"""Compressed fonts (font-pack-1).

glyphs:   every glyph of every shipped face unpacks to the bitmap fontgen rendered (test-font-pack); with
          --expect-fail the same comparison must find differences (the negative control reads the compressed
          bytes as bitmaps).
overflow: the UI's medium and bold faces with almanac-2's new characters and 50 more GB2312 characters the UI does
          not use yet: every font stays in font_data, with at least 1 MiB left; uncompressed they would not fit;
          and a face that does not fit no longer pushes the ones after it into the app image.
"""
import argparse
import importlib.util
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NEW_IN_ALMANAC_2 = '宜忌民俗老黄北南'


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def glyphs(a):
    build = a.build.resolve()
    shipped = sorted((build / 'assets/fonts').glob('*.inkf'))
    raw = build / 'assets-raw/fonts'
    assert len(shipped) >= 9, shipped
    pairs = []
    for face in shipped:
        assert face.read_bytes()[4:6] == struct.pack('<H', 2), f'{face.name} is not compressed'
        assert (raw / face.name).is_file(), f'no uncompressed copy of {face.name}'
        pairs += [str(face), str(raw / face.name)]
    binary = a.binary.resolve() if a.binary else build / 'test-font-pack'
    r = subprocess.run([str(binary), *pairs], capture_output=True, text=True, timeout=600)
    print(r.stdout + r.stderr, end='')
    if a.expect_fail:
        assert r.returncode == 1 and 'differ' in r.stderr, 'reading compressed glyphs as bitmaps must not pass'
        print('negative control: the comparison fails as it must')
    else:
        assert r.returncode == 0 and r.stdout.count('glyphs identical') == len(shipped), r.returncode


def names(pack, data):
    return [name for name, _ in pack.entries(data)]


def overflow(a):
    build = a.build.resolve()
    pack = load('asset_pack', ROOT / 'tools/asset_pack.py')
    fontgen = load('fontgen', ROOT / 'tools/fontgen/fontgen.py')
    used = set((build / 'assets/fonts/ui-charset.txt').read_text(encoding='utf-8'))
    level1 = [chr(cp) for cp in fontgen.gb2312() if 0x4e00 <= cp <= 0x9fff][:3755]  # GB2312 level 1, pinyin order
    fifty = [c for c in level1 if c not in used and c not in NEW_IN_ALMANAC_2][:50]
    extra = NEW_IN_ALMANAC_2 + ''.join(fifty)
    added = len(set(extra) - used)
    assert len(fifty) == 50 and added >= 50, (len(fifty), added)
    work = Path(tempfile.mkdtemp(prefix='font-pack-', dir=build))
    try:
        assets, raw = work / 'assets', work / 'raw'
        subprocess.run([sys.executable, str(ROOT / 'tools/fontgen/ui_fonts.py'), '--regular', a.regular, '--face', str(a.face),
                        '--medium', a.medium, '--bold', a.bold, '--out', str(assets / 'fonts'), '--raw-out', str(raw / 'fonts'),
                        '--extra', extra], check=True, capture_output=True, timeout=600)
        shutil.copyfile(build / 'assets/fonts/sans-44.inkf', assets / 'fonts/sans-44.inkf')
        shutil.copyfile(build / 'assets-raw/fonts/sans-44.inkf', raw / 'fonts/sans-44.inkf')
        for item in ('icons', 'ime', 'sample'):
            for root in (assets, raw):
                (root / item).symlink_to(build / 'assets' / item)
        for face in ('ui-1-36.inkf', 'ui-2-53.inkf'):  # the characters really are in the faces now
            before, after = (struct.unpack_from('<I', (d / face).read_bytes(), 16)[0] for d in (build / 'assets/fonts', assets / 'fonts'))
            assert after == before + added, (face, before, after, added)
        fonts = sorted('fonts/' + p.name for p in (assets / 'fonts').glob('*.inkf'))

        first, second = pack.split(assets)
        left = pack.FONT_LIMIT - len(first)
        print(f'{added} new characters ({extra}): font_data {len(first)}/{pack.FONT_LIMIT}, {left} bytes left; '
              f'app image pack {len(second)} bytes')
        assert names(pack, first) == fonts and not pack.spilled(second), names(pack, second)
        assert left >= 1024 * 1024, f'only {left} bytes left in font_data'

        plain = sum(p.stat().st_size for p in (raw / 'fonts').glob('*.inkf')) + pack.HEADER.size + pack.ENTRY.size * len(fonts)
        print(f'uncompressed the same faces need {plain} bytes')
        assert plain > pack.FONT_LIMIT, 'the test must be one that did not fit before'

        # Tight on purpose: sans-44 (in the middle of the order) does not fit, the faces after it do.
        sizes = {name: (assets / name).stat().st_size for name in fonts}
        tight = len(first) - pack.ENTRY.size - sizes['fonts/sans-44.inkf'] // 2
        first, second = pack.split(assets, tight, 64 * 1024 * 1024)
        print(f'font_data of {tight} bytes: ' + pack.spilled(second))
        assert names(pack, first) == [n for n in fonts if n != 'fonts/sans-44.inkf'], names(pack, first)
        assert pack.spilled(second) == f'fonts that did not fit in font_data, now in the app image: fonts/sans-44.inkf {sizes["fonts/sans-44.inkf"]} bytes'
    finally:
        shutil.rmtree(work)


p = argparse.ArgumentParser(description=__doc__)
p.add_argument('mode', choices=['glyphs', 'overflow'])
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--expect-fail', action='store_true')
p.add_argument('--regular')
p.add_argument('--face', type=int, default=2)
p.add_argument('--medium')
p.add_argument('--bold')
a = p.parse_args()
glyphs(a) if a.mode == 'glyphs' else overflow(a)
