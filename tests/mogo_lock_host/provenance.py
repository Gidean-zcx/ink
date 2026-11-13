"""third_party/mogo-lock against its manifest: every pinned file as Mogo had it, unless SOURCE.md and the manifest
say which line changed and why. Also: no second copy of Mogo's calendar data (almanac-2 owns it)."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
vendor = ROOT / 'third_party/mogo-lock'
manifest = json.loads((vendor / 'upstream-files.json').read_text())
assert manifest['commit'] == '864f2f9dc75980a67d2d6b67b17365020b7a50ce'
listed = set()
for entry in manifest['files']:
    data = (vendor / entry['path']).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    listed.add(entry['path'])
    if 'adapted_sha256' in entry:
        assert digest == entry['adapted_sha256'] and digest != entry['sha256'] and entry.get('adapted'), entry['path']
        assert entry['path'] in (vendor / 'SOURCE.md').read_text(), f"SOURCE.md does not explain {entry['path']}"
    else:
        assert digest == entry['sha256'], f"unrecorded change: {entry['path']}"
present = {str(p.relative_to(vendor)) for p in vendor.rglob('*') if p.is_file()} - {'SOURCE.md', 'upstream-files.json'}
assert present == listed, (present - listed, listed - present)
for name in ('calendar.c', 'calendar_data.h'):
    assert not list(vendor.rglob(name)), f'second copy of {name}: use system/services/time/vendor/mogo-calendar'
for required in ('LICENSE', 'licenses/Apache-2.0.txt', 'licenses/kiikoread-Personal-Study-1.0.txt', 'main/assets/clock-font/OFL.txt',
                 'tools/vendor/libre-bodoni/OFL.txt'):
    assert required in listed, required
print(f'mogo-lock provenance: {len(listed)} pinned files, '
      f"{sum('adapted_sha256' in e for e in manifest['files'])} documented adaptation(s)")
