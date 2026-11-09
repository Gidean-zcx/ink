"""Production child pages, synthetic member data and settled physical screenshots."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
p.add_argument("--binary", type=Path)
p.add_argument("--scene", choices=("KidHome", "KidToday", "KidApps"))
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / "sim-kid-3"
area = build / "test-out" / ("kid-3-" + binary.name)
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
for scene in (a.scene,) if a.scene else ("KidHome", "KidToday", "KidApps"):
    target = area / scene
    result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(ROOT / "tests/kid_3.script"), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--out", str(target),
        "--flash-image", str(area / (scene + ".img")), "--sd-dir", str(area / (scene + "-sd")),
        "--no-random-outages", "--quiet"], cwd=ROOT, capture_output=True, text=True, timeout=140,
        env={**os.environ, "INK_KID3_SCENE": scene, "UBSAN_OPTIONS": "halt_on_error=1"})
    target.mkdir(parents=True, exist_ok=True)
    (target / "stdout.log").write_text(result.stdout)
    (target / "stderr.log").write_text(result.stderr)
    data = json.loads((target / "metrics.json").read_text()) if (target / "metrics.json").exists() else {}
    counts = data.get("metrics", {})
    if result.returncode or counts.get("test.finished") != 1 or counts.get("test.fail", 0) or data.get("warnings"):
        raise SystemExit(f'{scene}: FAIL exit={result.returncode}\n{result.stderr[-10000:]}\n{data.get("warnings", [])}')
    assert counts.get("test.snapshot.settled") == 1 and counts.get(f"test.snapshot.{scene}") == 1
    assert (target / f"snap-{scene}.png").exists()
    text = (target / f"{scene}.txt").read_text()
    assert scene in text
    if scene == "KidToday":
        assert json.loads(text)["page"] == scene
    shutil.copy2(target / f"snap-{scene}.png", target / "snap-scene.png")
    print(f'{scene}: PASS checks={counts.get("test.pass")}', flush=True)
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(area)
else:
    print("Evidence:", area.relative_to(ROOT))
