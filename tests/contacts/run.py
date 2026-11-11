"""Check CRUD across boots, real touch/AI paths and complete pagination snapshots."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

sim, root, assets, content, output = map(Path, sys.argv[1:])
shutil.rmtree(output, ignore_errors=True)
output.mkdir(parents=True)
for phase in ("write", "verify", "stale", "delete"):
    command = [str(sim), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
               "--script", str(root / "tests/contacts/contacts.script"),
               "--budget", str(root / "tests/ui_scene.budget.json"),
               "--assets", str(assets), "--content", str(content), "--flash-image", str(output / ("user.img" if phase in ("write", "verify") else f"{phase}.img")),
               "--sd-dir", str(output / "sd"), "--out", str(output / phase), "--quiet", "--no-random-outages"]
    result = subprocess.run(command, env=dict(os.environ, INK_CONTACTS_PHASE=phase), capture_output=True, text=True)
    metrics = json.loads((output / phase / "metrics.json").read_text())
    assert result.returncode == 0, (phase, result.returncode, result.stderr, metrics.get("warnings"))
    assert metrics["metrics"].get("test.finished") == 1
    if phase in ("stale", "delete"):
        print(f'{phase}: {metrics["metrics"]["test.pass"]} assertions')
        continue
    snapshots = [json.loads(line) for line in (output / phase / "snapshots.ndjson").read_text().splitlines()]
    expected = {"empty": (1, 1, 0, 0, 0), "one": (1, 1, 1, 0, 1), "four": (1, 1, 4, 0, 4),
                "multi": (1, 2, 5, 0, 4), "last": (2, 2, 5, 4, 1), "kid-empty": (1, 1, 0, 0, 0),
                "owner-return": (1, 2, 5, 0, 4), "reboot": (1, 2, 5, 0, 4), "kid-reboot": (1, 1, 1, 0, 1)}
    for item in snapshots:
        snapshot = item["snapshot"]
        if item["case"] == "form":
            assert [f["value"] for f in snapshot["fields"]] == ["触摸测试", "00000000002", ""]
            assert {a["capability"] for a in snapshot["actions"]} == {"contacts.name", "contacts.number", "contacts.note", "contacts.put"}
            continue
        p = snapshot["pagination"]
        assert tuple(p[k] for k in ("page", "pages", "total", "offset", "visible")) == expected[item["case"]], item
        assert len(snapshot["fields"]) == p["visible"]
        assert p["truncated"] is (p["visible"] < p["total"])
        assert len({field["object"]["id"] for field in snapshot["fields"]}) == p["visible"]
        assert ("contacts.next" in {a["capability"] for a in snapshot["actions"]}) is (p["page"] < p["pages"])
    print(f'{phase}: {metrics["metrics"]["test.pass"]} assertions, {len(snapshots)} complete snapshots')
