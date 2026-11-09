"""Real child surfaces, virtual time and durable member records across reboots."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--phase", choices=("limit", "restart", "blocked", "control", "settings", "no-pin"))
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix="kid-2-", dir=build))
ok = True
for phase in (a.phase,) if a.phase else ("limit", "restart", "blocked", "control", "settings", "no-pin"):
    out = work / phase
    image = work / ("user.img" if phase in ("limit", "restart") else f"{phase}.img")
    result = subprocess.run([str(build / "sim-kid-2"), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(ROOT / ("tests/kid_2_settings.script" if phase == "settings" else "tests/kid_2.script")), "--assets", str(build / "assets"), "--content", str(build / "test-content"),
        "--wall-start", "1794268800", "--flash-image", str(image), "--sd-dir", str(work / "sd"), "--out", str(out), "--quiet", "--no-random-outages",
        "--budget", str(ROOT / "tests/ui_scene.budget.json")], env={**os.environ, "INK_KID2_PHASE": phase}, capture_output=True, text=True, timeout=150)
    (work / f"{phase}.log").write_text(result.stdout + result.stderr)
    data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
    metrics = data.get("metrics", {})
    passed = result.returncode == 0 and metrics.get("test.finished") == 1 and not metrics.get("test.fail", 0)
    print(phase, "PASS" if passed else "FAIL", "exit", result.returncode, "checks", metrics.get("test.pass", 0), "failures", metrics.get("test.fail", 0), flush=True)
    ok &= passed
    if not passed:
        print(data.get("warnings", []), result.stderr[-1600:])
        break
    for path in out.rglob("*"):
        if path.suffix in (".txt", ".json", ".ndjson", ".log"):
            assert "24681357" not in path.read_text(errors="replace"), (phase, path.name, "PIN leak")
    scene = {"limit": "KidTimeUp", "blocked": "KidLimit", "control": "KidControl", "settings": "SetKid"}.get(phase)
    if scene:
        target = build / "kid-2-compare" / scene
        target.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(out / f"snap-{scene}.png", target / "snap-scene.png")
        shutil.copyfile(out / "metrics.json", target / "metrics.json")
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT), flush=True)
raise SystemExit(0 if ok else 1)
