"""Run real clock pages twice against the same synthetic flash image."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--binary", type=Path)
a = p.parse_args()
build = a.build.resolve()
area = Path(tempfile.mkdtemp(prefix="clock-alarm-", dir=build / "test-out"))
(area / "sd").mkdir()
for phase in ("create", "restart"):
    out = area / phase
    cmd = [str(a.binary.resolve() if a.binary else build / "sim-clock-alarm"), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--out", str(out),
           "--end", "120s", "--flash-image", str(area / "user.img"), "--sd-dir", str(area / "sd"),
           "--quiet", "--no-random-outages", "--budget", str(ROOT / "tests/ui_scene.budget.json")]
    result = subprocess.run(cmd, env={**os.environ, "INK_CLOCK_PHASE": phase}, capture_output=True, text=True, timeout=120)
    (area / f"{phase}.log").write_text(result.stdout + result.stderr)
    metrics = json.loads((out / "metrics.json").read_text())["metrics"]
    print(f"{phase}: exit={result.returncode}, pass={metrics.get('test.pass', 0)}, "
          f"fail={metrics.get('test.fail', 0)}, finished={metrics.get('test.finished', 0)}; {out.relative_to(ROOT)}", flush=True)
    if result.returncode or metrics.get("test.fail", 0) or metrics.get("test.finished", 0) != 1:
        raise SystemExit(result.stdout + result.stderr)
print("PASS: real touch, AI snapshots, repeat masks, deletion, ringing and restart persistence")
