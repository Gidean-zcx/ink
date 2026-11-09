"""Undo one clock fix at a time in separate objects, retaining all assertions."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--variant", default="all", choices=("all", "input-snapshot", "editing", "toggle", "input-error", "input-bytes", "minus", "list-labels", "persistence"))
a = p.parse_args()
build = a.build.resolve()
work = build / "clock-negative"
work.mkdir(exist_ok=True)
flags = {}
for line in (build / "CMakeFiles/ink_system.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
variants = {
    "input-snapshot": ("if (C.name && ink_text_focused()) EDIT.action_count = 0;", "if (C.name && ink_text_focused()) EDIT.action_count = actions;"),
    "editing": ("static void draft(ink_call_t *c) {", "static void draft(ink_call_t *c) { if (C.draft.id) return;"),
    "toggle": ('ink_cap_invoke_json("alarm.update", args, INK_SRC_TOUCH, ink_ui_content_version(), &rc);', "(void)rc;"),
    "input-error": ('snprintf(C.error, sizeof C.error, "%s", c->rc.status == INK_RC_INVALID ? "名称最多15个字，请缩短后保存" : "没能保存，请重试");', "C.error[0] = 0;"),
    "input-bytes": (".max = sizeof C.draft.title - 1", ".max = sizeof C.draft.title"),
    "minus": ('{"时-", "时＋", "分-", "分＋"}', '{"时−", "时＋", "分−", "分＋"}'),
    "list-labels": ('lv_label_set_text(lv_obj_get_child(slot, 1), time); lv_label_set_text(lv_obj_get_child(slot, 2), note);',
                    'lv_label_set_text(lv_obj_get_child(slot, 1), ""); lv_label_set_text(lv_obj_get_child(slot, 2), "");'),
    "persistence": ("if (ink_db_put(T.db, &w, NULL) == INK_DB_OK) {", "if ((void)w, true) {"),
}
summaries = []
for variant in variants if a.variant == "all" else (a.variant,):
    source_path = ROOT / ("system/services/time/time_service.c" if variant == "persistence" else "system/apps/clock/clock.c")
    source = source_path.read_text()
    old, new = variants[variant]
    assert source.count(old) == 1, variant
    area = Path(tempfile.mkdtemp(prefix=variant + "-", dir=work))
    link = shlex.split((build / "CMakeFiles/sim-clock-alarm.dir/link.txt").read_text())
    obj, binary = area / "variant.o", area / "sim-clock-alarm"
    subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                    "-I", str(source_path.parent), "-x", "c", "-c", "-", "-o", str(obj)],
                   input=source.replace(old, new), text=True, cwd=build, check=True)
    link.insert(next(i for i, value in enumerate(link) if value.endswith(".a")), str(obj))
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    (area / "sd").mkdir()
    for phase in (("create", "restart") if variant == "persistence" else ("create",)):
        out = area / phase
        result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
            "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--out", str(out), "--end", "120s",
            "--flash-image", str(area / "user.img"), "--sd-dir", str(area / "sd"),
            "--quiet", "--no-random-outages", "--budget", str(ROOT / "tests/ui_scene.budget.json")],
            env={**os.environ, "INK_CLOCK_PHASE": phase}, capture_output=True, text=True, timeout=120)
        (area / f"{phase}.log").write_text(result.stdout + result.stderr)
        metrics = json.loads((out / "metrics.json").read_text())
        data = {"variant": variant, "phase": phase, "simulator_exit": result.returncode,
                "pass": metrics["metrics"].get("test.pass", 0), "fail": metrics["metrics"].get("test.fail", 0),
                "finished": metrics["metrics"].get("test.finished", 0)}
        (area / f"{phase}-summary.json").write_text(json.dumps(data, indent=2) + "\n")
        print(json.dumps(data), flush=True)
        summaries.append(data)
        if variant == "persistence" and phase == "create":
            assert result.returncode == 0 and not data["fail"] and data["finished"] == 1
        else:
            assert result.returncode != 0 and data["fail"] > 0 and data["finished"] == 1
(work / "summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
