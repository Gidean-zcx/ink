"""File-page persistence and source-only negative controls; artifacts stay in build."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--visual", action="store_true")
p.add_argument("--trace", action="store_true")
p.add_argument("--mutant", choices=("child", "browse", "thumb", "rename", "paging", "preview", "ai-panel", "up", "long-snapshot", "notice", "index-redraw", "memo-cache"))
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix="files-app-", dir=build))
binary = build / "sim-files-app"
if a.mutant:
    source = (ROOT / "system/apps/files/files.c").read_text()
    changes = {
        "child": [("if (ink_member_current()->role == INK_ROLE_ADULT) for", "if (true) for")],
        "browse": [("if (!allowed || (F.mode == MOVE && !writable(path)))", "if (false && (!allowed || (F.mode == MOVE && !writable(path))))")],
        "thumb": [("lv_image_set_src(F.images[at], &F.thumbs[at]);", "(void)F.thumbs[at];")],
        "rename": [('if (ink_cap_call(a->capability, NULL, &opts))', 'if (strcmp(a->id, "rename") && ink_cap_call(a->capability, NULL, &opts))')],
        "paging": [("ink_ui_list_turn(F.list, !strcmp(id, \"files.next\") ? 1 : -1)", "(false)")],
        "preview": [("lv_label_set_text(F.preview, F.preview_text)", "lv_label_set_text(F.preview, \"\")")],
        "ai-panel": [("strcmp(ink_ui_action_page(), PAGES[F.mode].name)", "strcmp(ink_ui_top_name(), PAGES[F.mode].name)")],
        "up": [("end != path && !member_root", "end != path && end != path + 8 && (member_root || true)")],
        "long-snapshot": [
            ('char ids[4][32]', 'char ids[4][INK_FS_PATH_MAX + 8]'),
            ('"dir:%zu", index', '"dir:%s", item->path'),
            ('.label = item->dir ? F.names[at] : item->name', '.label = item->name'),
            ('strcmp(F.names[at], item->name) ? "文件夹（名称已截断）" : "文件夹"', '"文件夹"'),
        ],
        "notice": [("notice_y = INK_CANVAS_H - INK_MARGIN - 2 * INK_PRIMARY - INK_GAP - 2 * ink_ui_line_height(INK_FONT_NOTE) - INK_GAP_L;", "(void)notice_y;")],
        "index-redraw": [('    if (F.mode == HOME && !strcmp(kind, "indexed")) return; /* Roots do not depend on the index; keep queued taps valid. */\n', '')],
        "memo-cache": [],
    }[a.mutant]
    for old, new in changes:
        assert source.count(old) == 1, old
        source = source.replace(old, new)
    (work / "files-product.c").write_text(source)
    driver = (ROOT / "tests/files_app/files_app.c").read_text().replace(
        '#include "../../system/apps/files/files.c"', '#include "files-product.c"').replace(
        '#include "../../system/boot/main.c"', f'#include "{ROOT}/system/boot/main.c"').replace(
        '#include "../integration/whole_product.h"', f'#include "{ROOT}/tests/integration/whole_product.h"')
    (work / "files-test.c").write_text(driver)
    flags = {}
    for line in (build / "CMakeFiles/sim-files-app.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-files-app.dir/link.txt").read_text())
    obj = work / "files-test.o"
    subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                    "-c", str(work / "files-test.c"), "-o", str(obj)], cwd=build, check=True)
    link = [str(obj) if s.endswith("tests/files_app/files_app.c.o") else s for s in link]
    if a.mutant == "memo-cache":
        memo = (ROOT / "system/apps/memo/memo.c").read_text()
        for call in ("ink_db_open_member(&MEMOS, ink_cap_call_member(c))", "ink_db_open_member(&MEMOS, who)"):
            assert memo.count(call) == 1, call
            memo = memo.replace(call, call.replace("ink_db_open_member", "ink_db_open_cached_member"))
        (work / "memo-product.c").write_text(memo)
        memo_obj = work / "memo-product.o"
        subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                        "-c", str(work / "memo-product.c"), "-o", str(memo_obj)], cwd=build, check=True)
        link.insert(1, str(memo_obj))
    binary = work / "sim-files-app"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)

runs = [("write", ""), ("verify", ""), ("review", ""), ("cold-save", "")]
if a.mutant:
    runs = [("cold-save" if a.mutant == "memo-cache" else "review" if a.mutant in ("up", "long-snapshot", "notice") else "write", "")]
if a.visual:
    runs += [("write", name) for name in ("Files", "FileList", "FileActions", "FileMove")]
    runs += [("review", name) for name in ("FileActionsPreview", "FileListLong", "FileMoveLong")]
ok = True
seen = set()
summary = []
for phase, scene in runs:
    name = phase + ("-" + scene if scene else "")
    out = work / name
    scope = scene or (phase if phase in ("review", "cold-save") else "persistence")
    result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(ROOT / "tests/files_app/files_app.script"), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"),
        "--flash-image", str(work / (scope + ".img")), "--sd-dir", str(work / (scope + "-sd")),
        "--out", str(out), "--quiet", "--no-random-outages", *(["--trace"] if a.trace else [])],
        env={**os.environ, "INK_FILES_PHASE": phase, "INK_FILES_SCENE": scene}, capture_output=True, text=True, timeout=180)
    (work / f"{name}.log").write_text(result.stdout + result.stderr)
    data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
    counts = data.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
    summary.append({"phase": name, "exit": result.returncode, "passed": passed, "checks": counts.get("test.pass", 0),
                    "failures": counts.get("test.fail", 0), "finished": counts.get("test.finished", 0), "warnings": data.get("warnings", [])})
    print(name, "PASS" if passed else "FAIL", "exit", result.returncode, "checks", counts.get("test.pass", 0),
          "failures", counts.get("test.fail", 0), "finished", counts.get("test.finished", 0), flush=True)
    ok &= passed
    if not passed:
        print(result.stderr[-2200:])
        break
    for line in (out / "snapshots.ndjson").read_text().splitlines() if (out / "snapshots.ndjson").exists() else []:
        snap = json.loads(line)
        seen.add(snap["scene"])
        assert snap["snapshot"], snap
        if snap["scene"] in ("FileListLong", "FileMoveLong"):
            page = snap["snapshot"]
            fields = page["fields"]
            assert len(fields) == 4 and len({f["id"] for f in fields}) == 4, fields
            for field in fields:
                assert field["label"].endswith("…") and "名称已截断" in field["value"], field
                assert len(field["id"]) < 24 and field["object"]["id"] == field["id"], field
            paths = {json.loads(action["args_json"])["path"] for action in page["actions"] if action["capability"] == "files.browse"}
            expected = {"/me/SYNTH_资料目录/SYNTH_" + "测" * 17 + str(i) for i in range(4)}
            assert paths == expected, paths
    if scene:
        target = build / "files-app-compare" / scene
        target.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(out / "snap-scene.png", target / "snap-scene.png")
        shutil.copyfile(out / "metrics.json", target / "metrics.json")
if ok and not a.mutant:
    assert seen == {"Files", "FileList", "FileActions", "FileMove", "FileActionsPreview", "FileListLong", "FileMoveLong"}, seen
(work / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT), flush=True)
raise SystemExit(0 if ok else 1)
