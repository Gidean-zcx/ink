"""Compare the service design table with the existing runtime-surface contract."""
import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--design", type=Path, default=ROOT / "docs/system/system-services-design.md")
args = parser.parse_args()
design = args.design.read_text()
table = design.split("### 1.3 能力总表\n", 1)[1].split("\n## 2.", 1)[0]
columns = [set() for _ in range(7)]
for line in table.splitlines():
    if line.startswith("| "):
        for column, cell in zip(columns, line.split("|")[1:-1]):
            column.update(re.findall(r"`([a-z][a-z0-9_.]+)`", cell))

surface = (ROOT / "tests/integration/ai_surface.c").read_text()
domains = {
    "data", "job", "file", "usb", "time", "alarm", "reminder", "notify", "interrupt",
    "confirm", "app", "text", "clipboard", "media", "audio", "bt", "wifi", "member",
    "update", "system", "state", "settings", "caps",
    "storage", "power", "battery", "ai", "voice", "net",
}

def array(name):
    return surface.split(name + "[] = {", 1)[1].split("};", 1)[0]

failures = []
for name, column in (("FOR_AI", 1), ("NOT_FOR_AI", 2), ("STATES", 3), ("SETTINGS", 4), ("EVENTS", 5)):
    expression = r'\{"([a-z][a-z0-9_.]+)"' if name in ("FOR_AI", "NOT_FOR_AI") else r'"([a-z][a-z0-9_.]+)"'
    expected = {value for value in re.findall(expression, array(name)) if value.split(".", 1)[0] in domains}
    missing = expected - columns[column]
    if missing:
        failures.append(name + ": " + ", ".join(sorted(missing)))
    print(f"{name}: checked={len(expected)} missing={len(missing)}")
if failures:
    raise SystemExit("Missing from service design §1.3:\n" + "\n".join(failures))
print("PASS: service capability, state, setting and event columns match the surface contract")
