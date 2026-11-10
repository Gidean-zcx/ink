"""Run long-turn interruptions and time limits against the unmodified product binary."""
import argparse
import copy
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--binary", type=Path)
parser.add_argument("--case", choices=("stop", "barge", "member", "call", "silent", "gap", "limit"))
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
build = args.build.resolve()
binary = (args.binary or build / "sim-ai-speech-long").resolve()
out = Path(tempfile.mkdtemp(prefix="ai-speech-long-", dir=build))
fixture = json.loads((root / "tests/ai_answer_long/hub.json").read_text())
voice = next(r for r in fixture["rules"] if r["method"] == "ai.listen")
voice["listen"].pop("tts_speed")  # Exercise the default pace, not an override.
short = copy.deepcopy(voice)
short["stream"]["chunks"] = ["新的合成回答。"]
ok = True
for case in ([args.case] if args.case else ["stop", "barge", "member", "call", "silent", "gap", "limit"]):
    folder = out / case
    folder.mkdir()
    first = copy.deepcopy(voice)
    if case == "silent":
        first["listen"]["tts_first_ms"] = 60000
    elif case == "gap":
        first["stream"]["chunks"] = ["合成。"]
        first["stream"]["interval_ms"] = 30000  # No final reply to mask the no-frame deadline.
    elif case == "limit":
        first["listen"]["tts_chars_per_s"] = 2  # Deliberately more than ten minutes.
    rules = [r for r in fixture["rules"] if r["method"] != "ai.listen"]
    rules.append({"method": "ai.listen", "turns": [first, short]})
    hub = folder / "hub.json"
    hub.write_text(json.dumps({"rules": rules}, ensure_ascii=False))
    script = folder / "run.script"
    script.write_text("@0 net wifi\n@660s end\n")
    result = subprocess.run([str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
        "--script", str(script), "--hub", str(hub), "--assets", str(build / "assets"),
        "--content", str(build / "test-content"), "--budget", str(root / "tests/ui_scene.budget.json"),
        "--out", str(folder), "--quiet", "--no-random-outages"],
        env={**os.environ, "INK_SPEECH_CASE": case}, capture_output=True, text=True, timeout=180)
    (folder / "run.log").write_text(result.stdout + result.stderr)
    metrics = json.loads((folder / "metrics.json").read_text()) if (folder / "metrics.json").exists() else {}
    counts = metrics.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
    print(case, "PASS" if passed else "FAIL", "exit", result.returncode, "checks", counts.get("test.pass", 0),
          "virtual seconds", counts.get("test.elapsed_s"), "min internal", counts.get("mem.internal_total.min_free_bytes"), flush=True)
    if not passed:
        print(result.stderr[-4000:])
        print(metrics.get("warnings", []))
        ok = False
        break
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(out)
else:
    print("Evidence:", out.relative_to(root))
raise SystemExit(0 if ok else 1)
