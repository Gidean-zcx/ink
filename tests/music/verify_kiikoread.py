"""The kiikoread functions recorded as copied unchanged in third_party/kiikoread_music/SOURCE.md still are.

With --source <pinned kiikoread checkout> it also checks them against the original files (manual: the pinned copy
lives outside the repository)."""
import argparse
import hashlib
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
HERE = ROOT / 'third_party/kiikoread_music'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path)
a = p.parse_args()


def function(text, name):
    m = re.search(r'^(?:static |const )?[\w\s\*]*?\b' + re.escape(name) + r'\([^;{]*\)\s*\{', text, re.M)
    assert m, name
    end = text.index('\n}\n', m.start()) + 3 if '\n}\n' in text[m.start():] else None
    one = text.index('\n', m.start())
    if text[m.start():one].rstrip().endswith('}'):
        end = one + 1
    return text[m.start():end]


rows = re.findall(r'^\| `([^`]+)` \| `(\w+)` \| `([0-9a-f]{64})` \|$', (HERE / 'SOURCE.md').read_text(encoding='utf-8'), re.M)
assert len(rows) >= 30, len(rows)
for upstream, name, digest in rows:
    copy = HERE / ('app_music.c' if upstream.endswith('app_music.c') else 'music_library.c')
    got = hashlib.sha256(function(copy.read_text(encoding='utf-8'), name).encode()).hexdigest()
    assert got == digest, f'{name} in {copy.name} changed'
    if a.source:
        original = hashlib.sha256(function((a.source / upstream).read_text(encoding='utf-8'), name).encode()).hexdigest()
        assert original == digest, f'{name} differs from {upstream}'
print(f'kiikoread functions copied unchanged: {len(rows)} verified' + (' against the pinned source' if a.source else ''))
