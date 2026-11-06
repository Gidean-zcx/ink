"""The comparison tool must refuse stale final images and unverified snapshots."""
import json
from pathlib import Path
import runpy
import sys
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
MAIN = runpy.run_path(str(ROOT / 'tools/ui_compare.py'))['main']


class CompareTest(unittest.TestCase):
    def test_reuse_requires_a_verified_scene_frame(self):
        (ROOT / 'work').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'work') as tmp:
            root = Path(tmp)
            reference = root / 'design/png'
            reference.mkdir(parents=True)
            output = root / 'out/Apps'
            output.mkdir(parents=True)
            Image.new('L', (8, 8), 255).save(reference / 'Apps.png')
            Image.new('L', (8, 8), 255).save(output / 'final.png')
            args = ['ui_compare', '--reuse', '--scenes', 'Apps', '--design', str(root / 'design'), '--out', str(root / 'out')]
            verified = {'test.snapshot.settled': 1, 'test.snapshot.Apps': 1, 'test.finished': 1}
            for metrics, snapshot in (({}, False), ({'test.snapshot.settled': 1, 'test.snapshot.Apps': 1}, False),
                                      ({'test.snapshot.Today': 1, 'test.snapshot.settled': 1}, True),
                                      ({'test.snapshot.Apps': 1}, True), ({**verified, 'test.fail': 1}, True)):
                (output / 'metrics.json').write_text(json.dumps({'metrics': metrics}))
                if snapshot: Image.new('L', (8, 8), 255).save(output / 'snap-scene.png')
                with self.subTest(metrics=metrics, snapshot=snapshot), patch.object(sys, 'argv', args):
                    with self.assertRaisesRegex(SystemExit, 'verified settled scene frame'): MAIN()
            (output / 'metrics.json').write_text(json.dumps({'metrics': verified}))
            with patch.object(sys, 'argv', args): MAIN()
            score = json.loads((root / 'out/scores.json').read_text())[0]
            self.assertEqual(score['mae_quarter'], 0)


if __name__ == '__main__': unittest.main()
