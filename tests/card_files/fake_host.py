#!/usr/bin/env python3
"""The simulator's install tool with faults (tests/card_files/run.py): card_files_serve.Server on stdin/stdout, one
answer line per device line. Every line both ways goes to --log."""
import argparse
import base64
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/device"))
import card_files_serve as cfs  # noqa: E402

ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
ap.add_argument("--bundle", required=True)
ap.add_argument("--log", required=True)
ap.add_argument("--corrupt-chunk", type=int, help="font chunk K (1-based): one byte wrong once, CRC of the right bytes")
ap.add_argument("--silent-chunk", type=int, help="font chunk K: no answer once")
ap.add_argument("--unplug-chunk", type=int, help="font chunk K: the cable is pulled instead of an answer")
ap.add_argument("--bad-file", action="store_true", help="serve a font with one byte changed (CRCs match it)")
ap.add_argument("--wrong-digest", action="store_true", help="a bundle of another build")
ap.add_argument("--data-on-hello", action="store_true", help="answer HELLO with a DATA line")
a = ap.parse_args()

server = cfs.Server(a.bundle)
if a.wrong_digest:
    server.digest = "0123456789abcdef"
if a.bad_file:  # bypass the server's own hash check: the bytes differ from the manifest in the middle
    data = bytearray(server.data[0])
    data[len(data) // 2] ^= 0x01
    server.data[0] = bytes(data)
used = set()


def once(kind, k, offset, idx):
    if k is None or idx != 0 or offset != (k - 1) * cfs.CHUNK or kind in used:
        return False
    used.add(kind)
    return True


with open(a.log, "a", buffering=1) as log:
    for line in sys.stdin:
        line = line.rstrip("\n")
        f = line.split(" ")
        reply = None
        if a.data_on_hello and len(f) > 1 and f[1] == "HELLO":
            reply = "INKCF1 DATA 0 0 4 " + "%08x" % zlib.crc32(b"evil") + " " + base64.b64encode(b"evil").decode()
        elif len(f) == 6 and f[1] == "GET":
            idx, offset = int(f[2]), int(f[4])
            if once("unplug", a.unplug_chunk, offset, idx):
                reply = "#UNPLUG"
            elif once("silent", a.silent_chunk, offset, idx):
                reply = "#SILENT"
            elif once("corrupt", a.corrupt_chunk, offset, idx):
                good = server.get(f[1:])
                parts = good.split(" ")
                raw = bytearray(base64.b64decode(parts[-1]))
                raw[0] ^= 0xFF
                reply = " ".join(parts[:-1] + [base64.b64encode(bytes(raw)).decode()])
        if reply is None:
            reply = server.handle(line)
        log.write(f"> {line}\n< {(reply or '#NONE')[:120]}\n")
        sys.stdout.write((reply or "#NONE") + "\n")
        sys.stdout.flush()
