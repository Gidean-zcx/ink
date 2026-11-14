"""The spoken answers of ai_ask over the pessimistic LTE link, seeds 1 to 20: random loss and retransmissions, and the
script's 600 ms mid-answer stall, must never leave a gap (a speaker underrun). Audible concealment (two or more frames
in a row made up) is counted and printed. Other budgets are the scenario tests' job.

--without-lead runs the same with the hub only 5 frames ahead (as before voice-stream-1): the reverse control, which
passes only when most runs are heard broken (a gap or audible concealment)."""
import argparse
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--seeds", type=int, default=20)
parser.add_argument("--profile", default="metalio-plus")
parser.add_argument("--without-lead", action="store_true")
args = parser.parse_args()
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="ai-ask-seeds-", dir=build))
hub = ["--hub", str(ROOT / "tests/scenarios/ai_ask_no_lead.hub.json")] if args.without_lead else []
gaps = audible = answers = broken = 0
first = []
for seed in range(1, args.seeds + 1):
    out = work / str(seed)
    subprocess.run([str(build / "sim-configured"), "--profile", str(ROOT / f"simulator/profiles/{args.profile}.json"),
        "--script", str(ROOT / "tests/scenarios/ai_ask.script"), "--budget", str(ROOT / "tests/scenarios/ai_ask.budget.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--out", str(out),
        "--quiet", "--no-random-outages", "--seed", str(seed), *hub], cwd=ROOT, capture_output=True, text=True, timeout=120)
    metrics = json.loads((out / "metrics.json").read_text())["metrics"]
    heard = metrics.get("ai.release_to_first_audio_heard_ms", {})
    run_gaps, run_audible = metrics.get("audio.speaker.underruns", 0), metrics.get("audio.audible_concealments", 0)
    gaps, audible, broken = gaps + run_gaps, audible + run_audible, broken + bool(run_gaps or run_audible)
    answers += heard.get("count", 0)
    first.append(heard.get("max", 0))
    print(f"seed {seed}: underruns {run_gaps}, audible concealments {run_audible}, concealed frames "
          f"{metrics.get('audio.play_concealed_frames', 0)}, retransmissions {metrics.get('net.retransmissions', 0)}, "
          f"stalls {metrics.get('net.stalls', 0)}, first sound max {heard.get('max', 0):.0f} ms", flush=True)
print(f"{args.seeds} seeds, {answers} spoken answers: {gaps} gaps, {audible} audible concealments, "
      f"{broken} runs heard broken; first sound max {max(first):.0f} ms, mean of the worst per run "
      f"{sum(first) / len(first):.0f} ms")
complete = answers == 2 * args.seeds
ok = complete and (broken * 4 >= args.seeds * 3 if args.without_lead else gaps == 0)
if ok:
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT))
raise SystemExit(0 if ok else 1)
