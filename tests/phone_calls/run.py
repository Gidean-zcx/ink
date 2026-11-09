"""Run calls through the real product and reopen the same persistent image."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

sim, root, assets, content, output = map(Path, sys.argv[1:])
shutil.rmtree(output, ignore_errors=True)
output.mkdir(parents=True)
for phase in ("write", "verify", "controls", "Phone", "PhoneRecents", "PhoneCall", "PhoneKeypad"):
    target = output / phase
    image = output / "user.img"
    if phase not in ("write", "verify"):
        image = output / f"{phase}.img"
        shutil.copyfile(output / "user.img", image)
    command = [str(sim), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
               "--script", str(root / "tests/phone_calls/calls.script"),
               "--budget", str(root / "tests/ui_scene.budget.json"),
               "--assets", str(assets), "--content", str(content), "--flash-image", str(image),
               "--sd-dir", str(output / "sd"), "--out", str(target), "--quiet", "--no-random-outages"]
    result = subprocess.run(command, env=dict(os.environ, INK_PHONE_PHASE=phase, INK_PHONE_SCENE=phase), capture_output=True, text=True)
    metrics_path = target / "metrics.json"
    metrics = json.loads(metrics_path.read_text()) if metrics_path.exists() else {}
    assert result.returncode == 0, (phase, result.returncode, result.stderr, metrics.get("warnings"))
    assert metrics["metrics"].get("test.finished") == 1
    print(f'{phase}: {metrics["metrics"]["test.pass"]} assertions')
    if phase == "controls":
        assert metrics["metrics"]["cell.call.uplink_samples"] == 640
        assert metrics["metrics"]["cell.call.muted_samples"] == 160
        assert metrics["metrics"]["cell.call.speaker_samples"] == 640
        assert metrics["metrics"]["cell.call.headset_samples"] == 160
