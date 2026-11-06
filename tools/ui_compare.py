#!/usr/bin/env python3
"""Run the real shell scenes, pair screenshots, and measure quarter-size grayscale MAE."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from PIL import Image, ImageChops, ImageDraw, ImageStat

ROOT = Path(__file__).resolve().parents[1]
SCENES = 'Main Today Apps Lock LockPage Control Notifications Members MembersBack Pin PinWrong ImePinyin ImeCandidates ImeEdit ImePassword ImeVoice Confirm ConfirmDelete ConfirmSecret AiListen AiThink Ai Settings SetDisplay SetSound SetNetwork PowerMenu PowerOff PhoneIncoming ClockRing'.split()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build', type=Path, default=ROOT / 'build')
    ap.add_argument('--design', type=Path, default=os.environ.get('INK_DESIGN'))
    ap.add_argument('--out', type=Path, default=ROOT / 'out/ui-compare')
    ap.add_argument('--scenes', nargs='*', default=SCENES)
    ap.add_argument('--reuse', action='store_true')
    a = ap.parse_args()
    if not a.design: ap.error('set INK_DESIGN or --design')
    a.out.mkdir(parents=True, exist_ok=True)
    scores = []
    for scene in a.scenes:
        target = a.out / scene
        if not a.reuse:
            (target / 'snap-scene.png').unlink(missing_ok=True)
            env = dict(os.environ, INK_UI_SCENE=scene)
            result = subprocess.run([str(a.build / 'sim-ui-scenes'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                '--script', str(ROOT / 'tests/ui_shell.script'), '--hub', str(ROOT / 'tests/ui_shell.hub.json'), '--assets', str(a.build / 'assets'),
                '--content', str(a.build / 'test-content'), '--out', str(target), '--no-random-outages', '--quiet'],
                env=env, capture_output=True, text=True)
            if result.returncode: raise SystemExit(f'{scene}: simulator failed\n{result.stdout}\n{result.stderr}')
        rendered = target / 'snap-scene.png'
        metrics = json.loads((target / 'metrics.json').read_text())
        counters = metrics.get('metrics', {})
        if (not rendered.exists() or counters.get('test.snapshot.settled', 0) != 1 or counters.get(f'test.snapshot.{scene}', 0) != 1
                or counters.get('test.finished', 0) != 1 or counters.get('test.fail', 0) != 0):
            raise SystemExit(f'{scene}: missing a verified settled scene frame; rerun without --reuse')
        reference = a.design / 'png' / f'{scene}.png'
        actual = Image.open(rendered).convert('L')
        design = Image.open(reference).convert('L')
        if actual.size != design.size: raise SystemExit(f'{scene}: size mismatch {actual.size}, {design.size}')
        small = tuple(n // 4 for n in design.size)
        diff = ImageChops.difference(actual.resize(small, Image.Resampling.LANCZOS), design.resize(small, Image.Resampling.LANCZOS))
        score = ImageStat.Stat(diff).mean[0]
        canvas = Image.new('RGB', (design.width * 3, design.height + 48), 'white')
        canvas.paste(design, (0, 48)); canvas.paste(actual, (design.width, 48))
        canvas.paste(ImageChops.difference(design, actual), (design.width * 2, 48))
        ImageDraw.Draw(canvas).text((16, 12), f'{scene} | design / product / absolute difference | quarter MAE {score:.3f}/255', fill='black')
        canvas.save(a.out / f'{scene}.png')
        scores.append({'scene': scene, 'mae_quarter': round(score, 3), 'target': 12, 'within_target': score <= 12})
        print(f'{scene}: {score:.3f}/255')
    (a.out / 'scores.json').write_text(json.dumps(scores, ensure_ascii=False, indent=2) + '\n')
    (a.out / 'scores.md').write_text('| 页面 | 1/4 灰度平均差 | 目标 |\n| --- | ---: | --- |\n' +
        ''.join(f'| {s["scene"]} | {s["mae_quarter"]:.3f}/255 | {"达到" if s["within_target"] else "超过 12"} |\n' for s in scores))


if __name__ == '__main__': main()
