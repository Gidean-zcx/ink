import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('asset_pack', Path(__file__).resolve().parents[3] / 'tools/asset_pack.py')
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


class AssetPackTest(unittest.TestCase):
    def test_order_split_and_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'icons').mkdir()
            (root / 'fonts').mkdir()
            (root / 'icons/z.inki').write_bytes(b'icon')
            (root / 'fonts/a.inkf').write_bytes(b'font')
            a, b = pack.split(root, 120, 120)
            self.assertEqual(a, pack.pack([('fonts/a.inkf', b'font')]))
            self.assertEqual(b, pack.pack([('icons/z.inki', b'icon')]))
            for data, value in ((a, b'font'), (b, b'icon')):
                self.assertEqual(struct.unpack_from('<4sII', data), (b'INKA', 1, 120))
                self.assertEqual(struct.unpack_from('<II', data, 108), (116, 4))
                self.assertEqual(data[116:], value)

    def test_overflow_reports_exact_excess(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'fonts').mkdir()
            (root / 'fonts/a.inkf').write_bytes(b'font')
            with self.assertRaisesRegex(ValueError, 'embedded: 120 bytes exceed 119 by 1 bytes'):
                pack.split(root, 119, 119)
            with self.assertRaisesRegex(ValueError, 'font_data: 12 bytes exceed 11 by 1 bytes'):
                pack.split(root, 11, 120)

    def test_no_assets_and_long_names_fail(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, 'no generated fonts'):
                pack.split(Path(tmp))
        with self.assertRaisesRegex(ValueError, 'name exceeds'):
            pack.pack([('x' * 96, b'x')])


if __name__ == '__main__':
    unittest.main()
