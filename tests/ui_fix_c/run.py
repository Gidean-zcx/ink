"""Bounded UI checks using existing build flags; history is used only for manual comparisons."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
PRODUCTS = {
    "panel": "system/ui/shell/ai_panel.c",
    "records": "system/ui/shell/ai_pages.c",
    "gallery": "system/apps/gallery/gallery.c",
}
KEEP_SCENES = {"panel", "panel-speech", "records", "receipt-expired", "gallery", "photo"}


def compile_driver(build, work, case, baseline):
    product = ROOT / PRODUCTS[case]
    if baseline:
        product = work / f"baseline-{case}.c"
        result = subprocess.run(["git", "show", f"{baseline}:{PRODUCTS[case]}"], cwd=ROOT,
                                capture_output=True, text=True, check=True)
        product.write_text(result.stdout)
    entries = json.loads((build / "compile_commands.json").read_text())
    entry = next(e for e in entries if Path(e["file"]) == ROOT / "tests/ai_pages.c")
    command = shlex.split(entry["command"]) if "command" in entry else entry["arguments"][:]
    obj = work / f"{case}.o"
    driver = ROOT / "tests/ui_fix_c" / f"{case}.c"
    command[command.index(entry["file"])] = str(driver)
    command[command.index("-o") + 1] = str(obj)
    command += [f'-DUI_FIX_PRODUCT="{product}"', "-iquote", str((ROOT / PRODUCTS[case]).parent)]
    result = subprocess.run(command, cwd=entry["directory"], capture_output=True, text=True)
    (work / f"compile-{case}.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.stderr)
    link = shlex.split((build / "CMakeFiles/sim-ai-pages.dir/link.txt").read_text())
    link = [str(obj) if item.endswith("tests/ai_pages.c.o") else item for item in link]
    binary = work / case
    link[link.index("-o") + 1] = str(binary)
    result = subprocess.run(link, cwd=build, capture_output=True, text=True)
    (work / f"link-{case}.log").write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.stderr)
    return binary


def fixtures(sd):
    folder = sd / "content/.m/owner/照片"
    folder.mkdir(parents=True)
    image = Image.new("RGB", (256, 256), "white")
    draw = ImageDraw.Draw(image)
    for y in range(0, 256, 32):
        for x in range(0, 256, 32):
            if (x // 32 + y // 32) % 2:
                draw.rectangle((x, y, x + 31, y + 31), fill="black")
    image.save(folder / "SYNTH-checker.png")
    image = Image.new("RGB", (256, 256), "white")
    draw = ImageDraw.Draw(image)
    draw.polygon([(30, 100), (150, 100), (150, 40), (230, 128), (150, 216), (150, 156), (30, 156)], fill="black")
    image.save(folder / "SYNTH-arrow.png")


def run_case(build, work, binary, case, large, before, empty):
    name = f'{case}{"-empty" if empty else ""}-{120 if large else 100}'
    folder = work / name
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir()
    sd = folder / "sd"
    sd.mkdir()
    if case == "gallery" and not empty:
        fixtures(sd)
    script = folder / "run.script"
    script.write_text("@0 net wifi\n@180s end\n")
    hub = folder / "hub.json"
    rules = [
        {"method": "hello", "think_ms": 20, "result": {"session": "synthetic-ui-fix-c", "proto": 1}},
        {"method": "ai.ask", "think_ms": 200, **(
            {"error": {"code": -32000, "message": "synthetic budget", "data": {"code": "budget"}}}
            if case == "records" else
            {"stream": {"notify": "ai.delta", "interval_ms": 30, "chunks": ["合成回答。"]}, "result": {"done": True, "source": "page"}})},
        {"method": "ai.listen", "think_ms": 1200,
         "listen": {"stt_ms": 300, "transcript": "合成语音问题", "tts_first_ms": 300, "tts_chars_per_s": 3, "tts_speed": 2},
         "stream": {"notify": "ai.delta", "interval_ms": 100, "chunks": ["合成语音回答用于检查停止朗读按钮与文字保留。"]}, "result": {"done": True}},
        {"method": "*", "think_ms": 20, "result": {}},
    ]
    hub.write_text(json.dumps({"rules": rules}, ensure_ascii=False))
    env = {**os.environ}
    if large:
        env["INK_UI_FIX_LARGE"] = "1"
    if before:
        env["INK_UI_FIX_BEFORE"] = "1"
    command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
               "--script", str(script), "--hub", str(hub), "--assets", str(build / "assets"),
               "--content", str(build / "test-content"), "--out", str(folder), "--sd-dir", str(sd),
               "--flash-image", str(folder / "synthetic.img"), "--quiet", "--no-random-outages",
               "--budget", str(ROOT / "tests/ui_scene.budget.json")]
    result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=180)
    (folder / "run.log").write_text(result.stdout + result.stderr)
    for snapshot in folder.glob("*.json"):
        json.loads(snapshot.read_text())
    data = json.loads((folder / "metrics.json").read_text()) if (folder / "metrics.json").exists() else {}
    metrics = data.get("metrics", {})
    failures = [str(w) for w in data.get("warnings", []) if "CHECK FAILED" in str(w)]
    passed = result.returncode == 0 and metrics.get("test.finished") == 1 and not metrics.get("test.fail", 0)
    row = {"case": name, "exit": result.returncode, "pass": passed, "checks": metrics.get("test.pass", 0),
           "failed": metrics.get("test.fail", 0), "finished": metrics.get("test.finished", 0), "assertions": failures}
    print(name, "PASS" if passed else "FAIL", "exit", result.returncode,
          "checks", row["checks"], "failures", row["failed"], flush=True)
    if not passed:
        print("\n".join(failures[:12]), result.stderr[-1600:], flush=True)
    return row, folder


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--before", metavar="BASELINE")
    mode.add_argument("--reverse", metavar="BASELINE")
    parser.add_argument("--case", choices=tuple(PRODUCTS))
    parser.add_argument("--screens", action="store_true", help="copy bounded PNG evidence into the task directory")
    args = parser.parse_args()
    build = args.build.resolve()
    label = "before" if args.before else "reverse" if args.reverse else "after"
    work = build / "ui-fix-c" / label
    work.mkdir(parents=True, exist_ok=True)
    results = []
    for case in [args.case] if args.case else PRODUCTS:
        binary = compile_driver(build, work, case, args.before or args.reverse)
        for large in (False, True):
            row, folder = run_case(build, work, binary, case, large, bool(args.before), False)
            results.append(row)
            if args.screens:
                for source in folder.glob("clean-*.png"):
                    scene = source.stem.removeprefix("clean-")
                    if scene not in KEEP_SCENES:
                        continue
                    target = ROOT / "docs/tasks/ui-fix-c" / f"{label}-{scene}-{120 if large else 100}.png"
                    with Image.open(source) as image:
                        image.convert("L").save(target, optimize=True)
                    assert target.stat().st_size < 300 * 1024, target.name
            if case == "gallery" and not args.reverse:
                empty, _ = run_case(build, work, binary, case, large, bool(args.before), True)
                results.append(empty)
    summary = work / "summary.json"
    summary.write_text(json.dumps(results, ensure_ascii=False, indent=2) + "\n")
    print("Evidence:", work.relative_to(ROOT), flush=True)
    if args.reverse:
        ok = all(r["finished"] == 1 and r["failed"] > 0 and r["assertions"] for r in results)
    else:
        ok = all(r["pass"] for r in results)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
