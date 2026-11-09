"""Run bounded AI page scenarios, plus source-only negative controls."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--mutant", choices=("errors", "member", "receipts", "snapshot", "hide", "lost", "load", "recover", "wrap", "undo"))
parser.add_argument("--case", choices=("offline", "timeout", "budget", "records", "hide", "lost", "newmember", "loadfail", "real"))
args = parser.parse_args()
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="ai-pages-", dir=build))
binary = build / "sim-ai-pages"
if args.mutant:
    source = (ROOT / "system/ui/shell/ai_pages.c").read_text()
    old, new = {
        "errors": ("if (kind < 0) return;", "return;"),
        "member": ('ink_db_find_def("activity.log"), ink_member_current()->id', 'ink_db_find_def("activity.log"), "owner"'),
        "receipts": ("return words[i];", "return words[0];"),
        "snapshot": ("return ink_ui_semantics_snapshot(&E.page, out, cap);", "return -1;"),
        "hide": ("overlay->name = BOARDS[kind]; overlay->level = 0; overlay->snapshot = error_snapshot;",
                 "static ink_ui_overlay_t orphan; orphan = *overlay; orphan.name = BOARDS[kind]; orphan.level = 0; orphan.snapshot = error_snapshot; ink_ui_overlay_add(&orphan, E.root);"),
        "lost": (" || lost ? 0 :", " ? 0 :"),
        "load": ('if (ink_cap_call("activity.list", NULL, &opts))', 'if ((void)opts, 1)'),
        "recover": ("if (L.list && !L.db)", "if (false)"),
        "wrap": ("(L.receipts ? ink_ui_label : ink_ui_line)", "ink_ui_line"),
        "undo": ("bool undo = L.receipts &&", "bool undo = false && L.receipts &&"),
    }[args.mutant]
    assert source.count(old) == ({"member": 3, "wrap": 2}.get(args.mutant, 1))
    source = source.replace(old, new)
    target = work / "ai-pages-product.c"
    target.write_text(source)
    flags = {}
    for line in (build / "CMakeFiles/ink_system.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-ai-pages.dir/link.txt").read_text())
    obj = work / "ai-pages-product.o"
    result = subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                             "-c", str(target), "-o", str(obj)], cwd=build)
    if result.returncode: raise SystemExit(2)
    link.insert(1, str(obj))
    binary = work / "sim-ai-pages"
    link[link.index("-o") + 1] = str(binary)
    if subprocess.run(link, cwd=build).returncode: raise SystemExit(2)

cases = ["offline", "timeout", "budget", "records", "hide", "lost", "newmember", "loadfail", "real"]
if args.mutant: cases = [{"errors": "offline", "snapshot": "offline", "member": "records", "receipts": "records", "hide": "hide", "lost": "lost", "load": "newmember", "recover": "loadfail", "wrap": "real", "undo": "real"}[args.mutant]]
if args.case: cases = [args.case]
ok = True
for case in cases:
    folder = work / case
    folder.mkdir()
    script = folder / "case.script"
    script.write_text(("@0 net none\n" if case in ("offline", "hide") else "@0 net wifi\n") +
                      ("@10s net none\n" if case == "lost" else "") + "@150s end\n")
    hub = folder / "hub.json"
    hub.write_text(json.dumps({"rules": [
        {"method": "hello", "think_ms": 20, "result": {"session": "synthetic-ai-pages", "proto": 1}},
        {"method": "ai.ask", "think_ms": 60000 if case in ("timeout", "lost") else 20,
         "error": {"code": -32000, "message": "synthetic budget response", "data": {"code": "budget"}}},
        {"method": "*", "think_ms": 10, "result": {}}
    ]}))
    result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(script), "--hub", str(hub), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"),
        "--out", str(folder), "--quiet", "--no-random-outages"],
        env={**os.environ, "INK_AI_PAGES_CASE": case}, capture_output=True, text=True, timeout=180)
    (folder / "run.log").write_text(result.stdout + result.stderr)
    data = json.loads((folder / "metrics.json").read_text()) if (folder / "metrics.json").exists() else {}
    counts = data.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
    ok &= passed
    print(case, "PASS" if passed else "FAIL", "exit", result.returncode, "checks", counts.get("test.pass", 0),
          "failures", counts.get("test.fail", 0), flush=True)
    if not passed:
        for warning in data.get("warnings", []): print(warning)
        print(result.stderr[-2000:])
        break
    for scene in ("AiOffline", "AiTimeout", "AiBudget", "AiLog", "AiReceipts"):
        snap = folder / scene / "snapshot.json"
        if not snap.exists(): continue
        model = json.loads(snap.read_text())
        assert model["page"] == scene and model["fields"] and model["actions"]
        assert counts[f"test.snapshot.{scene}"] == 1
        target = build / "ai-pages-compare" / scene
        target.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(folder / f"snap-{scene}.png", target / "snap-scene.png")
        proof = {"test.snapshot.settled": counts[f"test.snapshot.{scene}"], f"test.snapshot.{scene}": counts[f"test.snapshot.{scene}"],
                 "test.finished": counts["test.finished"], "test.fail": counts.get("test.fail", 0)}
        (target / "metrics.json").write_text(json.dumps({"metrics": proof}) + "\n")
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT), flush=True)
raise SystemExit(0 if ok else 1)
