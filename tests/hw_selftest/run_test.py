#!/usr/bin/env python3
"""tools/hw-selftest/run.py without a device: the otadata it makes (ESP-IDF format), the layout checks,
the write guard and a full --dry-run against a made-up 16 MiB backup (only 0xd000 and 0x580000 appear)."""
import hashlib
import importlib.util
import io
import os
import re
import struct
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
RUN = os.path.join(ROOT, "tools", "hw-selftest", "run.py")
spec = importlib.util.spec_from_file_location("hw_selftest_run", RUN)
run = importlib.util.module_from_spec(spec)
spec.loader.exec_module(run)

MiB = 1024 * 1024
FACTORY_TABLE = [  # the device's table (docs/hardware/2026-11-03-phase0.md section 4)
    ("nvs", 1, 0x02, 0x9000, 0x4000), ("otadata", 1, 0x00, 0xD000, 0x2000), ("phy_init", 1, 0x01, 0xF000, 0x1000),
    ("model", 1, 0x82, 0x10000, 0x64000), ("ota_0", 0, 0x10, 0x80000, 0x500000), ("ota_1", 0, 0x11, 0x580000, 0x500000),
    ("resources", 1, 0x82, 0xA80000, 0x64000), ("font_data", 1, 0x40, 0xAE4000, 0x500000),
    ("coredump", 1, 0x03, 0xFE4000, 0x10000)]


def read(path):
    with open(path, "rb") as f:
        return f.read()


def crc32_le_rom(crc, data):
    """Bit by bit, as esp_rom_crc32_le: ~crc in, reflected poly 0xEDB88320, ~crc out (independent of binascii)."""
    crc ^= 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return crc ^ 0xFFFFFFFF


def table_bin(rows, md5=True):
    out = b""
    for label, ptype, sub, off, size in rows:
        out += b"\xaa\x50" + struct.pack("<BBII", ptype, sub, off, size) + label.encode().ljust(16, b"\0") + b"\0" * 4
    if md5:
        out += b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(out).digest()
    return out.ljust(0xC00, b"\xff")


def entry(seq, state=2, crc=None):
    """One esp_ota_select_entry_t: ota_seq, seq_label[20], ota_state, crc."""
    crc = crc32_le_rom(0xFFFFFFFF, struct.pack("<I", seq)) if crc is None else crc
    return struct.pack("<I20sII", seq, b"\xff" * 20, state, crc)


def otadata(first=None, second=None):
    data = bytearray(b"\xff" * 0x2000)
    if first:
        data[0:32] = first
    if second:
        data[0x1000:0x1020] = second
    return bytes(data)


def backup(path, rows=FACTORY_TABLE, ota=None, size=16 * MiB, md5=True):
    data = bytearray(b"\xff" * size)
    t = table_bin(rows, md5)
    data[0x8000:0x8000 + len(t)] = t
    ota = otadata(entry(1)) if ota is None else ota  # the factory state: seq 1 in sector 0 (ota_0)
    data[0xD000:0xF000] = ota
    with open(path, "wb") as f:
        f.write(data)


class Otadata(unittest.TestCase):
    def test_crc_matches_the_rom_algorithm(self):
        for seq in (0, 1, 2, 3, 0x7FFFFFFF, 0xFFFFFFFE):
            self.assertEqual(run.seq_crc(seq), crc32_le_rom(0xFFFFFFFF, struct.pack("<I", seq)))

    def test_seq_plus_one_in_the_other_sector(self):
        factory = otadata(entry(1))
        self.assertEqual(run.boot_slot(factory), 0)
        made = run.select_ota1(factory)
        self.assertEqual(len(made), 0x2000)
        self.assertEqual(made[:0x1000], factory[:0x1000], "the factory entry stays as it was")
        seq, label, state, crc = struct.unpack_from("<I20sII", made, 0x1000)
        self.assertEqual((seq, label, state), (2, b"\xff" * 20, 2))
        self.assertEqual(crc, crc32_le_rom(0xFFFFFFFF, struct.pack("<I", 2)))
        self.assertEqual(made[0x1020:], b"\xff" * (0x2000 - 0x1020))
        self.assertEqual(run.boot_slot(made), 1)

    def test_factory_entry_in_sector_one(self):
        made = run.select_ota1(otadata(entry(1), entry(3)))  # seq 3 is the newest: (3-1)%2 = ota_0
        self.assertEqual(struct.unpack_from("<I", made, 0)[0], 4)
        self.assertEqual(made[0x1000:0x1020], entry(3))
        self.assertEqual(run.boot_slot(made), 1)

    def test_stale_bytes_in_the_other_sector_are_cleared(self):
        factory = bytearray(otadata(entry(1), entry(7, crc=0xDEAD)))
        factory[0x1100:0x1108] = b"leftover"
        made = run.select_ota1(bytes(factory))
        self.assertEqual(struct.unpack_from("<I", made, 0x1000)[0], 2)
        self.assertEqual(made[0x1020:], b"\xff" * (0x2000 - 0x1020))

    def test_refuses_what_it_does_not_expect(self):
        for bad in (otadata(), otadata(entry(1, crc=0x1234)), otadata(entry(2)), otadata(entry(1, state=3)),
                    otadata(entry(1))[:0x1000]):
            with self.assertRaises(run.Stop):
                run.select_ota1(bad)


