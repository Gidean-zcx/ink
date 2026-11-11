"""Compile six private page fixtures. Historical comparisons are manual modes only."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
PRODUCTS = {
    "scan": "system/apps/camera/scan.c",
    "menu": "system/ui/shell/power_menu.c",
    "battery": "system/ui/shell/power_pages.c",
    "off": "system/ui/shell/off_picture.c",
    "interrupt": "system/ui/shell/interrupt_card.c",
    "airplane": "system/ui/shell/airplane_card.c",
}


def compile_driver(build, work, case, baseline):
    product = ROOT / PRODUCTS[case]
    if baseline:
        product = work / f"baseline-{case}.c"
        result = subprocess.run(["git", "show", f"{baseline}:{PRODUCTS[case]}"], cwd=ROOT,
                                capture_output=True, text=True, check=True)
        product.write_text(result.stdout)
    entries = json.loads((build / "compile_commands.json").read_text())
    entry = next(e for e in entries if Path(e["file"]) == ROOT / "tests/qr_scan/device.c")
    command = shlex.split(entry["command"]) if "command" in entry else entry["arguments"][:]
    obj = work / f"{case}.o"
    driver = ROOT / "tests/ui_fix_g" / f"{case}.c"
    command[command.index(entry["file"])] = str(driver)
    command[command.index("-o") + 1] = str(obj)
    command += [f'-DUI_FIX_PRODUCT="{product}"', "-iquote", str((ROOT / PRODUCTS[case]).parent)]
    result = subprocess.run(command, cwd=entry["directory"], capture_output=True, text=True)
    (work / f"compile-{case}.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.stderr)
    link = shlex.split((build / "CMakeFiles/sim-qr-scan.dir/link.txt").read_text())
    link = [str(obj) if item.endswith("tests/qr_scan/device.c.o") else item for item in link]
    binary = work / case
    link[link.index("-o") + 1] = str(binary)
    result = subprocess.run(link, cwd=build, capture_output=True, text=True)
    (work / f"link-{case}.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.stderr)
    return binary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--before", metavar="BASELINE")
    mode.add_argument("--reverse", metavar="BASELINE")
    parser.add_argument("--case", choices=PRODUCTS, action="append")
    parser.add_argument("--screens", action="store_true")
    args = parser.parse_args()
    build = args.build.resolve()
    label = "before" if args.before else "reverse" if args.reverse else "after"
    work = build / "ui-fix-g" / label
    work.mkdir(parents=True, exist_ok=True)
    results = []
    for case in args.case or PRODUCTS:
        binary = compile_driver(build, work, case, args.before or args.reverse)
        variants = ("low", "saving", "charging", "empty", "unsaved") if case == "battery" else ("",)
        for large, variant in ((large, variant) for large in (False, True) for variant in variants):
            name = f'{case}-{variant + "-" if variant else ""}{120 if large else 100}'
            folder = work / name
            if folder.exists():
                shutil.rmtree(folder)
            folder.mkdir(exist_ok=True)
            (folder / "sd").mkdir(exist_ok=True)
            (folder / "synthetic.img").unlink(missing_ok=True)
            script = folder / "run.script"
            script.write_text("@0 net wifi\n@180s end\n")
            env = {**os.environ, "UBSAN_OPTIONS": "halt_on_error=1", "INK_UI_FIX_SCENE": variant}
            if large:
                env["INK_UI_FIX_LARGE"] = "1"
            if args.before:
                env["INK_UI_FIX_BEFORE"] = "1"
            command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
                       "--script", str(script), "--assets", str(build / "assets"),
                       "--content", str(build / "test-content"), "--out", str(folder),
                       "--sd-dir", str(folder / "sd"), "--flash-image", str(folder / "synthetic.img"),
                       "--quiet", "--no-random-outages", "--budget", str(ROOT / "tests/ui_scene.budget.json")]
            try:
                result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=60)
            except subprocess.TimeoutExpired as error:
                result = subprocess.CompletedProcess(command, 124, (error.stdout or b"").decode(),
                                                     (error.stderr or b"").decode() + "\nfixture timed out\n")
            (folder / "run.log").write_text(result.stdout + result.stderr)
            data = json.loads((folder / "metrics.json").read_text()) if (folder / "metrics.json").exists() else {}
            metrics = data.get("metrics", {})
            failures = [w for w in data.get("warnings", []) if "CHECK FAILED" in w]
            row = {"case": name, "exit": result.returncode, "checks": metrics.get("test.pass", 0),
                   "failed": metrics.get("test.fail", 0), "finished": metrics.get("test.finished", 0), "assertions": failures}
            results.append(row)
            print(name, "exit", row["exit"], "checks", row["checks"], "failures", row["failed"], flush=True)
            if row["exit"] or row["failed"]:
                print("\n".join(failures[:12]), result.stderr[-1200:], flush=True)
            if args.screens:
                for source in folder.glob("clean-*.png"):
                    scene = source.stem.removeprefix("clean-")
                    target = ROOT / "docs/tasks/ui-fix-g" / f"{label}-{scene}-{120 if large else 100}.png"
                    with Image.open(source) as image:
                        image.convert("L").save(target, optimize=True)
                    assert target.stat().st_size <= 300 * 1024, target.name
    (work / "summary.json").write_text(json.dumps(results, ensure_ascii=False, indent=2) + "\n")
    print("Evidence:", work.relative_to(ROOT), flush=True)
    if args.reverse:
        # A fix may only affect the large font. Every source must fail in at least one scale.
        okay = all(any(r["case"].startswith(case + "-") and r["finished"] == 1 and r["failed"] > 0
                       and r["assertions"] for r in results) for case in args.case or PRODUCTS)
    else:
        okay = all(r["exit"] == 0 and r["finished"] == 1 and not r["failed"] for r in results)
    return 0 if okay else 1


if __name__ == "__main__":
    raise SystemExit(main())
