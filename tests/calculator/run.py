"""Run the calculator acceptance and the 20-app Apps page on the real product; frames stay in test-out."""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--binary", type=Path, help="a rebuilt simulator, for reverse checks")
args = parser.parse_args()
build = args.build.resolve()
binary = args.binary or build / "sim-calculator"
ok = True
for scene in ("calculator", "apps20"):
    out = build / "test-out" / ("calculator" if scene == "calculator" else "calculator-apps20")
    result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(ROOT / "tests/calculator/calculator.script"), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"),
        "--out", str(out), "--quiet", "--no-random-outages"],
        env={**os.environ, "INK_CALC_SCENE": scene}, capture_output=True, text=True, timeout=300)
    data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
    counts = data.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
    ok &= passed
    print(scene, "PASS" if passed else "FAIL", "exit", result.returncode,
          "checks", counts.get("test.pass", 0), "failures", counts.get("test.fail", 0), flush=True)
    if not passed:
        for warning in data.get("warnings", []):
            print(warning)
        print(result.stderr[-2000:])
raise SystemExit(0 if ok else 1)
