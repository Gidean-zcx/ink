"""Storage queue refusal and failed completion, followed by process restart."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--binary", type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / "sim-pairing-storage"
work = Path(tempfile.mkdtemp(prefix="pairing-storage-", dir=build))
for mode in range(1, 11):
    area = work / str(mode)
    area.mkdir()
    for recover in (False, True):
        label = "recover" if recover else "refuse"
        out = area / label
        env = {**os.environ, "INK_PAIR_STORAGE_MODE": str(mode), "UBSAN_OPTIONS": "halt_on_error=1"}
        if recover: env["INK_PAIR_STORAGE_RECOVER"] = "1"
        cmd = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
               "--assets", str(build / "assets"), "--content", str(build / "test-content"),
               "--out", str(out), "--end", "35s", "--quiet", "--no-random-outages",
               "--flash-image", str(area / "user.img"), "--sd-dir", str(area / "sd")]
        run = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=60, env=env)
        (area / (label + ".log")).write_text(run.stdout + run.stderr)
        data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
        metrics = data.get("metrics", {})
        done = metrics.get("test.finished") == 1 if recover else metrics.get("test.checkpoint") == 1 and metrics.get("power.reboot") == 1
        ok = run.returncode == 0 and done and metrics.get("test.fail", 0) == 0
        print(mode, label, "PASS" if ok else "FAIL", "exit=" + str(run.returncode), "checks=" + str(metrics.get("test.pass", 0)), flush=True)
        if not ok:
            print(run.stderr[-2500:]); print(data.get("warnings", [])); raise SystemExit(1)
print("Evidence directory:", work.relative_to(ROOT))
