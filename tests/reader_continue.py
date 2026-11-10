"""The continue-reading card: read, go home, restart (same flash and card), then the book goes away."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / "test-out" / "reader-continue"
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
for phase in ("read", "restart"):
    out = area / phase
    result = subprocess.run([str(build / "sim-reader-continue"), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(ROOT / "tests/kid_3.script"), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--out", str(out),
        "--flash-image", str(area / "user.img"), "--sd-dir", str(area / "sd"), "--no-random-outages", "--quiet"],
        cwd=ROOT, capture_output=True, text=True, timeout=200,
        env={**os.environ, "INK_READER_CONTINUE_PHASE": phase, "UBSAN_OPTIONS": "halt_on_error=1"})
    (area / f"{phase}.log").write_text(result.stdout + result.stderr)
    data = json.loads((out / "metrics.json").read_text(errors="replace")) if (out / "metrics.json").exists() else {}
    counts = data.get("metrics", {})
    if result.returncode or counts.get("test.finished") != 1 or counts.get("test.fail", 0):
        raise SystemExit(f"{phase}: FAIL exit={result.returncode}\n{result.stderr[-6000:]}\n{data.get('warnings', [])}")
    print(f'{phase}: PASS checks={counts.get("test.pass")}', flush=True)
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(area)
else:
    print("Evidence:", area.relative_to(ROOT))
