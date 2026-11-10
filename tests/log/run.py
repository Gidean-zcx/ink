"""Run the real logger/file model, then independently decode and test the read-only CLI."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("pull", ROOT / "tools/device/pull_logs.py")
pull = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pull)


def run(binary):
    with tempfile.TemporaryDirectory(prefix="ink-log-test-") as d:
        root = Path(d) / "device"
        root.mkdir()
        subprocess.run([binary, str(root)], check=True)
        data = Path(str(root) + ".logs").read_bytes()
        decoded = pull.decode(data)
        assert "问题 #300001" in decoded and "PRIVATE_TEST_SECRET" not in decoded
        assert len(pull.records(data)) <= 128
        subprocess.run([sys.executable, str(ROOT / "tools/device/pull_logs.py"), "--logs",
                        str(root) + ".logs", "--output", str(Path(d) / "decoded")], check=True)
        exported, = (Path(d) / "decoded").glob("*/logs.txt")
        assert exported.read_text() == decoded
        sd_log, = root.glob("content/ink-logs/*/logs.bin")
        assert "问题 #300001" in pull.decode(sd_log.read_bytes())
        sd_core, = root.glob("content/ink-logs/*/coredump.bin")
        assert sd_core.stat().st_size == 65536
        # Decode ignores a torn payload and rejects tampering with sequence/boot metadata too.
        import struct
        import zlib
        payload = "中文跨扇区".encode()
        def record(seq, content):
            head = struct.pack("<4I", pull.MAGIC, seq, 1, len(content))
            crc = zlib.crc32(content, zlib.crc32(head))
            return (head + struct.pack("<I", crc) + content).ljust(4096, b"\xff")
        pair = record(1, payload[:5]) + record(2, payload[5:])
        assert "中文跨扇区" in pull.decode(pair)
        corrupted = bytearray(pair)
        corrupted[4] ^= 1
        assert len(pull.records(corrupted)) == 1
        assert len(pull.records(pair[:4096 + 22])) == 1
        assert "write-flash" not in (ROOT / "tools/device/pull_logs.py").read_text()
        print("PASS: independent CRC decode, UTF-8 boundaries, CLI round-trip, raw SD export")


if __name__ == "__main__":
    run(sys.argv[1])