class Layout(unittest.TestCase):
    def test_factory_table_passes(self):
        run.check_layout(run.parse_partitions(table_bin(FACTORY_TABLE)))

    def test_moved_or_extra_partitions_stop(self):
        moved = [r if r[0] != "ota_1" else ("ota_1", 0, 0x11, 0x600000, 0x500000) for r in FACTORY_TABLE]
        small_otadata = [r if r[0] != "otadata" else ("otadata", 1, 0x00, 0xD000, 0x1000) for r in FACTORY_TABLE]
        third_app = FACTORY_TABLE + [("ota_2", 0, 0x12, 0x1000000 - 0x10000, 0x10000)]
        for rows in (moved, small_otadata, third_app):
            with self.assertRaises(run.Stop):
                run.check_layout(run.parse_partitions(table_bin(rows)))

    def test_bad_md5_stops(self):
        t = bytearray(table_bin(FACTORY_TABLE))
        t[40] ^= 1
        with self.assertRaises(run.Stop):
            run.parse_partitions(bytes(t))


class Guard(unittest.TestCase):
    def test_only_otadata_and_ota1(self):
        run.check_write(0xD000, 0x2000)
        run.check_write(0x580000, 0x500000)
        run.check_write(0x580000, 952208)
        for addr, size in ((0x0, 0x1000), (0x8000, 0x1000), (0x9000, 0x1000), (0x80000, 0x1000), (0xD000, 0x2001),
                           (0x580000, 0x500001), (0xC000, 0x2000), (0x581001, 0x10), (0xA80000, 0x1000)):
            with self.assertRaises(run.Stop, msg=hex(addr)):
                run.check_write(addr, size)


class DryRun(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = self.tmp.name
        self.backup = os.path.join(self.dir, "backup.bin")
        self.image = os.path.join(self.dir, "image.bin")
        with open(self.image, "wb") as f:
            f.write(b"\xe9" + b"\x5a" * (300 * 1024 - 1))

    def tearDown(self):
        self.tmp.cleanup()

    def dry(self, out="out"):
        args = ["--dry-run", "--port", "/dev/cu.fake", "--backup", self.backup, "--image", self.image,
                "--out", os.path.join(self.dir, out)]
        buf = io.StringIO()
        with redirect_stdout(buf):
            rc = run.main(args)
        return rc, buf.getvalue()

    def test_dry_run_prints_only_the_two_addresses(self):
        backup(self.backup)
        rc, text = self.dry()
        self.assertEqual(rc, 0, text[-600:])
        self.assertEqual(set(re.findall(r"0x[0-9a-fA-F]+", text)), {"0xd000", "0x580000"}, text[-600:])
        cmds = [ln for ln in text.splitlines() if ln.startswith("run: ")]
        self.assertEqual(len(cmds), 4, text[-600:])  # write + verify, restore write + verify
        for c in cmds:
            self.assertNotIn("erase", c)
            self.assertRegex(c, r" (write-flash|verify-flash) ")
        out = os.path.join(self.dir, "out")
        made = read(os.path.join(out, "otadata_ota1.bin"))
        self.assertEqual(run.boot_slot(made), 1)
        self.assertEqual(os.path.getsize(os.path.join(out, "ota_1_original.bin")), 0x500000)
        self.assertEqual(os.path.getsize(os.path.join(out, "ota_1_restore.bin")), 300 * 1024)
        self.assertEqual(read(os.path.join(out, "otadata_factory.bin")), otadata(entry(1)))

    def test_dry_run_as_a_command(self):
        backup(self.backup)
        p = subprocess.run([sys.executable, RUN, "--dry-run", "--port", "/dev/cu.fake", "--backup", self.backup,
                            "--image", self.image, "--out", os.path.join(self.dir, "cli")], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stdout[-600:] + p.stderr[-600:])
        self.assertIn("DRY RUN done", p.stdout)

    def test_bad_backups_stop_before_anything(self):
        cases = {"small": dict(size=8 * MiB), "no_md5_bad_layout": dict(md5=False, rows=FACTORY_TABLE[:5]),
                 "boots_ota1": dict(ota=otadata(entry(2)))}
        for name, kw in cases.items():
            backup(self.backup, **kw)
            rc, text = self.dry(name)
            self.assertEqual(rc, 2, name + text[-300:])
            self.assertNotIn("run: ", text, name)

    def test_image_that_is_not_an_app_stops(self):
        backup(self.backup)
        with open(self.image, "wb") as f:
            f.write(b"\x00" * 1024)
        rc, text = self.dry()
        self.assertEqual(rc, 2, text[-300:])


if __name__ == "__main__":
    unittest.main()
