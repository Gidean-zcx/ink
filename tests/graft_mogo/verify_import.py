"""Check the pinned vendor snapshot without requiring Git history or external checkouts."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[2]
vendor = root / "third_party/mogo"
manifest = json.loads((vendor / "upstream-files.json").read_text())
changed = {"main/book/book_source.c", "main/book/book_source_internal.h", "main/book/book_txt.c"}
seen = set()
for entry in manifest["files"]:
    name = entry["path"]
    if name in seen:
        raise SystemExit(f"duplicate source entry: {name}")
    seen.add(name)
    data = (vendor / name).read_bytes()
    if len(data) > 5 * 1024 * 1024:
        raise SystemExit(f"vendor file exceeds task limit: {name}")
    digest = hashlib.sha256(data).hexdigest()
    if name not in changed and digest != entry["sha256"]:
        raise SystemExit(f"unrecorded upstream modification: {name}")
    if name in changed and digest == entry["sha256"]:
        raise SystemExit(f"documented adaptation missing: {name}")
for required in ("LICENSE", "licenses/Apache-2.0.txt", "licenses/kiikoread-Personal-Study-1.0.txt"):
    if required not in seen:
        raise SystemExit(f"missing license: {required}")
lines = sum(len(p.read_text().splitlines()) for p in (root / "system/graft").glob("*") if p.is_file())
if lines > 600:
    raise SystemExit(f"graft interface exceeds 600 lines: {lines}")
print(f"Mogo provenance: {len(seen)} pinned files, 3 documented adaptations; graft {lines}/600 lines")
