#!/usr/bin/env python3
"""Build an .inkfw upgrade package (system services design §12.2).

Little endian:
  header (4 KiB at most): magic "INKFW1\\0\\0", u32 format=1, u32 header_size, char product[32], char version[24],
    u32 version_code, u32 min_from_code, u32 flags (1: allow downgrade), u32 nsections,
    sections[n] { u32 kind (0 app, 1 font, 2 resources), u32 reserved, u64 offset, u64 size, u8 sha256[32] },
    u32 key_id, u8 signature[64]: over sha256 of the header, signature zeroed
  section data

The signature here is the simulator's stand-in (a keyed hash, the key is not secret). The real one is ECDSA P-256, made
off-line with a key that is not in this repository.

usage: make_inkfw.py OUT --version 2.0.0 --code 200 [--product P] [--min-from N] [--allow-downgrade]
                         [--app-size N] [--resources N] [--font N] [--corrupt-section] [--corrupt-signature]
"""
import argparse
import hashlib
import hmac
import struct

KEY = b"ink-simulator-test-signing-key"
PRODUCT = "metalio-e-ink4-plus"


def pad(s, n):
    return s.encode().ljust(n, b"\0")


def build(version, code, product=PRODUCT, min_from=0, flags=0, app_size=64 * 1024, resources=0, font=0, app_head=None):
    head = (app_head or f"SIMAPP {code} {version}\n").encode()
    app = head + bytes((i * 7) & 0xff for i in range(max(0, app_size - len(head))))
    sections = [(0, app)]
    if font:
        sections.append((1, bytes((i * 11) & 0xff for i in range(font))))
    if resources:
        sections.append((2, bytes((i * 13) & 0xff for i in range(resources))))
    header_size = 8 + 4 + 4 + 32 + 24 + 4 + 4 + 4 + 4 + len(sections) * (4 + 4 + 8 + 8 + 32) + 4 + 64
    off = header_size
    table = b""
    for kind, data in sections:
        table += struct.pack("<IIQQ", kind, 0, off, len(data)) + hashlib.sha256(data).digest()
        off += len(data)
    h = b"INKFW1\0\0" + struct.pack("<II", 1, header_size) + pad(product, 32) + pad(version, 24)
    h += struct.pack("<IIII", code, min_from, flags, len(sections)) + table + struct.pack("<I", 1)
    digest = hashlib.sha256(h + b"\0" * 64).digest()
    sig = hmac.new(KEY, digest, hashlib.sha256).digest().ljust(64, b"\0")
    return bytearray(h + sig + b"".join(d for _, d in sections)), header_size


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--version", required=True)
    ap.add_argument("--code", type=int, required=True)
    ap.add_argument("--product", default=PRODUCT)
    ap.add_argument("--min-from", type=int, default=0)
    ap.add_argument("--allow-downgrade", action="store_true")
    ap.add_argument("--app-size", type=int, default=64 * 1024)
    ap.add_argument("--resources", type=int, default=0)
    ap.add_argument("--font", type=int, default=0)
    ap.add_argument("--corrupt-section", action="store_true")
    ap.add_argument("--corrupt-signature", action="store_true")
    ap.add_argument("--corrupt-header", action="store_true")
    a = ap.parse_args()
    data, hs = build(a.version, a.code, a.product, a.min_from, 1 if a.allow_downgrade else 0, a.app_size, a.resources, a.font)
    if a.corrupt_section:
        data[hs + 100] ^= 0xff
    if a.corrupt_signature:
        data[hs - 64 + 2] ^= 0xff
    if a.corrupt_header:
        data[8 + 4 + 4 + 32 + 2] ^= 0x01  # a character of the version: the signature no longer covers it
    with open(a.out, "wb") as f:
        f.write(data)
