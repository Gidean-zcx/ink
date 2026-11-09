"""Remove only the focused-input snapshot fix in a separate compiled object."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
work = build / "clock-negative"
work.mkdir(exist_ok=True)
flags = {}
for line in (build / "CMakeFiles/ink_system.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
source = (ROOT / "system/apps/clock/clock.c").read_text()
old = "if (C.name && ink_text_focused()) EDIT.action_count = 0;"
assert source.count(old) == 1
(work / "clock.c").write_text(source.replace(old, "if (C.name && ink_text_focused()) EDIT.action_count = actions;"))
link = shlex.split((build / "CMakeFiles/sim-clock-alarm.dir/link.txt").read_text())
obj = work / "clock.o"
subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                "-c", str(work / "clock.c"), "-o", str(obj)], cwd=build, check=True)
link.insert(next(i for i, value in enumerate(link) if value.endswith(".a")), str(obj))
binary = work / "sim-clock-alarm"
link[link.index("-o") + 1] = str(binary)
subprocess.run(link, cwd=build, check=True)
out = Path(tempfile.mkdtemp(prefix="run-", dir=work))
result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
    "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--out", str(out), "--end", "120s",
    "--quiet", "--no-random-outages", "--budget", str(ROOT / "tests/ui_scene.budget.json")], capture_output=True, text=True, timeout=120)
(work / "run.log").write_text(result.stdout + result.stderr)
metrics = json.loads((out / "metrics.json").read_text())
data = {"simulator_exit": result.returncode, "pass": metrics["metrics"].get("test.pass"),
        "fail": metrics["metrics"].get("test.fail"), "finished": metrics["metrics"].get("test.finished")}
(work / "summary.json").write_text(json.dumps(data, indent=2) + "\n")
print(json.dumps(data))
assert result.returncode != 0 and data["finished"] == 1 and data["fail"] == 4
