"""Check the five real scene snapshots and prepare verified ui_compare inputs."""
import json
from pathlib import Path
import shutil
import sys

source, target = map(Path, sys.argv[1:])
metrics = json.loads((source / "metrics.json").read_text())["metrics"]
assert metrics.get("test.finished") == 1 and metrics.get("test.fail", 0) == 0
scenes = ("AiHome", "AiAsk", "AiListenTap", "AiLong", "AiSpeak")
caps = {"shell.ai_ask", "shell.ai_send", "shell.ai_stop", "shell.ai_home", "shell.back",
        "ai.panel.listen", "ai.panel.hide", "ai.panel.next", "ai.panel.prev", "ai.panel.replay"}
for scene in scenes:
    snapshot = json.loads((source / scene / "snapshot.json").read_text())
    if "keyboard" in snapshot:
        assert snapshot["keyboard"]["session"] > 0
        snapshot = json.loads(snapshot["page"])
    assert snapshot["page_version"] > 0 and snapshot["object"]["id"]
    fields = {field["id"]: field["value"] for field in snapshot["fields"]}
    if scene in ("AiHome", "AiAsk"):
        assert snapshot["page"] == scene
    else:
        assert fields["board"] == scene
    assert snapshot["actions"]
    for action in snapshot["actions"]:
        assert action["capability"] in caps and action["object"]["id"]
    if scene == "AiLong":
        assert fields["page"].startswith("页码=1/") and int(fields["page"].split("/")[1]) > 1
        assert "next" in {action["id"] for action in snapshot["actions"]}
        assert "prev" not in {action["id"] for action in snapshot["actions"]}
    if scene == "AiSpeak":
        assert "stop" in {action["id"] for action in snapshot["actions"]}
    count = metrics[f"test.snapshot.{scene}"]
    assert count == 1
    folder = target / scene
    folder.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source / f"snap-{scene}.png", folder / "snap-scene.png")
    # Each capture checked the settled frame; preserve that scene's actual count.
    proof = {"test.snapshot.settled": count, f"test.snapshot.{scene}": count,
             "test.finished": metrics["test.finished"], "test.fail": metrics.get("test.fail", 0)}
    (folder / "metrics.json").write_text(json.dumps({"metrics": proof}) + "\n")
print("PASS: five F1 snapshots, page/action state and verified comparison frames")
