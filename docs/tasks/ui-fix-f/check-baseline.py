"""Manual control: run AI-surface with all four task modules restored to the baseline in separate objects."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
BASELINE = "a379ef4323b576dbda6543cf0874037f05ca1056"
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
work = build / "ui-fix-f-baseline-surface"
work.mkdir(exist_ok=True)
link = shlex.split((build / "CMakeFiles/sim-ai-surface.dir/link.txt").read_text())
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
binary = work / "sim-ai-surface"
link[link.index("-o") + 1] = str(binary)
subprocess.run(link, cwd=build, check=True)
command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--script", str(ROOT / "tests/integration/ai_surface.script"), "--budget", str(ROOT / "tests/integration/ai_surface.budget.json"),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--out", str(work),
           "--quiet", "--no-random-outages"]
r = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
(work / "run.log").write_text(r.stdout + r.stderr)
data = json.loads((work / "metrics.json").read_text())
counts = data.get("metrics", {})
warnings = [str(w) for w in data.get("warnings", [])]
expected = "gallery.reload must be in exactly one table"
assert r.returncode != 0 and counts.get("test.finished") == 1 and any(expected in w for w in warnings), warnings
print(json.dumps({"exit": r.returncode, "pass": counts.get("test.pass", 0), "fail": counts.get("test.fail", 0),
                  "baseline_failure": expected}, ensure_ascii=False))
