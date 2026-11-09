"""Check complete F4 snapshots exported by the real UI test."""
import json
from pathlib import Path
import sys

snapshots = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
assert len(snapshots) == 7, len(snapshots)
expected = [(1, 334, 1000, 0, 3), (2, 334, 1000, 3, 3),
            (2, 334, 1000, 3, 3), (2, 334, 1000, 3, 3),
            (334, 334, 1000, 999, 1), (1, 1, 0, 0, 0), (1, 1, 2, 0, 2)]
for snapshot, values in zip(snapshots, expected):
    p = snapshot["pagination"]
    assert tuple(p[k] for k in ("page", "pages", "total", "offset", "visible")) == values
    assert len(snapshot["fields"]) == p["visible"]
    assert p["truncated"] is (p["visible"] < p["total"])
    assert p["note"] == "仅含当前页，其他项请翻页读取"
    actions = {action["id"] for action in snapshot["actions"]}
    assert ("prev" in actions) is (p["page"] > 1)
    assert ("next" in actions) is (p["page"] < p["pages"])
    assert len({f["object"]["id"] for f in snapshot["fields"]}) == p["visible"]
for snapshot in snapshots[2:4]:
    selected = snapshot["pagination"]["selected"]
    assert selected.startswith("file:")
    assert snapshot["fields"][1]["object"]["id"] == selected
    assert snapshot["fields"][1]["label"] == "改名后.txt"
assert snapshots[4]["fields"][0]["object"]["id"] == "example:item-1000"
print("7 complete snapshots: pagination, endpoints, empty/single/last page and stable selection passed")
