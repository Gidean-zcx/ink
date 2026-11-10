#!/usr/bin/env python3
"""Read-only log retrieval, or offline decoding of an SD/raw flash export."""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib

SECTOR = 4096
SIZE = 512 * 1024
HEADER = struct.Struct("<5I")
MAGIC = 0x31474F4C
ROOT = Path(__file__).resolve().parents[2]


def records(data):
    found = []
    for off in range(0, min(len(data), SIZE), SECTOR):
        sector = data[off:off + SECTOR]
        if len(sector) < HEADER.size:
            continue
        magic, seq, boot, size, crc = HEADER.unpack_from(sector)
        if magic != MAGIC or not seq or size > SECTOR - HEADER.size or HEADER.size + size > len(sector):
            continue
        payload = sector[HEADER.size:HEADER.size + size]
        if zlib.crc32(payload, zlib.crc32(sector[:16])) == crc:
            found.append((seq, boot, payload))
    return sorted(found)


def decode(data):
    result, run, previous = [], bytearray(), None
    for seq, boot, payload in records(data):
        if previous is not None and seq != previous + 1:
            result.append(run.decode("utf-8", errors="replace"))
            run.clear()
            result.append(f"\n[日志缺口 seq={previous + 1}..{seq - 1}]\n")
        run.extend(payload)
        previous = seq
    result.append(run.decode("utf-8", errors="replace"))
    def wall(match):
        seconds = int(match[1])
        try:
            stamp = datetime.fromtimestamp(seconds, timezone.utc).isoformat()
        except (OverflowError, OSError, ValueError):
            return match[0]
        return f"utc={seconds} ({stamp})"
    return re.sub(r"utc=(\d+)", wall, "".join(result))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--port", help="Device already in download mode; read-flash only")
    source.add_argument("--logs", type=Path, help="Offline logs.bin from SD or a previous pull")
    parser.add_argument("--coredump", type=Path, help="Offline coredump.bin")
    parser.add_argument("--elf", type=Path, help="Exact ELF corresponding to the crashed firmware")
    parser.add_argument("--output", type=Path, default=Path("../ink-hw/logs"))
    args = parser.parse_args(argv)
    out = args.output.resolve()
    if out == ROOT or ROOT in out.parents:
        parser.error("输出目录必须在仓库外")
    out.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    dest = out / stamp
    dest.mkdir()
    if args.port:
        for name, address, size in (("logs", "0xf64000", "0x80000"), ("coredump", "0xfe4000", "0x10000")):
            subprocess.run([sys.executable, "-m", "esptool", "--port", args.port,
                            "--before", "no-reset", "--after", "no-reset", "read-flash",
                            address, size, str(dest / f"{name}.bin")], check=True)
    else:
        (dest / "logs.bin").write_bytes(args.logs.read_bytes())
        if args.coredump:
            (dest / "coredump.bin").write_bytes(args.coredump.read_bytes())
    data = (dest / "logs.bin").read_bytes()
    if len(data) != SIZE:
        parser.error(f"日志环应为 {SIZE} 字节，实际 {len(data)}；原文件已保留")
    (dest / "logs.txt").write_text(decode(data), encoding="utf-8")
    if args.elf:
        core = dest / "coredump.bin"
        if not core.exists():
            parser.error("--elf 需要同时提供核心转储")
        with (dest / "coredump.txt").open("w", encoding="utf-8") as report:
            subprocess.run([sys.executable, "-m", "esp_coredump", "info_corefile", "-t", "raw",
                            "-c", str(core), str(args.elf)], stdout=report, stderr=subprocess.STDOUT, check=True)
    print(f"有效记录 {len(records(data))}；输出：{dest}")


if __name__ == "__main__":
    main()
