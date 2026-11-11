"""Verify all copied files against the pinned source manifests."""
import hashlib
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
count = 0
for folder in ['third_party/audio_formats', 'system/services/audio/vendor/kiikoread']:
    path = root / folder
    for name, expected in re.findall(r'→ `([^`]+)`：SHA-256 `([0-9a-f]{64})`', (path / 'SOURCE.md').read_text()):
        assert hashlib.sha256((path / name).read_bytes()).hexdigest() == expected, name
        count += 1
assert count == 12, count
print(f'vendor hashes: {count} verified')
