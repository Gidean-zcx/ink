"""Check every shipping dictionary word against the actual packaged glyph index."""
import argparse
import json
from pathlib import Path
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--dictionary", type=Path)
args = parser.parse_args()
dictionary = args.dictionary or args.build / "test-content/.system/ime/base.ikd"
data = dictionary.read_bytes()
font = (args.build / "assets/fonts/sans-44.inkf").read_bytes()
assert data[:4] == b"IKD1" and font[:4] == b"INKF"
count, index = struct.unpack_from("<II", font, 16)
glyphs = {struct.unpack_from("<I", font, index + i * 16)[0] for i in range(count)
          if font[index + i * 16 + 8] and font[index + i * 16 + 9]}
entries, entries_at, text_at = (struct.unpack_from("<I", data, at)[0] for at in (8, 16, 20))
words = set()
for i in range(entries):
    offset, length = struct.unpack_from("<IH", data, entries_at + i * 8)
    words.add(data[text_at + offset:text_at + offset + length].decode("utf-8"))
missing = sorted(word for word in words if any(ord(char) not in glyphs for char in word))
print(json.dumps({"words": len(words), "entries": entries, "missing_words": len(missing),
                  "examples": missing[:10]}, ensure_ascii=False))
assert not missing, f"{len(missing)} dictionary words contain missing glyphs"
