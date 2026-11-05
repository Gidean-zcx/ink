"""Only validate this audit's files; no product code or services are executed."""
import hashlib
import json
import re
from collections import Counter
from pathlib import Path
from urllib.parse import unquote, urlsplit

HERE = Path(__file__).resolve().parent
AUDIT = HERE.parent
ROOT = AUDIT.parents[2]


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


findings = read_json(AUDIT / "findings.json")
verification = read_json(AUDIT / "verification.json")
source = read_json(HERE / "source-verification.json")
report = (AUDIT / "report.md").read_text(encoding="utf-8")

assert findings == [], "Expected zero new critical/high findings"
assert len(verification) == 2
assert {v["id"] for v in verification} == {"HUB6-001", "HUB6-002"}
assert all(set(v) == {"id", "verdict", "evidence", "note"} for v in verification)
assert all(v["verdict"] == "已解决" for v in verification)
assert all(v["evidence"].startswith("【文档核查】") for v in verification)
assert "待核查" not in report and "待核查完成后填写" not in report
assert source["review_commit"] in report
assert source["base_commit"] in report

source_hashes = {}
for doc in source["documents"]:
    actual = hashlib.sha256((ROOT / doc["path"]).read_bytes()).hexdigest()
    assert actual == doc["local_sha256"], doc["path"]
    assert doc["base_plus_commit_patches_equals_local"] is True
    source_hashes[doc["path"]] = actual

references = []
for relative, line in re.findall(r"`(docs/[A-Za-z0-9_./-]+):(\d+)`", report):
    target = ROOT / relative
    assert target.is_file(), relative
    assert 1 <= int(line) <= len(target.read_text(encoding="utf-8").splitlines())
    references.append(f"{relative}:{line}")

links = []
for markdown in (AUDIT / "report.md", HERE / "README.md"):
    text = markdown.read_text(encoding="utf-8")
    for link in re.findall(r"\[[^\]]*\]\(([^)]+)\)", text):
        parts = urlsplit(link)
        if parts.scheme or not parts.path:
            continue
        target = markdown.parent / unquote(parts.path)
        assert target.is_file(), str(target)
        links.append(str(target.relative_to(ROOT)))
    assert all(line == line.rstrip() for line in text.splitlines()), str(markdown)

sizes = {}
for path in sorted(AUDIT.rglob("*")):
    if path.is_file():
        assert not path.is_symlink(), str(path)
        assert path.stat().st_size < 1_000_000, str(path)
        sizes[str(path.relative_to(AUDIT))] = path.stat().st_size

result = {
    "status": "passed",
    "evidence_type": "artifact_validation_only",
    "review_commit": source["review_commit"],
    "verification_counts": dict(Counter(v["verdict"] for v in verification)),
    "new_findings": {"严重": 0, "高": 0},
    "reviewed_source_hashes_unchanged": source_hashes,
    "local_links_checked": links,
    "explicit_file_line_references_checked": references,
    "all_audit_files_under_1MB": True,
    "file_sizes_bytes_before_this_result_write": sizes,
    "limitations": "No product, device, model, or service tests. Git metadata reads hung; full repository status could not be rechecked. Scope of writes is evidenced by the tool operations, not a successful final git status.",
}
serialized = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
assert len(serialized.encode("utf-8")) < 1_000_000
(HERE / "validation.json").write_text(serialized, encoding="utf-8")
print(json.dumps({"status": result["status"], "verification_counts": result["verification_counts"], "new_findings": result["new_findings"], "checked_source_files": len(source_hashes)}, ensure_ascii=False))
