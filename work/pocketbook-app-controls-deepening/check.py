#!/usr/bin/env python3
"""Read-only pre-merge validation. Do not run after merging the suggestions."""
import csv
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / "work/pocketbook-app-controls-deepening"
LIB = ROOT / "docs/research/interaction-library"
BATCH = LIB / "batches/pocketbook-app-controls-deepening"
ART = LIB / "artifacts/pocketbook-app-controls-deepening"


def read_json(path):
    return json.loads(path.read_text())


def read_jsonl(path):
    return [json.loads(line) for line in path.read_text().splitlines() if line]


def read_csv(path):
    with path.open(newline="") as stream:
        return list(csv.DictReader(stream))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_manifest(name):
    entries = read_json(WORK / name)
    for entry in entries:
        path = ROOT / entry["path"]
        assert path.is_file(), entry
        assert sha(path) == entry["sha256"], entry["path"]
    return len(entries)


baseline = read_json(WORK / "baseline.json")
assert sha(WORK / "case-before.md") == baseline["case_sha256"]
assert sha(WORK / "inventory-before.csv") == baseline["original_inventory_sha256"]
assert baseline["reverse_refs"] == []
old_e = read_json(WORK / "evidence-before.json")
old_f = read_json(WORK / "coverage-before.json")
all_e = read_jsonl(LIB / "evidence.jsonl")
all_f = read_csv(LIB / "coverage.csv")
master_e = {x["evidence_id"]: x for x in all_e}
master_f = {x["flow_id"]: x for x in all_f}
assert [x for x in all_e if x["product_id"] == "M-003"] == old_e, "M003 E baseline changed; check is pre-merge only"
assert [x for x in all_f if x["product_id"] == "M-003"] == old_f, "M003 F baseline changed; check is pre-merge only"
assert len(old_e) == 12 and len(old_f) == 9

new_e = read_jsonl(Path(str(BATCH) + "-evidence.jsonl"))
new_f = read_csv(Path(str(BATCH) + "-coverage.csv"))
inventory = read_csv(Path(str(BATCH) + "-inventory.csv"))
assert len(new_e) == 5 and len(new_f) == 7 and len(inventory) == 65
eids = {x["evidence_id"] for x in new_e}
fids = {x["flow_id"] for x in new_f}
assert eids == {f"E-PBACD-{n:02}" for n in range(1, 6)}
assert fids == {f"F-M003-{n:02}" for n in range(10, 17)}
assert not eids.intersection(master_e)
assert not fids.intersection(master_f)
for path in (LIB / "batches").glob("*-evidence.jsonl"):
    if path == Path(str(BATCH) + "-evidence.jsonl"):
        continue
    assert not eids.intersection(x["evidence_id"] for x in read_jsonl(path)), path

for row in new_e:
    assert row["product_id"] == "M-003"
    assert row["supersedes"] is None and row["derived_from"] == "E-PBK-01"
    assert row["url"] == master_e["E-PBK-01"]["url"]
    assert row["hands_on"] is False and row["visual_observed"] is True
    assert row["capture_state"] == "static_page_excerpt_archived"
    assert row["archived_artifact"].startswith("artifacts/pocketbook-app-controls-deepening/")
    assert sha(LIB / row["archived_artifact"]) == row["sha256"]
    assert sha(ROOT / row["raw_work_path"]) == row["raw_sha256"] == row["sha256"]
for row in new_f:
    assert row["product_id"] == "M-003" and row["depth"] == "D2"
    assert row["hands_on"] == "false"
    assert row["visual_status"] in {"not_observed", "static_reference_only"}
    assert set(row["evidence_ids"].split(";")).issubset(set(master_e) | eids)
    assert row["case_path"] == "cases/M-003-pocketbook.md"

before_inventory = read_csv(WORK / "inventory-before.csv")
for before, after in zip(before_inventory, inventory):
    for key in ("manual_item", "printed_page", "pdf_page"):
        assert before[key] == after[key], (key, before, after)
    assert after["read_basis"] in {"prior_inventory_scope_inherited", "existing_local_manual_body_reread"}
    assert set(filter(None, after["evidence_ids"].split(";"))).issubset(set(master_e) | eids)
    assert set(filter(None, after["flow_ids"].split(";"))).issubset(set(master_f) | fids)
