"""Run the weather acceptance on the real product with the simulated hub's fake forecast, then boot the same flash
again without a network; frames stay in test-out."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--binary", type=Path, help="a rebuilt simulator, for reverse checks")
args = parser.parse_args()
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="weather-", dir=build))
ok = True
for phase, script in (("main", "weather.script"), ("restart", "offline.script")):
    out = build / "test-out" / ("weather" if phase == "main" else "weather-restart")
    result = subprocess.run([str(args.binary or build / "sim-weather"), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(ROOT / "tests/weather" / script), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--flash-image", str(work / "user.img"),
        "--out", str(out), "--quiet", "--no-random-outages"],
        env={**os.environ, "INK_WEATHER_PHASE": phase}, capture_output=True, text=True, timeout=300)
    data = json.loads((out / "metrics.json").read_text(errors="replace")) if (out / "metrics.json").exists() else {}
    counts = data.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
    ok &= passed
    print("weather", phase, "PASS" if passed else "FAIL", "exit", result.returncode,
          "checks", counts.get("test.pass", 0), "failures", counts.get("test.fail", 0), flush=True)
    if not passed:
        for warning in data.get("warnings", []):
            print(warning)
        print(result.stderr[-2000:])
        break
shutil.rmtree(work)
raise SystemExit(0 if ok else 1)
