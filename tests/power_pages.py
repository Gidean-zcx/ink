"""Verify every real battery page and retain comparison inputs when requested."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

sim, root, assets, content, out = map(Path, sys.argv[1:])
shutil.rmtree(out, ignore_errors=True)
for scene in ("BatteryLow", "BatterySaving", "Charging", "PowerOffEmpty", "PowerOffNoBookmark"):
    target = out / scene
    command = [str(sim), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
               "--script", str(root / "tests/power_pages.script"), "--budget", str(root / "tests/ui_scene.budget.json"),
               "--assets", str(assets), "--content", str(content), "--out", str(target), "--quiet", "--no-random-outages"]
    result = subprocess.run(command, env=dict(os.environ, INK_POWER_SCENE=scene), capture_output=True, text=True)
    metrics = json.loads((target / "metrics.json").read_text())
    assert result.returncode == 0, (scene, result.returncode, result.stderr, metrics.get("warnings"))
    counters = metrics["metrics"]
    assert counters.get("test.finished") == 1, (scene, "did not finish")
    assert counters.get("test.snapshot.settled") == counters.get(f"test.snapshot.{scene}") == 1
    assert (target / "snap-scene.png").exists(), scene
    print(f'{scene}: {counters["test.pass"]} assertions; internal RAM minimum {counters["mem.internal_total.min_free_bytes"]} bytes')
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(out)
