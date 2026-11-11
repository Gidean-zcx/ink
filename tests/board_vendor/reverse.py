"""Manual reverse controls; mutate temporary objects, never the working tree or test assertions."""
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
a = p.parse_args()
build = a.build.resolve()
area = build / "test-out/board-vendor-reverse"
area.mkdir(parents=True, exist_ok=True)
db = json.loads((build / "compile_commands.json").read_text())
cases = (
    ("usb-cut", "drivers/chips/cx25601n/cx25601n.c", "&& vbus == 0", "", "charger", "external power must not cut BATFET"),
    ("uv-3200", "drivers/chips/cx25601n/cx25601n.c", "UV_MV = 3100", "UV_MV = 3200", "charger", "3101 mV must not cut BATFET"),
    ("busy-kick", "drivers/chips/cx25601n/cx25601n.c", "stat == CX25601N_NOT_CHARGING && ma < 5", "ma < 5", "charger", "non-idle charger must not kick"),
    ("instant-light", "system/services/display/light.c", "if (immediate) g_level = g_target;", "g_level = g_target;", "light", "setting does not jump straight to target"),
    ("extra-light-lock", "system/services/display/light.c", "if (ink_svc_init() != 0) return -1;",
     "osal_mutex_create(); if (ink_svc_init() != 0) return -1;", "light", "frontlight must reuse the service lock"),
    ("sync-recharge", "drivers/chips/cx25601n/cx25601n.c", "*kick_step = 1;\n        return 200;",
     "*kick_step = 1;\n        osal_task_delay_us(200000);\n        return 200;", "charger", "recharge must schedule waits, never block a task"),
    ("early-check", "system/services/power/power_service.c", "(uint64_t)PS.charger_ms * 1000, charger_due, NULL);",
     "0, charger_due, NULL);", "power", "boot waits 2 s before first charger check"),
    ("power-blocked", "system/services/power/power_service.c",
     "static void charger_due(void *ctx) { ink_exec_post(INK_EXEC_BG, &CH.job, charger_work, ctx); }",
     "static void charger_due(void *ctx) { (void)ctx; osal_sem_give(PS.kick); }", "power", "power task handles lock during charger I2C"),
)
for name, path, before, after, kind, expected in cases:
    source = (ROOT / path).read_text()
    assert source.count(before) == 1, (name, before)
    source = source.replace(before, after)
    compile_path = path
    if name == "sync-recharge":
        source = '#include "ink/osal.h"\n' + source
    if kind == "power":
        if name == "power-blocked":
            source = source.replace("for (;;) {\n        uint64_t now = osal_now_us();",
                                    "for (;;) {\n        if (CH.at.fn && !CH.at.armed) charger_work(NULL);\n        uint64_t now = osal_now_us();")
        compile_path = "tests/board_vendor/power.c"
        source = (ROOT / compile_path).read_text().replace('#include "../../system/services/power/power_service.c"', source)
    entry = next(e for e in db if Path(e["file"]) == ROOT / compile_path)
    cmd = shlex.split(entry["command"])
    obj = area / (name + ".o")
    cmd[cmd.index("-o") + 1] = str(obj)
    del cmd[cmd.index("-c"):cmd.index("-c") + 2]
    subprocess.run(cmd + ["-iquote", str((ROOT / compile_path).parent), "-x", "c", "-c", "-"],
                   input=source, cwd=build, text=True, check=True)
    target = ("sim-" if kind == "power" else "test-") + "board-vendor-" + kind
    link = shlex.split((build / f"CMakeFiles/{target}.dir/link.txt").read_text())
    binary = area / name
    link[link.index("-o") + 1] = str(binary)
    if kind == "charger":
        link.insert(next(i for i, part in enumerate(link) if part.endswith("libink_drivers.a")), str(obj))
    else:
        index = next(i for i, part in enumerate(link) if part.endswith(compile_path + ".o"))
        link[index] = str(obj)
    subprocess.run(link, cwd=build, check=True)
    command = [str(binary)]
    if kind == "power":
        command += ["--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
                    "--budget", str(ROOT / "tests/ui_scene.budget.json"), "--end", "75s", "--quiet", "--no-random-outages",
                    "--out", str(area / (name + ".out"))]
    result = subprocess.run(command, capture_output=True, text=True, timeout=30)
    log = result.stdout + result.stderr
    if kind == "power":
        log += "\n".join(json.loads((area / (name + ".out") / "metrics.json").read_text())["warnings"])
    (area / (name + ".log")).write_text(log)
    assert result.returncode != 0 and expected in log, (name, result.returncode, log)
    print(f"{name}: exit={result.returncode}; {expected}", flush=True)
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(area)
