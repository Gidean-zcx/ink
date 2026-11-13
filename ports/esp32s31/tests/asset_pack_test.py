import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

spec = importlib.util.spec_from_file_location('asset_pack', Path(__file__).resolve().parents[3] / 'tools/asset_pack.py')
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


class AssetPackTest(unittest.TestCase):
    def test_compressed_entries_roundtrip_and_first_pack_stays_unchanged(self):
        data = b'INKI' + b'icon pixels' * 1000
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'icons').mkdir()
            (root / 'icons/mic-line-106.inki').write_bytes(data)
            first, embedded = pack.split(root, 12)
            self.assertEqual(first, pack.pack([]))
            key, off, stored = pack.ENTRY.unpack_from(embedded, 12)
            self.assertEqual(key.rstrip(b'\0'), b'icons/mic-line-106.inki')
            self.assertTrue(stored & 0x80000000)
            self.assertEqual(off + (stored & 0x7fffffff), len(embedded))
            self.assertEqual(struct.unpack_from('<I', embedded, off)[0], len(data))
            self.assertEqual(zlib.decompress(embedded[off + 4:]), data)
            self.assertLess(len(embedded), len(data) // 2)
            # A file in the first partition keeps the installation format and bytes.
            first, _ = pack.split(root)
            self.assertEqual(first, pack.pack([('icons/mic-line-106.inki', data)]))

    def test_overflowing_font_is_stored_uncompressed(self):
        # A font stays mapped for as long as the device runs: inflated, it would hold PSRAM for good.
        font, icon = b'INKF' + b'glyph rows' * 1000, b'INKI' + b'icon pixels' * 1000
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'fonts').mkdir()
            (root / 'icons').mkdir()
            (root / 'fonts/ui-2-53.inkf').write_bytes(font)
            (root / 'icons/mic-line-106.inki').write_bytes(icon)
            first, embedded = pack.split(root, 12)
            self.assertEqual(first, pack.pack([]))
            key, off, stored = pack.ENTRY.unpack_from(embedded, 12)
            self.assertEqual(key.rstrip(b'\0'), b'fonts/ui-2-53.inkf')
            self.assertEqual(stored, len(font))
            self.assertEqual(embedded[off:off + stored], font)
            key, off, stored = pack.ENTRY.unpack_from(embedded, 12 + pack.ENTRY.size)
            self.assertEqual(key.rstrip(b'\0'), b'icons/mic-line-106.inki')
            self.assertTrue(stored & 0x80000000)
            self.assertEqual(zlib.decompress(embedded[off + 4:off + (stored & 0x7fffffff)]), icon)

    def test_state_icon_filter_is_the_shipping_filter(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'icons').mkdir()
            for name in ('mic-line-106', 'mic-solid-106', 'plus-line-106', 'plus-solid-106', 'plus-line-43'):
                (root / f'icons/{name}.inki').write_bytes(b'icon')
            first, embedded = pack.split(root)
            self.assertEqual(embedded, pack.pack([]))
            self.assertNotIn(b'plus-line-106', first)
            self.assertNotIn(b'plus-solid-106', first)
            for name in ('mic-line-106', 'mic-solid-106', 'plus-line-43'):
                self.assertIn(name.encode(), first)

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

    def test_original_sample_is_packed(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'sample').mkdir()
            sample = Path(__file__).resolve().parents[3] / 'assets/sample/welcome.txt'
            (root / 'sample/welcome.txt').write_bytes(sample.read_bytes())
            a, _ = pack.split(root)
            self.assertEqual(a, pack.pack([('sample/welcome.txt', sample.read_bytes())]))
            self.assertIn(b'CC0 1.0', a)

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
