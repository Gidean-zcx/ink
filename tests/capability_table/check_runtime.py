"""Check the compiled inventory, then prove that omissions and policy drift are rejected."""
import copy
import json
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import capabilities as catalogue

path = Path(sys.argv[1])
entries, _ = catalogue.load(ROOT)
catalogue.check_runtime(entries, path)
inventory = json.loads(path.read_text(encoding="utf-8"))

with tempfile.TemporaryDirectory(prefix="reverse-", dir=path.parent) as directory:
    negative = Path(directory) / "inventory.json"
    cases = []

    def reject_changed(label, changed, source_entries=entries):
        negative.write_text(json.dumps(changed, ensure_ascii=False), encoding="utf-8")
        try:
            catalogue.check_runtime(source_entries, negative)
        except ValueError:
            cases.append(label)
        else:
            raise SystemExit(f"FAIL: negative control accepted: {label}")

    for label in ("capability missing", "setting missing", "description", "risk", "exposure", "member", "confirmation", "setting confirmation", "setting type"):
        changed = copy.deepcopy(inventory)
        if label == "capability missing":
            changed["capabilities"].pop()
        elif label == "setting missing":
            changed["settings"].pop()
        elif label == "setting confirmation":
            changed["settings"][0]["ask_by_hand"] = not changed["settings"][0].get("ask_by_hand", False)
        elif label == "setting type":
            changed["settings"][0]["type"] = "unknown"
        else:
            cap = changed["capabilities"][0]
            if label == "description":
                cap["title"] = ""
            elif label == "risk":
                cap["effect"] = 99
            elif label == "exposure":
                cap["expose"] ^= 2
            elif label == "member":
                cap["child_ok"] = not cap["child_ok"]
            else:
                cap["confirmations"][0] = (cap["confirmations"][0] + 1) % 3
        reject_changed(label, changed)
    for i, setting in enumerate(inventory["settings"]):
        for flag in ("system", "ask_by_hand", "child_may_change"):
            changed = copy.deepcopy(inventory)
            changed["settings"][i][flag] = not setting[flag]
            reject_changed(f"{setting['id']} {flag}", changed)
        changed = copy.deepcopy(inventory)
        del changed["settings"][i]["child_may_change"]
        reject_changed(f"{setting['id']} missing child permission", changed)
        source_entries = copy.deepcopy(entries)
        source = next(e for e in source_entries if e.kind == "setting" and e.id == setting["id"])
        source.fields["child_may_change"] = ["false" if source.flag("child_may_change") else "true"]
        reject_changed(f"{setting['id']} wrong source child permission", inventory, source_entries)
    for flag in ("system", "ask_by_hand", "child_may_change"):
        changed = copy.deepcopy(inventory)
        changed["settings"][0][flag] = "false"
        reject_changed(f"non-boolean {flag}", changed)
    print(f"PASS: {len(cases)} runtime negative controls rejected: {', '.join(cases)}")
