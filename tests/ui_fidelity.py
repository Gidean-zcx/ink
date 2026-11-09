"""Collect verified simulator frames and score every available design counterpart.

Run the existing suite with INK_KEEP_EVIDENCE=1 first. Missing boards are listed,
never rendered as placeholder pages. Original screenshot/metrics paths are kept.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--design', type=Path, default=os.environ.get('INK_DESIGN'), required=not os.environ.get('INK_DESIGN'))
p.add_argument('--out', type=Path, required=True)
p.add_argument('--variants-only', action='store_true', help='add the four pressure boards to retained measurements')
p.add_argument('--large-source', type=Path, help='verified maximum-font Settings frame, for baseline comparison')
a = p.parse_args()
build, out = a.build.resolve(), a.out.resolve()
boards, components = set(), []
for path in sorted((a.design / 'png').glob('*.png')):
    with Image.open(path) as image:
        if image.size == (684, 1216):
            boards.add(path.stem)
        else:
            components.append({'board': path.stem, 'size': list(image.size)})
sources = json.loads((out / 'sources.json').read_text()) if a.variants_only else {}
paths = [] if a.variants_only else sorted(build.rglob('metrics.json'), key=lambda path: (
    0 if '-compare' in str(path) else 1 if path.parent.name.startswith('ui-scene-') else 2, str(path)))
for path in paths:
    relative = path.relative_to(build)
    if (out.is_relative_to(path.parent) or path.is_relative_to(out)
            or any(word in str(relative) for word in ('ui-fidelity', 'mutant', 'reverse', 'baseline', 'big-', 'child-'))):
        continue
    data = json.loads(path.read_text())
    metrics = data.get('metrics', {})
    if (metrics.get('test.finished') != 1 or metrics.get('test.fail', 0)
            or not metrics.get('test.snapshot.settled') or data.get('warnings')):
        continue
    captured = {key.removeprefix('test.snapshot.') for key, value in metrics.items()
                if key.startswith('test.snapshot.') and key != 'test.snapshot.settled' and value == 1}
    for scene in sorted(boards & captured):
        frame = path.parent / f'snap-{scene}.png'
        if not frame.is_file():
            frame = path.parent / 'snap-scene.png'
            if (len(captured) != 1 and not ('-compare' in str(relative) and path.parent.name == scene)) or not frame.is_file():
                continue
        if scene in sources:
            continue
        target = out / scene
        target.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(frame, target / 'snap-scene.png')
        shutil.copyfile(path, target / 'source-metrics.json')
        # One already verified frame from a run may have captured several boards.
        proof = {'test.finished': metrics['test.finished'], 'test.fail': metrics.get('test.fail', 0),
                 'test.snapshot.settled': 1, f'test.snapshot.{scene}': metrics[f'test.snapshot.{scene}']}
        (target / 'metrics.json').write_text(json.dumps({'metrics': proof}) + '\n')
        sources[scene] = {'frame': str(frame.relative_to(ROOT)), 'metrics': str(path.relative_to(ROOT)),
                          'sha256': hashlib.sha256(frame.read_bytes()).hexdigest()}
out.mkdir(parents=True, exist_ok=True)
if sources and not a.variants_only:
    subprocess.run([sys.executable, str(ROOT / 'tools/ui_compare.py'), '--build', str(build),
                    '--design', str(a.design), '--out', str(out), '--reuse', '--scenes', *sorted(sources)], check=True)
scores = {s['scene']: s for s in json.loads((out / 'scores.json').read_text())}
for board, scene in (('StressFiles', 'FileList'), ('StressLarge', 'Settings'),
                     ('StressNotes', 'Notifications'), ('StressThread', 'SmsThread')):
    source = ((a.large_source.resolve() if a.large_source else build / 'test-out/ui-scene-big-Settings')
              if board == 'StressLarge' else out / scene)
    if not (source / 'snap-scene.png').exists():
        continue
    target = out / board
    frame = target / scene
    reference = target / 'reference/png'
    frame.mkdir(parents=True, exist_ok=True)
    reference.mkdir(parents=True, exist_ok=True)
    for name in ('metrics.json', 'snap-scene.png'):
        shutil.copyfile(source / name, frame / name)
    # Map a design variant to its actual page; never invent a snapshot counter.
    ref = reference / f'{scene}.png'
    ref.unlink(missing_ok=True)
    ref.symlink_to(a.design.resolve() / 'png' / f'{board}.png')
    subprocess.run([sys.executable, str(ROOT / 'tools/ui_compare.py'), '--design', str(reference.parent),
                    '--out', str(target), '--reuse', '--scenes', scene], check=True)
    score = json.loads((target / 'scores.json').read_text())[0]
    scores[board] = {**score, 'scene': board, 'source_scene': scene}
    shutil.copyfile(target / f'{scene}.png', out / f'{board}.png')
    original = source / ('source-metrics.json' if (source / 'source-metrics.json').exists() else 'metrics.json')
    shutil.copyfile(original, target / 'source-metrics.json')
    sources[board] = {'frame': str((source / 'snap-scene.png').relative_to(ROOT)),
                      'metrics': str(original.relative_to(ROOT)), 'source_scene': scene,
                      'sha256': hashlib.sha256((source / 'snap-scene.png').read_bytes()).hexdigest()}
(out / 'scores.json').write_text(json.dumps(list(scores.values()), indent=2) + '\n')
(out / 'scores.md').write_text('| 页面 | 1/4 灰度平均差 |\n| --- | ---: |\n' + ''.join(
    f'| {s["scene"]} | {s["mae_quarter"]:.3f}/255 |\n' for s in sorted(scores.values(), key=lambda s: s['scene'])))
(out / 'sources.json').write_text(json.dumps(sources, indent=2) + '\n')
missing = sorted(boards - sources.keys())
(out / 'unmatched.json').write_text(json.dumps(missing, indent=2) + '\n')
(out / 'components.json').write_text(json.dumps(components, indent=2) + '\n')
print(f'{len(sources)}/{len(boards)} boards measured; unmatched: {" ".join(missing)}')
