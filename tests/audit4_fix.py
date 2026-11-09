"""Run one batch4 regression, optionally restoring its baseline product code."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASELINE = "2318b590062486809a0f13daad6c20a320f8677a"
SOURCES = {1: "system/policy/members.c", 5: "system/ui/shell/navigation.c", 6: "system/apps/remote/remote.c"}
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
p.add_argument("--case", type=int, choices=SOURCES, required=True)
p.add_argument("--baseline", action="store_true")
a = p.parse_args()
build = a.build.resolve()
area = build / "test-out" / (f"audit4-b4-{a.case:02}" + ("-baseline" if a.baseline else ""))
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
binary = build / "sim-audit4-fix"
if a.baseline:
    relative = SOURCES[a.case]
    source = area / "baseline.c"
    source.write_bytes(subprocess.check_output(["git", "show", f"{BASELINE}:{relative}"], cwd=ROOT))
    target = "sim-audit4-fix" if a.case == 6 else "ink_system"
    flags = {}
    for line in (build / f"CMakeFiles/{target}.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-audit4-fix.dir/link.txt").read_text())
    obj = area / "baseline.o"
    command = [link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"], "-I", str((ROOT / relative).parent)]
    if a.case == 6:
        command += [f'-DINK_TEST_REMOTE_SOURCE="{source}"', "-c", str(ROOT / "tests/audit4_fix.c")]
    else:
        command += ["-c", str(source)]
    subprocess.run([*command, "-o", str(obj)], cwd=build, check=True)
    if a.case == 6:
        link = [str(obj) if word.endswith("tests/audit4_fix.c.o") else word for word in link]
    else:
        link[1:1] = [str(obj)]
    binary = area / "sim-audit4-fix"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
script = area / "case.script"
script.write_text("@0 net wifi\n@480s end\n")
result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
    "--script", str(script), "--hub", str(ROOT / "tests/audit4_fix.hub.json"),
    "--assets", str(build / "assets"), "--content", str(build / "test-content"),
    "--budget", str(ROOT / "tests/ui_scene.budget.json"), "--out", str(area / "out"), "--quiet", "--no-random-outages"],
    env={**os.environ, "INK_AUDIT4_CASE": str(a.case), "UBSAN_OPTIONS": "halt_on_error=1"},
    capture_output=True, text=True, timeout=180)
(area / "run.log").write_text(result.stdout + result.stderr)
data = json.loads((area / "out/metrics.json").read_text()) if (area / "out/metrics.json").exists() else {}
counts = data.get("metrics", {})
passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0) and not data.get("warnings")
summary = {"case": a.case, "baseline": a.baseline, "exit": result.returncode, "finished": counts.get("test.finished"),
           "checks": counts.get("test.pass", 0), "failures": counts.get("test.fail", 0)}
(area / "result.json").write_text(json.dumps(summary) + "\n")
evidence = json.dumps(data.get("warnings", []), ensure_ascii=False) + result.stderr
verified = (not passed and counts.get("test.finished") == 1 and f"B4-{a.case:02}" in evidence) if a.baseline else passed
print("BASELINE_FAIL_CONFIRMED" if a.baseline and verified else "PASS" if verified else "FAIL", summary, flush=True)
if not verified:
    print(evidence[-6000:])
if verified and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(area)
raise SystemExit(0 if verified else 1)
