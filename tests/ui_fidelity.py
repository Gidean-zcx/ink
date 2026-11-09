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
a = p.parse_args()
build, out = a.build.resolve(), a.out.resolve()
boards, components = set(), []
for path in sorted((a.design / 'png').glob('*.png')):
    with Image.open(path) as image:
        if image.size == (684, 1216):
            boards.add(path.stem)
        else:
            components.append({'board': path.stem, 'size': list(image.size)})
sources = {}
paths = sorted(build.rglob('metrics.json'), key=lambda path: (
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
(out / 'sources.json').write_text(json.dumps(sources, indent=2) + '\n')
missing = sorted(boards - sources.keys())
(out / 'unmatched.json').write_text(json.dumps(missing, indent=2) + '\n')
(out / 'components.json').write_text(json.dumps(components, indent=2) + '\n')
if sources:
    subprocess.run([sys.executable, str(ROOT / 'tools/ui_compare.py'), '--build', str(build),
                    '--design', str(a.design), '--out', str(out), '--reuse', '--scenes', *sorted(sources)], check=True)
print(f'{len(sources)}/{len(boards)} boards measured; unmatched: {" ".join(missing)}')
