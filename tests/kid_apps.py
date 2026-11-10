"""Run app-list, time-policy, real controls and separate-process persistence checks."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", required=True, type=Path)
p.add_argument("--binary", type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / "sim-kid-apps"
work = Path(tempfile.mkdtemp(prefix="kid-apps-", dir=build))
ok = True
for phase in ("policy", "restart", "locale"):
    out = work / phase
    result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(ROOT / "tests/kid_2_settings.script"), "--assets", str(build / "assets"),
        "--content", str(build / "test-content"), "--out", str(out), "--flash-image", str(work / "family.img"),
        "--sd-dir", str(work / "sd"), "--quiet", "--no-random-outages", "--wall-start", "1794268800",
        "--budget", str(ROOT / "tests/ui_scene.budget.json")],
        env={**os.environ, "INK_KID_APPS_PHASE": phase}, capture_output=True, text=True, timeout=150)
    (work / f"{phase}.log").write_text(result.stdout + result.stderr)
    data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
    counts = data.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
    print(phase, "PASS" if passed else "FAIL", "exit", result.returncode, "checks", counts.get("test.pass", 0), flush=True)
    ok &= passed
    if not passed:
        print(result.stderr[-10000:], data.get("warnings", []), flush=True)
        break
    for path in out.rglob("*"):
        if path.suffix in (".txt", ".json", ".ndjson", ".log"):
            assert "24681357" not in path.read_text(errors="replace"), (phase, path.name, "PIN leak")
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT), flush=True)
raise SystemExit(0 if ok else 1)
