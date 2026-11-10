"""One real shell presenter per run; optional baseline comparison is manual only."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCES = {
    "confirm": "system/ui/shell/confirm_card.c",
    "interrupt": "system/ui/shell/interrupt_card.c",
    "power": "system/ui/shell/power_menu.c",
    "member": "system/ui/shell/member_gate.c",
    "lock": "system/ui/lock_mark.c",
    "off": "system/ui/shell/off_picture.c",
}
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--scene", choices=SOURCES, required=True)
parser.add_argument("--baseline", help="Manual reverse comparison; never used by ctest")
args = parser.parse_args()
build = args.build.resolve()
tag = "baseline" if args.baseline else "current"
area = build / "test-out" / f"semantics-shell-{args.scene}-{tag}"
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
binary = build / "sim-semantics-shell"
if args.baseline:
    original = subprocess.check_output(["git", "show", f"{args.baseline}:{SOURCES[args.scene]}"], cwd=ROOT, text=True)
    replacement = area / "original.c"
    replacement.write_text(original)
    unit = ROOT / f"tests/semantics_shell/{args.scene}.c"
    db = json.loads((build / "compile_commands.json").read_text())
    entry = next(item for item in db if Path(item["file"]) == unit)
    command = shlex.split(entry["command"])
    obj = area / "replacement.o"
    command[command.index("-o") + 1] = str(obj)
    command += ["-I" + str(ROOT / "system/ui/shell"), "-DINK_TEST_SHELL_SOURCE=" + json.dumps(str(replacement))]
    subprocess.run(command, cwd=entry["directory"], check=True)
    link = shlex.split((build / "CMakeFiles/sim-semantics-shell.dir/link.txt").read_text())
    index = next(i for i, part in enumerate(link) if part.endswith(f"semantics_shell/{args.scene}.c.o"))
    link[index] = str(obj)
    binary = area / "sim-semantics-shell-baseline"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--script", str(ROOT / "tests/semantics_shell/scenes.script"),
           "--budget", str(ROOT / "tests/ui_scene.budget.json"), "--assets", str(build / "assets"),
           "--content", str(build / "test-content"), "--out", str(area), "--quiet", "--no-random-outages"]
result = subprocess.run(command, cwd=ROOT, env={**os.environ, "INK_SHELL_SCENE": args.scene}, capture_output=True, text=True, timeout=100)
(area / "test.log").write_text(result.stdout + result.stderr)
data = json.loads((area / "metrics.json").read_text()) if (area / "metrics.json").exists() else {}
metrics = data.get("metrics", {})
ok = (result.returncode == 0 and metrics.get("test.finished") == 1 and not metrics.get("test.fail", 0) and not data.get("warnings"))
if args.baseline:
    ok = result.returncode != 0 and metrics.get("test.fail", 0) > 0 and any(
        "uses the semantic JSON contract" in warning for warning in data.get("warnings", []))
summary = {"scene": args.scene, "mode": tag, "exit": result.returncode, "assertions": metrics.get("test.pass", 0),
           "failures": metrics.get("test.fail", 0), "expected_result": ok}
(area / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(summary, ensure_ascii=False), flush=True)
if not ok:
    print(result.stderr[-4000:], data.get("warnings", []), flush=True)
if ok and not args.baseline and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(area)
raise SystemExit(0 if ok else 1)
