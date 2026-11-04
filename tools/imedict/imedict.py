#!/usr/bin/env python3
"""The input method's dictionary file (system services design §9.2): tab-separated `pinyin word frequency` lines in,
a table the device can search in place out. Little endian:

  header   "IKD1", u32 nkeys, u32 nentries, u32 keys_off, u32 entries_off, u32 text_off, u32 text_len, u32 crc32(rest)
  keys     u32 text offset of the key, u16 key length, u16 entries in it, u32 first entry   (by key, as bytes)
  entries  u32 text offset of the word, u16 word length, u16 frequency                      (by frequency, highest first)
  text     the keys and words, UTF-8, no terminators
"""
import struct
import sys
import zlib
from collections import defaultdict


def build(lines):
    by_key = defaultdict(list)
    for n, line in enumerate(lines, 1):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) != 3 or not parts[0].isascii() or not parts[0].islower():
            raise SystemExit(f"line {n}: expected: pinyin<TAB>word<TAB>frequency: {line!r}")
        by_key[parts[0]].append((parts[1], min(int(parts[2]), 65535)))
    text = bytearray()
    keys = bytearray()
    entries = bytearray()
    count = 0
    for key in sorted(by_key):
        off = len(text)
        text += key.encode()
        words = sorted(by_key[key], key=lambda w: -w[1])
        keys += struct.pack("<IHHI", off, len(key), len(words), count)
        for word, freq in words:
            woff = len(text)
            raw = word.encode()
            text += raw
            entries += struct.pack("<IHH", woff, len(raw), freq)
            count += 1
    head = 32
    rest = bytes(keys) + bytes(entries) + bytes(text)
    return b"IKD1" + struct.pack("<IIIIIII", len(by_key), count, head, head + len(keys), head + len(keys) + len(entries), len(text), zlib.crc32(rest)) + rest


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: imedict.py words.tsv out.ikd")
    with open(sys.argv[1], encoding="utf-8") as f:
        data = build(f)
    with open(sys.argv[2], "wb") as f:
        f.write(data)
