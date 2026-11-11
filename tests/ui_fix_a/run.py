"""Run isolated page fixtures at normal and 120% text sizes; optionally retain PNG evidence."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--case", choices=("backup", "confirm", "transfer", "factory"), required=True)
parser.add_argument("--binary", type=Path)
parser.add_argument("--evidence", type=Path)
parser.add_argument("--expect-failure", action="store_true")
a = parser.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / ("sim-ui-fix-a-" + a.case)
area = build / "test-out" / ("ui-fix-a-" + a.case + "-" + binary.parent.name)
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
for scale in (100, 120):
    out = area / str(scale)
    script = area / "scenario.script"
    script.write_text("@0 net wifi\n@180s end\n")
    result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(script), "--assets", str(build / "assets"), "--content", str(build / "test-content"),
        "--out", str(out), "--sd-dir", str(out / "synthetic-card"), "--quiet", "--no-random-outages"],
        cwd=ROOT, capture_output=True, text=True, timeout=90,
        env={**os.environ, "INK_UI_FIX_SCALE": str(scale), "UBSAN_OPTIONS": "halt_on_error=1"})
    data = json.loads((out / "metrics.json").read_text())
    metrics = data["metrics"]
    failures = [w for w in data.get("warnings", []) if "CHECK FAILED" in str(w)]
    (out / "run.log").write_text(result.stdout + result.stderr)
    assert metrics.get("test.finished") == 1, (result.returncode, result.stderr[-4000:])
    if a.expect_failure:
        assert metrics.get("test.fail", 0) > 0 and failures, "baseline unexpectedly satisfies the regression"
    else:
        assert not result.returncode and not metrics.get("test.fail", 0) and not data.get("warnings"), result.stderr[-7000:]
    if a.evidence:
        from PIL import Image
        target = a.evidence / a.case / str(scale)
        target.mkdir(parents=True, exist_ok=True)
        for source in out.glob("snap-*.png"):
            image = Image.open(source).convert("L")
            image.save(target / source.name, optimize=True)
            assert (target / source.name).stat().st_size <= 300_000
    print(f"{a.case} {scale}%: {'EXPECTED FAILURE' if a.expect_failure else 'PASS'}; checks={int(metrics.get('test.pass', 0))}; failures={int(metrics.get('test.fail', 0))}")
    if a.expect_failure:
        print("\n".join(str(w) for w in failures[:5]))
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(area)
