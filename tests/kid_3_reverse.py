"""Link negative navigation objects without changing the working source or library."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / "kid-3-reverse"
area.mkdir(parents=True, exist_ok=True)
path = ROOT / "system/ui/shell/navigation.c"
source = path.read_text()
digest = hashlib.sha256(path.read_bytes()).hexdigest()
baseline = subprocess.run(["git", "show", "2228df7de99de9fea15fddc22868a0de1caae770:system/ui/shell/navigation.c"],
    cwd=ROOT, capture_output=True, text=True, check=True).stdout
needle = "ink_db_open_member(&TODAY_DB, ink_member_current()->id)"
assert needle in source
cases = [("baseline", baseline, "child Apps snapshot uses KidApps"),
    ("shared-adult-cards", source.replace(needle, 'ink_db_open_member(&TODAY_DB, "owner")'),
        "adult TodayEdit changes cannot remove the child's cards (counterexample)"),
    ("overlap-active-work", source.replace("bool kid_idle = child() && !call_offset;", "bool kid_idle = child();"),
        "KidHome gives active work its own row without an overlapping allowance")]
flags = (build / "CMakeFiles/ink_system.dir/flags.make").read_text()
options = []
for key in ("C_DEFINES", "C_INCLUDES", "C_FLAGS"):
    options += shlex.split(re.search(r"^" + key + r" = (.*)$", flags, re.M)[1])
summary = []
for name, variant, expected in cases:
    link = shlex.split((build / "CMakeFiles/sim-kid-3.dir/link.txt").read_text())
    obj, binary = area / (name + ".o"), area / name
    compiled = subprocess.run([link[0], *options, "-iquote", str(path.parent), "-x", "c", "-", "-c", "-o", str(obj)],
        input=variant, cwd=build, capture_output=True, text=True, timeout=90)
    assert compiled.returncode == 0, compiled.stderr
    link[link.index("-o") + 1] = str(binary)
    link.insert(next(i for i, part in enumerate(link) if part.endswith("libink_system.a")), str(obj))
    linked = subprocess.run(link, cwd=build, capture_output=True, text=True, timeout=90)
    assert linked.returncode == 0, linked.stderr
    result = subprocess.run([os.sys.executable, str(ROOT / "tests/kid_3.py"), "--build", str(build),
        "--binary", str(binary), "--scene", "KidToday"], cwd=ROOT, capture_output=True, text=True,
        timeout=150, env={**os.environ, "INK_KEEP_EVIDENCE": "1"})
    output = result.stdout + result.stderr
    (area / (name + ".log")).write_text(output)
    counts = json.loads((build / "test-out" / ("kid-3-" + name) / "KidToday/metrics.json").read_text())["metrics"]
    assert result.returncode != 0 and counts.get("test.finished") == 1 and counts.get("test.fail", 0) > 0 and expected in output, (name, output)
    summary.append({"case": name, "exit": result.returncode, "failures": counts["test.fail"], "expected": expected})
    print(f'{name}: expected failure ({int(counts["test.fail"])} checks)', flush=True)
assert hashlib.sha256(path.read_bytes()).hexdigest() == digest
(area / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
