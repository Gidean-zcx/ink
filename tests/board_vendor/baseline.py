"""Manual wake controls using Git history; deliberately not registered in CTest."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
p.add_argument("--base", default="40d965f5b4b5d8d510064396dc59e3ae7e96855c")
a = p.parse_args()
build = a.build.resolve()
area = build / "test-out/board-vendor-baseline"
area.mkdir(parents=True, exist_ok=True)
db = json.loads((build / "compile_commands.json").read_text())
paths = ("drivers/chips/cx25601n/cx25601n.c", "simulator/models/dev_power_chips.c",
         "system/services/display/light.c", "system/services/power/power_service.c",
         "boards/metalio_plus/metalio_board.c")
objects = []
for i, path in enumerate(paths):
    source = subprocess.check_output(["git", "show", a.base + ":" + path], cwd=ROOT, text=True)
    entry = next(e for e in db if Path(e["file"]) == ROOT / path)
    cmd = shlex.split(entry["command"])
    obj = area / (str(i) + ".o")
    cmd[cmd.index("-o") + 1] = str(obj)
    del cmd[cmd.index("-c"):cmd.index("-c") + 2]
    subprocess.run(cmd + ["-iquote", str((ROOT / path).parent), "-x", "c", "-c", "-"],
                   input=source, cwd=build, text=True, check=True)
    objects.append(str(obj))
ctest = Path(sys.executable).with_name("ctest")
tests = json.loads(subprocess.check_output([str(ctest), "--test-dir", str(build), "--show-only=json-v1"], text=True))["tests"]
test = next(t for t in tests if t["name"] == "scenario-standby-metalio-plus")
for name, objs in (("no-task", objects[:4]), ("no-poll", [objects[3]]), ("old-rails", objects)):
    cmd = shlex.split((build / "CMakeFiles/sim-configured.dir/link.txt").read_text())
    binary = area / name
    cmd[cmd.index("-o") + 1] = str(binary)
    index = next(i for i, s in enumerate(cmd) if s.endswith("libink_apps.a"))
    cmd[index:index] = objs
    subprocess.run(cmd, cwd=build, check=True)
    run = test["command"][:]
    run[0] = str(binary)
    output = area / (name + ".out")
    run[run.index("--out") + 1] = str(output)
    result = subprocess.run(run, cwd=ROOT, capture_output=True, text=True, timeout=120)
    (area / (name + ".log")).write_text(result.stdout + result.stderr)
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    metrics = json.loads((output / "metrics.json").read_text())["metrics"]
    print(f"{name}: exit=0; wake_max_ms={metrics['power.unlock_to_visible_ms']['max']}", flush=True)
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(area)
