"""Enrollment recovery across actual simulator process restarts; synthetic data only."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--binary", type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / "sim-pairing"
work = Path(tempfile.mkdtemp(prefix="pairing-", dir=build))

def boot(area, phase, checkpoint=False):
    out = area / phase
    cmd = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"),
           "--out", str(out), "--end", "65s", "--quiet", "--no-random-outages",
           "--flash-image", str(area / "user.img"), "--sd-dir", str(area / "sd")]
    run = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=90,
                         env={**os.environ, "INK_PAIR_PHASE": phase, "UBSAN_OPTIONS": "halt_on_error=1"})
    (area / (phase + ".stdout.log")).write_text(run.stdout)
    (area / (phase + ".stderr.log")).write_text(run.stderr)
    data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
    metrics = data.get("metrics", {})
    done = metrics.get("test.checkpoint") == 1 and metrics.get("power.reboot") == 1 if checkpoint else metrics.get("test.finished") == 1
    if phase == "wipe": done = metrics.get("power.reboot") == 1 and metrics.get("system.factory_resets") == 1
    ok = run.returncode == 0 and done and metrics.get("test.fail", 0) == 0
    print(phase, "PASS" if ok else "FAIL", "exit=" + str(run.returncode), "checks=" + str(metrics.get("test.pass", 0)), flush=True)
    if not ok:
        print(run.stderr[-3500:])
        print(json.dumps(data.get("warnings", []), ensure_ascii=False))
        raise SystemExit(1)

area = work / "recovery"
area.mkdir()
for phase in ("network", "claim", "commit"):
    boot(area, phase, checkpoint=True)
# Preserve the committing image for mutually independent error and wipe scenarios.
for label in ("expired", "invalid", "wipe"):
    target = work / label
    target.mkdir()
    shutil.copy2(area / "user.img", target / "user.img")
    shutil.copytree(area / "sd", target / "sd")
boot(area, "bound", checkpoint=True)
boot(area, "revoked")
for phase in ("expired", "invalid"):
    boot(work / phase, phase)
boot(work / "wipe", "wipe")
boot(work / "wipe", "wiped")
for phase in ("hotspot", "card", "pin"):
    target = work / phase
    (target / "sd/content").mkdir(parents=True)
    if phase == "card":
        (target / "sd/content/ink-pair.json").write_text(json.dumps({
            "url": "wss://example.com/v1/device", "pin": "11" * 32, "preauth": "test-once"}))
    boot(target, phase)
    if phase == "hotspot": boot(target, "hotspot-reboot")
print("Evidence directory:", work.relative_to(ROOT))
