"""Check the pinned vendor snapshot without requiring Git history or external checkouts."""
from pathlib import Path
import argparse
import difflib
import hashlib
import io
import json
import subprocess
import tarfile

root = Path(__file__).resolve().parents[2]
vendor = root / "third_party/mogo"
manifest = json.loads((vendor / "upstream-files.json").read_text())
changed = {"main/book/book_source.c", "main/book/book_source_internal.h", "main/book/book_txt.c",
           "tools/book_shelf_test_code.py", "tools/book_ui_host_test.py"}
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
print(f"Mogo provenance: {len(seen)} pinned files, {len(changed)} documented adaptations; graft {lines}/600 lines")

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--upstream", type=Path, help="manual check against the pinned read-only upstream Git objects")
args = parser.parse_args()
if args.upstream:
    result = subprocess.run(["git", "-C", str(args.upstream), "archive", "--format=tar",
                             manifest["commit"], *sorted(seen)], check=True, capture_output=True)
    expected = {e["path"]: e["sha256"] for e in manifest["files"]}
    verified = additions = removals = 0
    with tarfile.open(fileobj=io.BytesIO(result.stdout)) as archive:
        for entry in archive:
            if not entry.isfile():
                continue
            data = archive.extractfile(entry).read()
            if hashlib.sha256(data).hexdigest() != expected[entry.name]:
                raise SystemExit(f"pinned Git object mismatch: {entry.name}")
            verified += 1
            if entry.name in changed:
                original = data.decode().splitlines()
                current = (vendor / entry.name).read_text().splitlines()
                for op, start, end, new_start, new_end in difflib.SequenceMatcher(a=original, b=current, autojunk=False).get_opcodes():
                    if op in ("insert", "replace"):
                        additions += new_end - new_start
                    if op in ("delete", "replace"):
                        removals += end - start
    if verified != len(seen) or additions + removals > 1500:
        raise SystemExit("upstream file count or adaptation budget mismatch")
    print(f"Git objects verified: {verified}; Mogo adaptations +{additions}/-{removals}, {additions + removals}/1500 lines")
