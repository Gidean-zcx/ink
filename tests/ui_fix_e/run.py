"""Run UX09/UX15/UX18 checks and optional manual baseline screenshot/negative controls."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
p.add_argument("--baseline", help="manual control only; requires this local git revision")
p.add_argument("--capture", action="store_true", help="capture without fix-specific assertions")
p.add_argument("--large", action="store_true")
p.add_argument("--export", type=Path)
a = p.parse_args()
build = a.build.resolve()
tag = ("before" if a.baseline else "after") + ("-120" if a.large else "-100") + ("-capture" if a.capture else "-check")
work = build / "ui-fix-e" / tag
work.mkdir(parents=True, exist_ok=True)
flags = {}
for line in (build / "CMakeFiles/sim-settings-b.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
link = shlex.split((build / "CMakeFiles/sim-settings-b.dir/link.txt").read_text())
def compile_source(source, obj):
    subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                    "-I", str(build / "generated"), "-I", str(ROOT / "system/ui/shell"),
                    "-c", str(source), "-o", str(obj)], cwd=build, check=True)
obj = work / "test.o"
compile_source(ROOT / "tests/ui_fix_e/test.c", obj)
link = [str(obj) if item.endswith("tests/settings_b.c.o") else item for item in link]
if a.baseline:
    for relative in ("system/ui/shell/settings_family.c", "system/ui/shell/ai_chat.c", "system/apps/files/files.c"):
        source = work / Path(relative).name
        source.write_bytes(subprocess.check_output(["git", "show", f"{a.baseline}:{relative}"], cwd=ROOT))
        baseline_obj = source.with_suffix(".o")
        compile_source(source, baseline_obj)
        link.insert(1, str(baseline_obj))
relative = "system/ui/file_picker.c"
source = work / "picker.c"
picker_source = subprocess.check_output(["git", "show", f"{a.baseline}:{relative}"], cwd=ROOT) if a.baseline else (ROOT / relative).read_bytes()
source.write_bytes(b"#define ink_fs_index ui_fix_e_index\n" + picker_source)
picker_obj = work / "picker.o"
compile_source(source, picker_obj)
link.insert(1, str(picker_obj))
binary = work / "sim-ui-fix-e"
link[link.index("-o") + 1] = str(binary)
subprocess.run(link, cwd=build, check=True)
script = work / "case.script"
script.write_text("@0 net wifi\n@300s end\n")
out = work / "out"
if out.exists():
    shutil.rmtree(out)
env = {**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"}
if not a.capture:
    env["INK_UI_FIX_E_VERIFY"] = "1"
if a.large:
    env["INK_UI_FIX_E_LARGE"] = "1"
r = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
                    "--script", str(script), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
                    "--assets", str(build / "assets"), "--content", str(build / "test-content"),
                    "--out", str(out), "--quiet", "--no-random-outages"], cwd=ROOT, env=env,
                   capture_output=True, text=True, timeout=180)
(work / "run.log").write_text(r.stdout + r.stderr)
data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
counts = data.get("metrics", {})
summary = {"variant": tag, "exit": r.returncode, "finished": counts.get("test.finished", 0),
           "passed": counts.get("test.pass", 0), "failed": counts.get("test.fail", 0),
           "failures": [s for s in data.get("warnings", []) if "CHECK FAILED" in s]}
(work / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(summary, ensure_ascii=False, indent=2), flush=True)
if a.export:
    from PIL import Image
    target = a.export.resolve()
    target.mkdir(parents=True, exist_ok=True)
    for image in out.glob("snap-*.png"):
        dest = target / (tag.split("-capture")[0].split("-check")[0] + "-" + image.name[5:])
        Image.open(image).save(dest, optimize=True)
        assert dest.stat().st_size <= 300 * 1024, dest.name
if a.baseline and not a.capture:
    failures = "\n".join(summary["failures"])
    assert r.returncode != 0 and counts.get("test.finished") == 1 and all(key in failures for key in ("UX09", "UX15", "UX18")), summary
    print("PASS: removing all four UI fixes fails UX09, UX15 and UX18", flush=True)
else:
    assert r.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0), summary
