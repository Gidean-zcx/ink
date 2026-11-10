import contextlib
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import shlex
import struct
import tempfile
import unittest
from unittest.mock import patch
import zlib

spec = importlib.util.spec_from_file_location('flash_plan', Path(__file__).resolve().parents[3] / 'tools/device/flash_plan.py')
plan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(plan)


class FlashPlanTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        plan.ROOT = self.root
        self.image = bytearray(b'\xff' * (16 * 1024 * 1024))
        for i, (name, (address, size)) in enumerate(plan.LAYOUT.items()):
            struct.pack_into('<HBBII16sI', self.image, 0x8000 + i * 32, 0x50aa, 1, i, address, size, name.encode(), 0)
        self.backup = self.root / 'fake-full.bin'
        self.backup.write_bytes(self.image)
        build = self.root / 'build-s31'
        build.mkdir()
        (build / 'ink-s31.bin').write_bytes(b'fake-application')
        (build / 'ink-fonts.bin').write_bytes(b'fake-fonts')
        self.env = {'INK_FLASH_BACKUP': str(self.backup), 'INK_FLASH_BACKUP_SHA256': hashlib.sha256(self.image).hexdigest()}

    def run_plan(self, mode):
        result = io.StringIO()
        with patch.dict(os.environ, self.env), contextlib.redirect_stdout(result):
            plan.plan(mode)
        return result.getvalue()

    def test_trial_writes_only_allowed_ranges_and_verifies_before_select(self):
        text = self.run_plan('trial')
        lines = [shlex.split(line) for line in text.splitlines() if 'write-flash' in line or 'verify-flash' in line]
        self.assertEqual([line[-2] for line in lines], ['0x580000'] * 2 + ['0xae4000'] * 2 + ['0xd000'] * 2)
        self.assertEqual([line[-3] for line in lines], ['write-flash', 'verify-flash'] * 3)
        ota = (self.root / 'build-s31/trial-parts/otadata.bin').read_bytes()
        self.assertEqual(len(ota), 8192)
        self.assertEqual(struct.unpack_from('<I', ota)[0], 2)
        self.assertEqual(struct.unpack_from('<I', ota, 28)[0], zlib.crc32(ota[:4], 0xffffffff))

    def test_restore_uses_exact_backup_segments_and_selects_factory_last(self):
        text = self.run_plan('restore')
        self.assertTrue(text.rstrip().endswith('otadata.bin'))
        for name in ('model', 'font_data', 'otadata'):
            off, size = plan.LAYOUT[name]
            self.assertEqual((self.root / f'build-s31/restore-parts/{name}.bin').read_bytes(), self.image[off:off + size])

    def test_refuses_unverified_backup_wrong_layout_and_oversize(self):
        self.env['INK_FLASH_BACKUP_SHA256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'SHA256 mismatch'):
            self.run_plan('trial')
        self.image[0x8008] ^= 1
        self.backup.write_bytes(self.image)
        self.env['INK_FLASH_BACKUP_SHA256'] = hashlib.sha256(self.image).hexdigest()
        with self.assertRaisesRegex(ValueError, 'partition table differs'):
            self.run_plan('restore')

    def test_refuses_app_and_font_overflow(self):
        for filename, size in (('ink-s31.bin', 5242881), ('ink-fonts.bin', 4718593)):
            path = self.root / 'build-s31' / filename
            old = path.read_bytes()
            path.write_bytes(b'x' * size)
            with self.assertRaisesRegex(ValueError, 'excess 1'):
                self.run_plan('trial')
            path.write_bytes(old)


if __name__ == '__main__':
    unittest.main()
