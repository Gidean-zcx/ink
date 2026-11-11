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
)
for name, path, before, after, kind, expected in cases:
    source = (ROOT / path).read_text()
    assert source.count(before) == 1, (name, before)
    source = source.replace(before, after)
    entry = next(e for e in db if Path(e["file"]) == ROOT / path)
    cmd = shlex.split(entry["command"])
    obj = area / (name + ".o")
    cmd[cmd.index("-o") + 1] = str(obj)
    del cmd[cmd.index("-c"):cmd.index("-c") + 2]
    subprocess.run(cmd + ["-iquote", str((ROOT / path).parent), "-x", "c", "-c", "-"],
                   input=source, cwd=build, text=True, check=True)
    target = "test-board-vendor-" + kind
    link = shlex.split((build / f"CMakeFiles/{target}.dir/link.txt").read_text())
    binary = area / name
    link[link.index("-o") + 1] = str(binary)
    if kind == "charger":
        link.insert(next(i for i, part in enumerate(link) if part.endswith("libink_drivers.a")), str(obj))
    else:
        index = next(i for i, part in enumerate(link) if part.endswith("system/services/display/light.c.o"))
        link[index] = str(obj)
    subprocess.run(link, cwd=build, check=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
    log = result.stdout + result.stderr
    (area / (name + ".log")).write_text(log)
    assert result.returncode != 0 and expected in log, (name, result.returncode, log)
    print(f"{name}: exit={result.returncode}; {expected}", flush=True)
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(area)
