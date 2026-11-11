"""Run synthetic acceptance cases; optional source mutations are manual reverse checks."""
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
p.add_argument("--case")
p.add_argument("--mutant")
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix="ai-history-photo-", dir=build))
binary = build / "sim-ai-history-photo"
mutations = {
    "write": ("ai_history", "if (ink_db_put(db, &w, NULL))", "if (0)"),
    "limit": ("ai_history", ".max_records = 50", ".max_records = 51"),
    "bytes": ("ai_history", ".max_bytes = 128 * 1024", ".max_bytes = 512 * 1024"),
    "order": ("ai_history", "ink_db_cursor_t cur = {.desc = true};", "ink_db_cursor_t cur = {.desc = false};"),
    "paging": ("ai_history", "if (offset-- > 0) continue;", "if (false) continue;"),
    "member": ("ai_history", "ink_db_open_member(&HISTORY, ink_cap_call_member(c))", 'ink_db_open_member(&HISTORY, "owner")'),
    "clear": ("ai_history", ".ask_by_hand = true, .effect = INK_FX_LOCAL_DESTRUCTIVE", ".ask_by_hand = false, .effect = INK_FX_LOCAL_WRITE"),
    "delete": ("ai_history", "else if (ink_db_del(db, (uint32_t)id, c->src))", "else if (1)"),
    "replay": ("ai_service", 'ink_hub_call_for("ai.replay", params,', 'ink_hub_call_for("ai.missing", params,'),
    "offline": ("ai_service", "&& ink_hub_online();", ";"),
    "photo": ("ai_service", "if (g_photo) {", "if (false && g_photo) {"),
    "photo-member": ("gallery", "p->epoch == ink_member_epoch() && p->generation", "true && p->generation"),
    "photo-changed": ("gallery", "p->revision && p->revision == ink_fs_revision()", "true"),
    "photo-id": ("gallery", "p->photo == G.photo", "true"),
    "photo-display": ("gallery", " || G.revision != ink_fs_revision()", ""),
    "photo-switch": ("gallery", 'if (p->epoch == ink_member_epoch() && p->generation == ink_ui_generation() && p->photo == G.photo &&\n        p->revision && p->revision == ink_fs_revision() && ink_ui_page_current() && !strcmp(ink_ui_page_name(), "Photo") && rc->status == INK_RC_DONE && rc->result)', 'if (rc->status == INK_RC_DONE && rc->result)'),
}
if a.mutant:
    name, old, new = mutations[a.mutant]
    paths = {"ai_history": "system/services/ai/ai_history.c", "ai_service": "system/services/ai/ai_service.c", "gallery": "system/apps/gallery/gallery.c"}
    source = (ROOT / paths[name]).read_text()
    assert source.count(old) >= 1, (name, old)
    target = work / f"{name}.c"
    target.write_text(source.replace(old, new))
    driver = (ROOT / "tests/ai_history_photo/test.c").read_text()
    for relative in [*paths.values(), "system/boot/main.c", "tests/integration/whole_product.h"]:
        driver = driver.replace(f'"../../{relative}"', f'"{target if relative == paths[name] else ROOT / relative}"')
    driver = driver.replace('"../integration/whole_product.h"', f'"{ROOT}/tests/integration/whole_product.h"')
    test = work / "driver.c"
    test.write_text(driver)
    flags = {}
    for line in (build / "CMakeFiles/sim-ai-history-photo.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-ai-history-photo.dir/link.txt").read_text())
    obj = work / "driver.o"
    subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                    "-I", str(ROOT / "system/apps/gallery"), "-Wno-unused-variable", "-c", str(test), "-o", str(obj)], cwd=build, check=True)
    link = [str(obj) if v.endswith("tests/ai_history_photo/test.c.o") else v for v in link]
    binary = work / "mutant"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)

hub = work / "hub.json"
hub.write_text(json.dumps({"rules": [
    {"method": "hello", "think_ms": 20, "result": {"proto": 1, "session": "synthetic-history"}},
    {"method": "ai.ask", "think_ms": 300, "stream": {"notify": "ai.delta", "interval_ms": 20, "chunks": ["合成回答。"]}, "result": {"done": True}},
    {"method": "*", "think_ms": 20, "result": {}}
]}, ensure_ascii=False))
script = work / "run.script"
script.write_text("@0 net wifi\n@300s end\n")
cases = [a.case] if a.case else ["history", "restart", "replay", "photo", "photo-member", "photo-changed", "photo-id", "photo-switch"]
ok = True
for case in cases:
    folder = work / case
    result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(script), "--hub", str(hub), "--budget", str(ROOT / "tests/ui_scene.budget.json"), "--assets", str(build / "assets"),
        "--content", str(build / "test-content"), "--flash-image", str(work / ("history.img" if case in ("history", "restart") else case + ".img")),
        "--out", str(folder), "--quiet", "--no-random-outages"],
        env={**os.environ, "INK_HISTORY_CASE": case}, capture_output=True, text=True, timeout=150)
    (work / f"{case}.log").write_text(result.stdout + result.stderr)
    data = json.loads((folder / "metrics.json").read_text()) if (folder / "metrics.json").exists() else {}
    metrics = data.get("metrics", {})
    passed = result.returncode == 0 and metrics.get("test.finished") == 1 and not metrics.get("test.fail", 0)
    print(case, "PASS" if passed else "FAIL", "exit", result.returncode, "checks", metrics.get("test.pass", 0), "failures", metrics.get("test.fail", 0), flush=True)
    if not passed:
        print(result.stderr[-2500:])
        print(data.get("warnings", [])[-20:])
        ok = False
        break
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT), flush=True)
raise SystemExit(0 if ok else 1)
