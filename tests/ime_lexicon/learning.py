"""Check learning and LRU across real simulator restarts, with synthetic text only."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/imedict"))
from imedict import build as dictionary

parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
args = parser.parse_args()
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="ime-learning-", dir=build))
content = work / "content/.system/ime"
content.mkdir(parents=True)
rows = ["xian\t先\t65000", "xian\t西安\t1", "xa\t喜爱\t65000", "xa\t西安\t1", "xi\t西\t65535"]
for i in range(513):
    key = "synth" + chr(97 + i // 676) + chr(97 + i // 26 % 26) + chr(97 + i % 26)
    rows += [f"{key}\t合成\t65000", f"{key}\t测试\t1"]
(content / "base.ikd").write_bytes(dictionary(rows))
summary = {}
for scope in ("learning", "capacity"):
    state = work / scope
    (state / "sd").mkdir(parents=True)
    for phase in ("write", "verify"):
        mode = scope + "-" + phase
        output = state / phase
        result = subprocess.run([str(build / "sim-ime-learning"), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
                                 "--assets", str(build / "assets"), "--content", str(work / "content"),
                                 "--flash-image", str(state / "user.img"), "--sd-dir", str(state / "sd"),
                                 "--out", str(output), "--end", "120s", "--quiet", "--no-random-outages",
                                 "--budget", str(ROOT / "tests/integration/textinput.budget.json")],
                                env={**os.environ, "INK_IME_LEARNING": mode, "UBSAN_OPTIONS": "halt_on_error=1"},
                                cwd=ROOT, capture_output=True, text=True, timeout=180)
        (state / (phase + ".log")).write_text(result.stdout + result.stderr)
        report = json.loads((output / "metrics.json").read_text())
        metrics = report["metrics"]
        ok = result.returncode == 0 and metrics.get("test.finished") == 1 and metrics.get("test.fail", 0) == 0
        summary[mode] = {"exit": result.returncode, "checks": metrics.get("test.pass", 0),
                         "failures": metrics.get("test.fail", 0), "passed": ok}
        print(mode, summary[mode], flush=True)
        if not ok:
            print(report.get("warnings"), flush=True)
(work / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
assert all(row["passed"] for row in summary.values()), str(work)
if os.environ.get("INK_KEEP_EVIDENCE"):
    print("evidence:", work)
else:
    shutil.rmtree(work)
