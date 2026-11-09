"""Relink the new test with the baseline off-picture initializer, removing all new page wiring."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, default=root / "build")
parser.add_argument("--baseline", default="f0b0c539f3f8a30bca03f663484287d65c76de33")
args = parser.parse_args()
build = args.build.resolve()
work = build / "test-out/power-pages-negative"
work.mkdir(parents=True, exist_ok=True)
source = work / "off_picture.c"
source.write_bytes(subprocess.check_output(["git", "show", f"{args.baseline}:system/ui/shell/off_picture.c"], cwd=root))
cache = (build / "CMakeCache.txt").read_text()
compiler = re.search(r"^CMAKE_C_COMPILER:FILEPATH=(.+)$", cache, re.M)[1]
flags = (build / "CMakeFiles/ink_system.dir/flags.make").read_text()
compile_args = [part for name in ("C_DEFINES", "C_INCLUDES", "C_FLAGS")
                for part in shlex.split(re.search(rf"^{name} = (.*)$", flags, re.M)[1])]
obj = work / "off_picture.o"
subprocess.run([compiler, *compile_args, "-I", str(root / "system/ui/shell"), "-c", str(source), "-o", str(obj)], cwd=build, check=True)
link = shlex.split((build / "CMakeFiles/sim-power-pages.dir/link.txt").read_text())
binary = work / "sim-power-pages-before"
link[link.index("-o") + 1] = str(binary)
link.insert(link.index("-o"), str(obj))
subprocess.run(link, cwd=build, check=True)
for scene in ("BatteryLow", "BatterySaving", "Charging", "PowerOffEmpty"):
    out = work / scene
    command = [str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
               "--script", str(root / "tests/power_pages.script"), "--budget", str(root / "tests/ui_scene.budget.json"),
               "--assets", str(build / "assets"), "--content", str(build / "test-content"),
               "--out", str(out), "--quiet", "--no-random-outages"]
    result = subprocess.run(command, env=dict(os.environ, INK_POWER_SCENE=scene), capture_output=True, text=True)
    metrics = json.loads((out / "metrics.json").read_text())
    failures = metrics["metrics"].get("test.fail", 0)
    assert result.returncode != 0 and failures > 0, (scene, "negative control did not fail")
    print(f"{scene}: baseline wiring exit={result.returncode}, failed assertions={failures}")
