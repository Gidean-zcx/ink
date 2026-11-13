"""Run calendar acceptance on real pages; optionally remove the service member filter."""
import argparse
import json
import os
import re
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
STATUS_BAR = int(re.search(r'INK_STATUS_BAR\s*=\s*(\d+)', (ROOT / 'system/ui/ink/ui_theme.h').read_text())[1])
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--negative-member", action="store_true")
parser.add_argument("--visual", action="store_true")
args = parser.parse_args()
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="calendar-", dir=build))
binary = build / "sim-calendar"
if args.negative_member:
    source_path = ROOT / "system/services/time/time_service.c"
    source = source_path.read_text()
    old = "T.a[i].kind == kind && !strcmp(T.a[i].owner, member)"
    assert source.count(old) == 1
    flags = {}
    for line in (build / "CMakeFiles/ink_system.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-calendar.dir/link.txt").read_text())
    obj = work / "time-unscoped.o"
    subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                    "-I", str(source_path.parent), "-x", "c", "-c", "-", "-o", str(obj)],
                   input=source.replace(old, "T.a[i].kind == kind"), text=True, cwd=build, check=True)
    link.insert(next(i for i, word in enumerate(link) if word.endswith(".a")), str(obj))
    binary = work / "sim-calendar"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)

ok = True
for scene in (("Upcoming", "Calendar") if args.visual else ("",)):
    out = work / (scene or "acceptance")
    result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(ROOT / "tests/calendar/calendar.script"), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"),
        "--out", str(out), "--quiet", "--no-random-outages", "--trace"],
        env={**os.environ, "INK_CALENDAR_SCENE": scene}, capture_output=True, text=True, timeout=150)
    (work / f"{scene or 'acceptance'}.log").write_text(result.stdout + result.stderr)
    data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
    counts = data.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
    trace = json.loads((out / "trace.json").read_text()) if (out / "trace.json").exists() else []
    marks = {e["name"]: e["ts"] for e in trace if e.get("name") in ("calendar.minute.before", "calendar.minute.after")}
    updates = [e for e in trace if e.get("name") in ("DU", "GC16", "GL16", "A2") and
               marks.get("calendar.minute.before", float("inf")) <= e.get("ts", -1) < marks.get("calendar.minute.after", 0)]
    # control-1: a new minute refreshes the status bar's time alone, so the area lies inside the bar, not at its corner
    partial = bool(updates) and all(e["name"] == "DU" and e["args"]["x"] >= 0 and e["args"]["y"] >= 0 and
                                  e["args"]["x"] + e["args"]["w"] <= STATUS_BAR and e["args"]["y"] + e["args"]["h"] <= 684 for e in updates)
    print("Minute refresh stays in the status bar:", partial, flush=True)
    passed &= partial
    if scene == "Calendar":
        from PIL import Image, ImageChops
        with Image.open(out / "snap-MinuteBefore.png") as before, Image.open(out / "snap-MinuteAfter.png") as after:
            unchanged = ImageChops.difference(before, after).crop((0, STATUS_BAR, before.width, before.height)).getbbox() is None
        print("Rendered calendar below the status bar is unchanged:", unchanged, flush=True)
        passed &= unchanged
    if args.negative_member:
        passed = result.returncode != 0 and counts.get("test.finished") == 1 and counts.get("test.fail", 0) > 0
    ok &= passed
    print(scene or "acceptance", "PASS" if passed else "FAIL", "exit", result.returncode,
          "checks", counts.get("test.pass", 0), "failures", counts.get("test.fail", 0), flush=True)
    if args.negative_member or not passed:
        for warning in data.get("warnings", []):
            if "CHECK FAILED" in str(warning) or not passed: print(warning)
        if not passed: print(result.stderr[-2000:])
    if scene and passed:
        target = build / "calendar-compare" / scene
        target.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(out / "snap-scene.png", target / "snap-scene.png")
        shutil.copyfile(out / "metrics.json", target / "metrics.json")
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT), flush=True)
raise SystemExit(0 if ok else 1)
