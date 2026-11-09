"""Run two processes on one simulated flash; optionally break one example contract."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--mutant", choices=("source", "state", "snapshot", "child", "loading"))
args = parser.parse_args()
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="jot-", dir=build))
binary = build / "sim-jot"
if args.mutant:
    source = (ROOT / "examples/jot/jot.c").read_text()
    before, after = {
        "loading": ("    if (!ink_jot_list) { c->rc.status = INK_RC_FAILED; return; }\n", ""),
        "source": (".len = json.len, .src = c->src", ".len = json.len, .src = INK_SRC_TOUCH"),
        "state": ("*(uint8_t *)buf = (uint8_t)ink_jot_position.page;\n    return 1;",
                  "*(uint8_t *)buf = (uint8_t)ink_jot_position.page;\n    return 0;"),
        "snapshot": (".value = ink_jot_texts[at]", '.value = "合成错误快照"'),
        "child": ('.id = "jot.save", .title = "保存这一笔", .effect = INK_FX_LOCAL_WRITE, .child_ok = true',
                  '.id = "jot.save", .title = "保存这一笔", .effect = INK_FX_LOCAL_WRITE, .child_ok = false'),
    }[args.mutant]
    assert source.count(before) == 1
    if args.mutant == "snapshot":
        # Change only the semantic value, retaining the actual label text.
        source = source.replace("lv_label_set_text(lv_obj_get_child(slot, 0), field->value);",
                                "lv_label_set_text(lv_obj_get_child(slot, 0), ink_jot_texts[at]);")
    target = work / "jot.c"
    target.write_text(source.replace(before, after))
    flags = {}
    for line in (build / "CMakeFiles/sim-jot.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-jot.dir/link.txt").read_text())
    obj = work / "jot.o"
    subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                    "-I" + str(ROOT / "examples/jot"), "-c", str(target), "-o", str(obj)], cwd=build, check=True)
    old = next(i for i, item in enumerate(link) if item.endswith("examples/jot/jot.c.o"))
    link[old] = str(obj)
    binary = work / "sim-jot"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)

(work / "sd").mkdir()
ok = True
for phase in ("write", "recover"):
    out = work / phase
    command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
               "--assets", str(build / "assets"), "--content", str(build / "test-content"),
               "--script", str(ROOT / "tests/jot/app.script"), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
               "--out", str(out), "--quiet", "--no-random-outages", "--flash-image", str(work / "user.img"),
               "--sd-dir", str(work / "sd")]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=150,
                            env={**os.environ, "INK_JOT_PHASE": phase, "UBSAN_OPTIONS": "halt_on_error=1"})
    (work / (phase + ".log")).write_text(result.stdout + result.stderr)
    data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
    counts = data.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
    ok &= passed
    print(phase, "PASS" if passed else "FAIL", "exit", result.returncode,
          "checks", counts.get("test.pass", 0), "failures", counts.get("test.fail", 0), flush=True)
    if not passed:
        print(data.get("warnings", []))
        print(result.stderr[-4000:])
        break
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT))
raise SystemExit(0 if ok else 1)