assert sum(x["read_basis"] == "existing_local_manual_body_reread" for x in inventory) == 26
for suffix, row_count in (("-coverage.csv", 7), ("-inventory.csv", 65)):
    data = Path(str(BATCH) + suffix).read_bytes()
    assert data.count(b"\n") == row_count + 1
    assert b"\r" not in data and b"\\r\\n" not in data

updates = read_json(WORK / "old-coverage-updates.json")
assert {x["flow_id"] for x in updates} == {"F-M003-01", "F-M003-03"}
changed_count = 0
for update in updates:
    assert master_f[update["flow_id"]] == update["before"]
    actual = {key for key in update["before"] if update["before"][key] != update["after"][key]}
    assert actual == set(update["changed_fields"])
    for key, delta in update["changed_fields"].items():
        assert delta == {"before": update["before"][key], "after": update["after"][key]}
    changed_count += len(actual)
assert changed_count == 7
captures = read_json(WORK / "old-evidence-additional-captures.json")
assert len(captures) == 1 and captures[0]["evidence_id"] == "E-PBK-01"
capture = captures[0]
assert master_e["E-PBK-01"]["url"] == capture["expected_url"]
assert master_e["E-PBK-01"].get("additional_captures", []) == capture["before_additional_captures"]
assert len(capture["append_additional_captures"]) == 1
entry = capture["append_additional_captures"][0]
assert sha(LIB / entry["archived_artifact"]) == entry["sha256"]
assert entry["hands_on"] is False and entry["continuous_sequence"] is False
assert read_json(WORK / "old-evidence-field-updates.json") == []

pages = read_json(WORK / "raw/selected-manual-text.json")
expected_text_pages = list(range(28, 31)) + list(range(34, 42)) + list(range(69, 91))
assert [x["pdf_page"] for x in pages] == expected_text_pages and len(pages) == 33
observed = read_json(ART / "observed-pages.json")
expected_views = [29, 37, 41, 76, 77]
assert [x["pdf_page"] for x in observed] == expected_views
for row in observed:
    assert row["pdf_page"] == row["printed_page"]
    assert row["actual_view"] == "view_image"
    assert row["hands_on"] is False and row["continuous_sequence"] is False
    assert sha(LIB / row["archived_artifact"]) == row["sha256"]
assert entry["visual_pages"] == expected_views
metadata = read_json(WORK / "raw/manual-metadata.json")
assert metadata["page_count"] == 95
assert sha(ROOT / metadata["original_path"]) == metadata["sha256"] == "87bf56b461cd298b7c53421c5b21f72f1498e4c060d4eea89844545667030101"

case = LIB / "cases/M-003-pocketbook.md"
case_text = case.read_text()
assert len(re.findall(r"^\| F-M003-\d{2} \|", case_text, re.M)) == 16
assert not any(term in case_text for term in ("待root合并", "主表未改", "本次重看", "本次重读"))
for target in re.findall(r"\]\(([^)]+)\)", case_text):
    if target.startswith(("http://", "https://", "#")):
        continue
    target = target.split("#", 1)[0]
    assert (case.parent / target).exists(), target
formal_count = check_manifest("formal-manifest.json")
raw_count = check_manifest("raw-manifest.json")
assert formal_count == 11 and raw_count == 9
print(json.dumps({
    "result": "PASS", "mode": "read_only_pre_merge", "new_e": 5,
    "new_f": 7, "old_f_updated": 2, "old_f_fields": 7,
    "old_e_additional_captures": 1, "inventory_rows": 65,
    "rows_with_current_body_detail": 26, "text_pages": 33,
    "actual_static_pages": expected_views, "formal_files": formal_count,
    "raw_files": raw_count, "network_requests": 0,
    "hands_on": False, "state_changed_by_check": False
}, ensure_ascii=False, indent=2))
