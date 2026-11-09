"""Keep F6 acceptance frames and real-service evidence inside the build directory."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser()
ap.add_argument("--build", type=Path, required=True)
a = ap.parse_args()
build = a.build.resolve()
out = build / "test-out/activity-card"
shutil.rmtree(out, ignore_errors=True)
command = [str(build / "sim-activity-card"), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--script", str(ROOT / "tests/activity_card.script"), "--assets", str(build / "assets"),
           "--content", str(build / "test-content"), "--out", str(out), "--quiet", "--no-random-outages",
           "--budget", str(ROOT / "tests/ui_scene.budget.json")]
result = subprocess.run(command, capture_output=True, text=True)
metrics = json.loads((out / "metrics.json").read_text())
assert result.returncode == 0, (result.returncode, result.stdout, result.stderr, metrics.get("warnings"))
assert metrics["metrics"].get("test.finished") == 1
for scene in ("MainPlaying", "ControlPlaying", "LockPlaying", "MainInCall", "Tasks"):
    target = build / "activity-card-compare" / scene
    target.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(out / f"snap-{scene}.png", target / "snap-scene.png")
    shutil.copyfile(out / "metrics.json", target / "metrics.json")
print(f'activity-card: {metrics["metrics"]["test.pass"]} assertions; five settled frames')
