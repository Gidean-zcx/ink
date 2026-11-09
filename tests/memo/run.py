"""Run memo persistence, real controls, snapshots and source-only negative controls."""
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
parser.add_argument("--mutant", choices=("member", "write", "failure", "undo", "draft-session", "resume", "checks", "long-item", "final-save"))
parser.add_argument("--visual", action="store_true")
parser.add_argument("--phase", choices=("draft_loss", "retry_edit", "check_edit", "long_line", "burst", "full_snapshot"))
args = parser.parse_args()
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="memo-", dir=build))
binary = build / "sim-memo"
if args.mutant:
    source = (ROOT / "system/apps/memo/memo.c").read_text()
    old, new = {
        "member": ("ink_db_open_member(&MEMOS, ink_cap_call_member(c))", 'ink_db_open_member(&MEMOS, "owner")'),
        "write": ("ink_db_put(db, &wr, NULL)", "(save ? 0 : ink_db_put(db, &wr, NULL))"),
        "failure": ("if (rc->status != INK_RC_DONE && M.field && M.mode == EDIT)",
                    "if (false && rc->status != INK_RC_DONE && M.field && M.mode == EDIT)"),
        "undo": ("rec.ver > undo_ver", "rec.id > undo_ver"),
        "draft-session": ("save_failed(true);", "ink_ui_push(&PAGES[FAIL], (void *)(uintptr_t)M.id);"),
        "resume": ("else if (rc->status == INK_RC_DONE && M.field && M.mode == FAIL)",
                   "else if (false && rc->status == INK_RC_DONE && M.field && M.mode == FAIL)"),
        "checks": ("n->checked = remap_checks(raw, n->body, n->checked)", "(void)remap_checks(raw, n->body, n->checked)"),
        "long-item": ('bool truncated = M.mode == CHECKLIST && strcspn(text, "\\n") > len', "bool truncated = false"),
        "final-save": ("if (M.field && M.mode != FAIL && ink_ui_page_current()", "if (false && M.field && M.mode != FAIL && ink_ui_page_current()"),
    }[args.mutant]
    assert source.count(old) == 1
    source = source.replace(old, new)
    if args.mutant == "member":
        old = "ink_db_open_cached_member(&MEMOS, M.member)"
        assert source.count(old) == 1
        source = source.replace(old, 'ink_db_open_cached_member(&MEMOS, "owner")')
    if args.mutant == "undo":
        source = source.replace("undo_ver = rec.ver", "undo_ver = rec.id")
    target = work / "memo-product.c"
    target.write_text(source)
    test = (ROOT / "tests/memo/memo.c").read_text().replace(
        '#include "../../system/apps/memo/memo.c"', '#include "memo-product.c"').replace(
        '#include "../../system/boot/main.c"', f'#include "{ROOT}/system/boot/main.c"').replace(
        '#include "../integration/whole_product.h"', f'#include "{ROOT}/tests/integration/whole_product.h"')
    driver = work / "memo-test.c"
    driver.write_text(test)
    flags = {}
    for line in (build / "CMakeFiles/sim-memo.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-memo.dir/link.txt").read_text())
    obj = work / "memo-test.o"
    compiled = subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                               "-c", str(driver), "-o", str(obj)], cwd=build)
    if compiled.returncode: raise SystemExit(2)
    link = [str(obj) if word.endswith("tests/memo/memo.c.o") else word for word in link]
    binary = work / "sim-memo"
    link[link.index("-o") + 1] = str(binary)
    if subprocess.run(link, cwd=build).returncode: raise SystemExit(2)

ok = True
seen = set()
runs = [("fail", "MemoSaveFail")] if args.mutant == "failure" else [("write", "Memo"), ("verify", ""), ("fail", "MemoSaveFail")]
reverse_phase = {"draft-session": "draft_loss", "resume": "retry_edit", "checks": "check_edit", "long-item": "long_line", "final-save": "burst"}
if args.mutant in reverse_phase:
    runs = [(reverse_phase[args.mutant], "")]
if not args.mutant:
    runs += [(name, "") for name in ("draft_loss", "retry_edit", "check_edit", "long_line", "burst", "full_snapshot")]
if args.phase:
    runs = [(args.phase, "")]
if args.visual:
    runs += [("write", name) for name in ("MemoDetail", "MemoEdit", "MemoChecklist", "MemoUndo")]
for phase, capture_scene in runs:
    run_name = phase + ("-" + capture_scene if capture_scene else "")
    out = work / run_name
    result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(ROOT / "tests/memo/memo.script"), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"),
        "--flash-image", str(work / ("user.img" if phase in ("write", "verify") and capture_scene in ("Memo", "") else run_name + ".img")),
        "--sd-dir", str(work / "sd"), "--out", str(out), "--quiet", "--no-random-outages"],
        env={**os.environ, "INK_MEMO_PHASE": phase, "INK_MEMO_SCENE": capture_scene}, capture_output=True, text=True, timeout=150)
    (work / f"{run_name}.log").write_text(result.stdout + result.stderr)
    data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
    counts = data.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
    ok &= passed
    print(phase, "PASS" if passed else "FAIL", "exit", result.returncode,
          "checks", counts.get("test.pass", 0), "failures", counts.get("test.fail", 0), flush=True)
    if not passed:
        for warning in data.get("warnings", []): print(warning)
        print(result.stderr[-1600:])
        break
    for line in (out / "snapshots.ndjson").read_text().splitlines() if (out / "snapshots.ndjson").exists() else []:
        item = json.loads(line)
        scene = item["scene"]
        if scene in {"Memo", "MemoDetail", "MemoEdit", "MemoChecklist", "MemoUndo", "MemoSaveFail"}: seen.add(scene)
        assert item["snapshot"]
        if scene != capture_scene: continue
        target = build / "memo-compare" / scene
        target.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(out / "snap-scene.png", target / "snap-scene.png")
        shutil.copyfile(out / "metrics.json", target / "metrics.json")
if ok and not args.mutant and not args.phase:
    assert seen == {"Memo", "MemoDetail", "MemoEdit", "MemoChecklist", "MemoUndo", "MemoSaveFail"}, seen
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT), flush=True)
raise SystemExit(0 if ok else 1)
