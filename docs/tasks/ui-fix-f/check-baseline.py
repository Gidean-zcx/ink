"""Manual control: run AI-surface with all four task modules restored to the baseline in separate objects."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
BASELINE = "a379ef4323b576dbda6543cf0874037f05ca1056"
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
p.add_argument("--case", choices=("surface", "receipts", "privacy"), default="surface")
a = p.parse_args()
build = a.build.resolve()
target = {"surface": "sim-ai-surface", "receipts": "sim-audit5-fix", "privacy": "sim-privacy-records"}[a.case]
work = build / ("ui-fix-f-baseline-" + a.case)
work.mkdir(exist_ok=True)
link = shlex.split((build / f"CMakeFiles/{target}.dir/link.txt").read_text())
source_flags = (build / "CMakeFiles/ink_system.dir/flags.make").read_text()
flags = [v for name in ("C_DEFINES", "C_INCLUDES", "C_FLAGS")
         for v in shlex.split(re.search(r"^" + name + r" = (.*)$", source_flags, re.M)[1])]
objects = []
for name in ("remote", "weather", "calendar", "calculator"):
    path = f"system/apps/{name}/{name}.c"
    source = subprocess.run(["git", "show", BASELINE + ":" + path], cwd=ROOT, capture_output=True, check=True).stdout
    saved, obj = work / (name + ".c"), work / (name + ".o")
    saved.write_bytes(source)
    subprocess.run([link[0], *flags, "-iquote", str((ROOT / path).parent), "-c", str(saved), "-o", str(obj)], cwd=build, check=True)
    objects.append(str(obj))
pos = next(i for i, word in enumerate(link) if word.endswith(".a"))
link[pos:pos] = objects
binary = work / target
link[link.index("-o") + 1] = str(binary)
subprocess.run(link, cwd=build, check=True)
script, budget = ROOT / "tests/integration/ai_surface.script", ROOT / "tests/integration/ai_surface.budget.json"
if a.case != "surface":
    script, budget = work / "case.script", ROOT / "tests/ui_scene.budget.json"
    script.write_text("@0 net wifi\n@" + ("80" if a.case == "receipts" else "180") + "s end\n")
command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--script", str(script), "--budget", str(budget),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--out", str(work),
           "--quiet", "--no-random-outages"]
r = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120,
                   env={**os.environ, "INK_AUDIT5_MODE": "receipts"})
(work / "run.log").write_text(r.stdout + r.stderr)
data = json.loads((work / "metrics.json").read_text())
counts = data.get("metrics", {})
warnings = [str(w) for w in data.get("warnings", [])]
expected = {"surface": "gallery.reload must be in exactly one table", "receipts": "receipt snapshot contains only the requested member",
            "privacy": "AiLog count and retention match summary"}[a.case]
assert r.returncode != 0 and counts.get("test.finished") == 1 and any(expected in w for w in warnings), warnings
print(json.dumps({"case": a.case, "exit": r.returncode, "pass": counts.get("test.pass", 0), "fail": counts.get("test.fail", 0),
                  "baseline_failure": expected}, ensure_ascii=False))
