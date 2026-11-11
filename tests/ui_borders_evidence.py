"""Collect border-task screenshots and six comparisons; no Git history is needed."""
import argparse
import json
from pathlib import Path
import shutil
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--before', type=Path, required=True)
a = p.parse_args()
build, before = a.build.resolve(), a.before.resolve()
area = build / 'ui-borders-evidence'
area.mkdir(exist_ok=True)
paths = set(build.glob('test-out/ui-scene-*/snap-*.png'))
paths.update(build.glob('test-out/ui-borders/*/snap-*.png'))
for pattern in ('*-compare', '*-evidence', 'test-out/*compare*', 'test-out/settings-*', 'test-out/phone-calls',
                'test-out/sound-clock-*', 'test-out/clock-alarm-*', 'test-out/mobile-network', 'test-out/bluetooth',
                'test-out/weather*', 'kid-apps-*', 'lock-camera-*', 'test-out/ui-file-picker', 'test-out/ui-cards'):
    for directory in build.glob(pattern):
        paths.update(directory.rglob('snap-*.png'))
verified = []
for path in sorted(paths):
    metrics = path.parent / 'metrics.json'
    if not metrics.exists():
        continue
    counts = json.loads(metrics.read_text()).get('metrics', {})
    if counts.get('test.finished') == 1 and not counts.get('test.fail', 0) and not counts.get('panic.count', 0):
        verified.append(path)
paths = verified
lines = ['# UI 边框截图索引', '', '每张截图来自已稳定呈现的宿主机模拟器画面；缩略图按序排列。', '',
         '| 序号 | 截图路径（相对仓库） | 缩略图 |', '| --- | --- | --- |']
for start in range(0, len(paths), 16):
    sheet = Image.new('RGB', (684, 1328), '#eeeeee')
    draw = ImageDraw.Draw(sheet)
    sheet_name = f'sheet-{start // 16 + 1:02d}.png'
    for j, path in enumerate(paths[start:start + 16]):
        x, y = j % 4 * 171, j // 4 * 332
        with Image.open(path) as source:
            sheet.paste(source.convert('RGB').resize((171, 304)), (x, y + 28))
        label = path.parent.name if path.stem == 'snap-scene' else path.stem.removeprefix('snap-')
        draw.text((x + 2, y + 2), f'{start + j + 1} {label}'[:27], fill='black')
        lines.append(f'| {start + j + 1} | `{path.relative_to(ROOT)}` | `{sheet_name}` |')
    sheet.save(area / sheet_name)
for folder in ('before', 'after', 'comparisons'):
    (area / folder).mkdir(exist_ok=True)
for scene in ('Main', 'Apps', 'Control', 'SetDisplay', 'Confirm', 'Members'):
    old, new = before / f'{scene}.png', build / f'test-out/ui-scene-{scene}/snap-scene.png'
    assert old.exists() and new.exists(), scene
    shutil.copyfile(old, area / f'before/{scene}.png')
    shutil.copyfile(new, area / f'after/{scene}.png')
    comparison = Image.new('RGB', (1368, 1248), 'white')
    draw = ImageDraw.Draw(comparison)
    draw.text((10, 8), f'{scene}: before (gray)', fill='black')
    draw.text((694, 8), f'{scene}: after (frame)', fill='black')
    with Image.open(old) as source:
        comparison.paste(source.convert('RGB'), (0, 32))
    with Image.open(new) as source:
        comparison.paste(source.convert('RGB'), (684, 32))
    comparison.save(area / f'comparisons/{scene}.png')
(area / 'index.md').write_text('\n'.join(lines) + '\n')
print(f'{len(paths)} screenshots, {(len(paths) + 15) // 16} contact sheets, 6 comparisons: {area.relative_to(ROOT)}')
