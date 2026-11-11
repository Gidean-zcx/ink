"""Manual screenshots and baseline controls; regular ctest never needs Git history."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DEST = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--baseline", action="store_true")
args = parser.parse_args()
build = args.build.resolve()
stage = "before" if args.baseline else "after"
area = build / "test-out" / f"polish-1-{stage}"
area.mkdir(parents=True, exist_ok=True)
baseline = "2587faa2fca29425bf913127c3e6914daa576e8e"
entries = json.loads((build / "compile_commands.json").read_text())
checks = {
    "ime-keyboard1": ("tests/ime_keyboard1.c", "system/ui/shell/keyboard.c", "../system/ui/shell/keyboard.c",
                      {"WifiPassword-normal": "Password-100", "WifiPassword-large": "Password-120"},
                      ["password keyboard removes", "password hides candidates", "password symbols retain"]),
    "sms": ("tests/sms/sms.c", "system/apps/sms/sms.c", "../../system/apps/sms/sms.c",
            {"SmsThread-normal": "SmsThread-100", "SmsThread-large": "SmsThread-120"},
            ["local date label", "adjacent gap", "SMS keyboard enter key visibly says Send", "keyboard enter sends exactly once"]),
}
summaries = []
for target, (test, product, include, shots, expected) in checks.items():
    case = area / target
    case.mkdir(parents=True, exist_ok=True)
    binary = build / f"sim-{target}"
    if args.baseline:
        original = subprocess.check_output(["git", "show", f"{baseline}:{product}"], cwd=ROOT, text=True)
        source = (ROOT / test).read_text().replace(f'#include "{include}"', original)
        entry = next(e for e in entries if Path(e["file"]) == ROOT / test)
        command = shlex.split(entry["command"])
        obj = case / "baseline.o"
        command[command.index("-o") + 1] = str(obj)
        command[command.index(str(ROOT / test))] = "-"
        command += ["-iquote", str((ROOT / test).parent), "-x", "c"]
        # -x must precede stdin for both clang and GCC.
        command.remove("-")
        command.append("-")
        subprocess.run(command, input=source, cwd=entry["directory"], text=True, check=True, capture_output=True)
        link = shlex.split((build / f"CMakeFiles/sim-{target}.dir/link.txt").read_text())
        link = [str(obj) if part.endswith(test + ".o") else part for part in link]
        binary = case / "sim-baseline"
        link[link.index("-o") + 1] = str(binary)
        subprocess.run(link, cwd=build, check=True, capture_output=True)
    out = case / "run"
    shutil.rmtree(out, ignore_errors=True)
    state = case / "state"
    shutil.rmtree(state, ignore_errors=True)
    (state / "sd").mkdir(parents=True)
    script = ROOT / ("tests/ime_keyboard1.script" if target == "ime-keyboard1" else "tests/sms/sms.script")
    command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
               "--script", str(script), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
               "--assets", str(build / "assets"), "--content", str(build / "test-content"),
               "--flash-image", str(state / "user.img"), "--sd-dir", str(state / "sd"),
               "--out", str(out), "--quiet", "--no-random-outages"]
    result = subprocess.run(command, env=dict(os.environ, INK_SMS_PHASE="polish"),
                            cwd=ROOT, capture_output=True, text=True, timeout=150)
    (case / "process.log").write_text(result.stdout + result.stderr)
    data = json.loads((out / "metrics.json").read_text())
    metrics = data["metrics"]
    warnings = "\n".join(data.get("warnings", []))
    passed = metrics.get("test.finished") == 1
    if args.baseline:
        passed = passed and result.returncode == 1 and metrics.get("test.fail", 0) > 0 and all(s in warnings for s in expected)
    else:
        passed = passed and result.returncode == 0 and metrics.get("test.fail", 0) == 0 and not warnings
    summary = {"stage": stage, "target": target, "exit": result.returncode,
               "checks": metrics.get("test.pass", 0), "failures": metrics.get("test.fail", 0), "accepted": passed}
    summaries.append(summary)
    print(json.dumps(summary), flush=True)
    if not passed:
        print(warnings, result.stderr[-3000:])
        raise SystemExit(1)
    (DEST / "screenshots").mkdir(exist_ok=True)
    for source, name in shots.items():
        picture = out / f"snap-{source}.png"
        if picture.stat().st_size > 300_000:
            raise SystemExit(f"screenshot exceeds 300 KB: {picture.name}")
        shutil.copyfile(picture, DEST / "screenshots" / f"{stage}-{name}.png")
(area / "summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
