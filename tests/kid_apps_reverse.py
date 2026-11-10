"""Manual negative controls; no source edits or Git history required."""
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
p.add_argument("--build", required=True, type=Path)
a = p.parse_args()
build = a.build.resolve()
area = build / "kid-apps-reverse"
area.mkdir(parents=True, exist_ok=True)
path = ROOT / "system/policy/members.c"
source = path.read_text()
digest = hashlib.sha256(path.read_bytes()).hexdigest()
cases = [
    ("list", "mode = KP[s - M.s].apps[i].mode", "mode = 1", "AI directory hides removed apps"),
    ("period", "|| !ink_time_valid()) return false;", "|| ink_time_valid() || !ink_time_valid()) return false;",
        "Tuesday morning belongs to Monday's overnight period"),
    ("persistence", "r->policy = KP[s - M.s];", "r->policy = (kid_policy_t){0};",
        "overnight period and weekdays survive restart"),
    ("home-entry", '!strcmp(app, "home") || ink_member_app_mode(NULL, app) == 2',
        'strcmp(app, "home") && ink_member_app_mode(NULL, app) == 2',
        "restricted hours allow the real HOME key to reach permitted applications"),
    ("deadline-notice", '    if (previous_daily != daily) ink_app_card_changed("home");', "    (void)previous_daily;",
        "daily expiry updates an exempt app inside an already restricted period"),
]
flags = (build / "CMakeFiles/ink_system.dir/flags.make").read_text()
options = []
for key in ("C_DEFINES", "C_INCLUDES", "C_FLAGS"):
    options += shlex.split(re.search(r"^" + key + r" = (.*)$", flags, re.M)[1])
summary = []
for name, needle, replacement, expected in cases:
    assert source.count(needle) == 1, (name, needle)
    variant = source.replace(needle, replacement)
    link = shlex.split((build / "CMakeFiles/sim-kid-apps.dir/link.txt").read_text())
    obj, binary = area / (name + ".o"), area / name
    result = subprocess.run([link[0], *options, "-iquote", str(path.parent), "-x", "c", "-", "-c", "-o", str(obj)],
        input=variant, cwd=build, capture_output=True, text=True, timeout=90)
    assert result.returncode == 0, result.stderr
    link[link.index("-o") + 1] = str(binary)
    link.insert(next(i for i, x in enumerate(link) if x.endswith("libink_system.a")), str(obj))
    result = subprocess.run(link, cwd=build, capture_output=True, text=True, timeout=90)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([os.sys.executable, str(ROOT / "tests/kid_apps.py"), "--build", str(build), "--binary", str(binary)],
        cwd=ROOT, capture_output=True, text=True, timeout=300, env={**os.environ, "INK_KEEP_EVIDENCE": "1"})
    output = result.stdout + result.stderr
    (area / (name + ".log")).write_text(output)
    evidence = ROOT / re.search(r"Evidence: (.+)", output)[1].strip()
    data = [json.loads(x.read_text())["metrics"] for x in evidence.glob("*/metrics.json")]
    assert result.returncode != 0 and expected in output and any(x.get("test.finished") == 1 and x.get("test.fail", 0) for x in data), output
    item = {"case": name, "exit": result.returncode, "failures": sum(x.get("test.fail", 0) for x in data), "expected": expected}
    summary.append(item)
    print(json.dumps(item, ensure_ascii=False), flush=True)
assert hashlib.sha256(path.read_bytes()).hexdigest() == digest
(area / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
