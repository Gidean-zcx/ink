"""Run one audit regression; optionally replace only its product object with the baseline."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASELINE = "9eac583f5f2d7c6c89092b2e10ddebc00dcb6085"
SOURCES = {
    1: ["system/apps/camera/png.c"],
    2: ["system/ui/search.c"],
    3: ["system/ui/shell/ai_pages.c"],
    4: ["system/services/backup/backup_service.c", "system/ui/shell/backup_pages.c"],
    5: ["system/services/camera/camera_service.c"],
    6: ["system/apps/files/files.c", "system/apps/gallery/gallery.c"],
    7: ["system/ui/shell/settings_family.c"],
    8: ["system/policy/members.c"],
    9: ["system/ui/shell/wallpaper.c"],
}
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
p.add_argument("--case", type=int, choices=range(1, 10), required=True)
p.add_argument("--baseline", action="store_true")
a = p.parse_args()
build = a.build.resolve()
name = f"audit3-b3-{a.case:02}"
area = build / "test-out" / (name + ("-baseline" if a.baseline else ""))
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
target = "test-audit3-png" if a.case == 1 else "sim-audit3-fix"
binary = build / target

if a.baseline:
    link = shlex.split((build / f"CMakeFiles/{target}.dir/link.txt").read_text())
    objects = []
    for i, relative in enumerate(SOURCES[a.case]):
        source = area / f"baseline-{i}.c"
        source.write_bytes(subprocess.check_output(["git", "show", f"{BASELINE}:{relative}"], cwd=ROOT))
        flags = {}
        flag_target = "ink_camera_png" if a.case == 1 else "ink_system"
        for line in (build / f"CMakeFiles/{flag_target}.dir/flags.make").read_text().splitlines():
            if " = " in line:
                key, value = line.split(" = ", 1)
                flags[key] = shlex.split(value)
        obj = area / f"baseline-{i}.o"
        command = [link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                   "-I", str((ROOT / relative).parent), "-c", str(source), "-o", str(obj)]
        result = subprocess.run(command, cwd=build, capture_output=True, text=True)
        (area / f"compile-{i}.log").write_text(result.stdout + result.stderr)
        if result.returncode:
            raise SystemExit("baseline compilation failed: " + result.stderr)
        objects.append(str(obj))
    binary = area / target
    link[1:1] = objects
    link[link.index("-o") + 1] = str(binary)
    result = subprocess.run(link, cwd=build, capture_output=True, text=True)
    (area / "link.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit("baseline linking failed: " + result.stderr)

command = [str(binary)]
if a.case != 1:
    script = area / "case.script"
    script.write_text("@0 net wifi\n@150s end\n")
    command += ["--profile", str(ROOT / "simulator/profiles/metalio-plus.json"), "--script", str(script),
                "--assets", str(build / "assets"), "--content", str(build / "test-content"),
                "--budget", str(ROOT / "tests/ui_scene.budget.json"), "--out", str(area / "out"),
                "--quiet", "--no-random-outages"]
result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180,
                        env={**os.environ, "INK_AUDIT3_CASE": str(a.case), "UBSAN_OPTIONS": "halt_on_error=1"})
(area / "run.log").write_text(result.stdout + result.stderr)
if a.case == 1:
    passed = result.returncode == 0
    evidence = result.stdout + result.stderr
    counts = {}
else:
    metrics_path = area / "out/metrics.json"
    data = json.loads(metrics_path.read_text()) if metrics_path.exists() else {}
    counts = data.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0) and not data.get("warnings")
    evidence = json.dumps(data.get("warnings", []), ensure_ascii=False) + result.stderr
summary = {"case": a.case, "baseline": a.baseline, "exit": result.returncode,
           "finished": counts.get("test.finished"), "checks": counts.get("test.pass"), "failures": counts.get("test.fail", 0)}
(area / "result.json").write_text(json.dumps(summary, ensure_ascii=False) + "\n")
if a.baseline:
    expected = f"B3-{a.case:02}"
    verified = not passed and expected in evidence and (a.case == 1 or counts.get("test.finished") == 1)
else:
    verified = passed
print(name, "BASELINE_FAIL_CONFIRMED" if a.baseline and verified else "PASS" if verified else "FAIL", summary, flush=True)
if not verified:
    print(evidence[-5000:])
if verified and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(area)
raise SystemExit(0 if verified else 1)
