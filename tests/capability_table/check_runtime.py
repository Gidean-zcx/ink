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
    for label in ("capability missing", "setting missing", "description", "risk", "exposure", "member", "confirmation", "setting confirmation"):
        changed = copy.deepcopy(inventory)
        if label == "capability missing":
            changed["capabilities"].pop()
        elif label == "setting missing":
            changed["settings"].pop()
        elif label == "setting confirmation":
            changed["settings"][0]["ask_by_hand"] = not changed["settings"][0].get("ask_by_hand", False)
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
        negative.write_text(json.dumps(changed, ensure_ascii=False), encoding="utf-8")
        try:
            catalogue.check_runtime(entries, negative)
        except ValueError:
            cases.append(label)
        else:
            raise SystemExit(f"FAIL: negative control accepted: {label}")
    print(f"PASS: {len(cases)} runtime negative controls rejected: {', '.join(cases)}")
