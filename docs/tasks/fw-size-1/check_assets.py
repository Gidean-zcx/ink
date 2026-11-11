#!/usr/bin/env python3
"""Check the retained size/name contract; --packer supports a manual reverse check."""
import argparse
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--packer', type=Path, default=ROOT / 'tools/asset_pack.py')
args, remaining = parser.parse_known_args()
spec = importlib.util.spec_from_file_location('asset_pack', args.packer)
packer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packer)


def unpack(blob):
    magic, count, total = struct.unpack_from('<4sII', blob)
    assert magic == b'INKA' and total == len(blob)
    result = {}
    for i in range(count):
        name, offset, size = struct.unpack_from('<96sII', blob, 12 + i * 104)
        result[name.split(b'\0')[0].decode()] = blob[offset:offset + size]
    return result


class IconSizeContract(unittest.TestCase):
    def test_only_unused_state_variants_are_removed(self):
        names = json.loads((ROOT / 'assets/icons/manifest.json').read_text())['names']
        # Direct calls, ternaries, and the weather/remote finite name sets.
        state = set('battery check clock cloud cloudoff cloudsun download family globe lock mic '
                    'person rain sd search snow sun undo usb warn wave wifioff'.split())
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'icons').mkdir()
            expected = {}
            for name in names:
                for look in ('line', 'solid'):
                    for size in (34, 43, 53, 77, 106):
                        key = f'icons/{name}-{look}-{size}.inki'
                        data = key.encode()
                        (root / key).write_bytes(data)
                        if size != 106 or name in state:
                            expected[key] = data
            actual = {}
            for blob in packer.split(root):
                actual.update(unpack(blob))
            self.assertEqual(set(actual) - set(expected), set(), 'unused 106 px variants remain')
            self.assertEqual(set(expected) - set(actual), set(), 'a required variant is missing')
            self.assertEqual(actual, expected)

    def test_fonts_ime_sample_and_unknown_icon_names_are_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            expected = {'fonts/a.inkf': b'font', 'ime/words': b'words',
                        'sample/welcome.txt': b'sample', 'icons/z.inki': b'icon'}
            for key, data in expected.items():
                path = root / key
                path.parent.mkdir(exist_ok=True)
                path.write_bytes(data)
            actual = {}
            for blob in packer.split(root):
                actual.update(unpack(blob))
            self.assertEqual(actual, expected)


if __name__ == '__main__':
    unittest.main(argv=[__file__, *remaining])
