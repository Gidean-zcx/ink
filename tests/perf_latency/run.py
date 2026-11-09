"""Repeat fresh configured boots; keep simulator distributions, never host wall time."""
import argparse
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
p.add_argument("--runs", type=int, default=1)
p.add_argument("--regression", action="store_true")
p.add_argument("--out", type=Path)
p.add_argument("--binary", type=Path)
a = p.parse_args()
build = a.build.resolve()
work = a.out.resolve() if a.out else Path(tempfile.mkdtemp(prefix="perf-latency-", dir=build))
work.mkdir(parents=True, exist_ok=True)
script = work / "run.script"
script.write_text("@0 net wifi\n@120s end\n")
common = [str(a.binary.resolve() if a.binary else build / "sim-perf-latency"), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
          "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--script", str(script),
          "--quiet", "--no-random-outages"]
mode = [] if a.regression else ["--cpu", "measured", "--trace"]
env = {**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"}
if a.regression:
    env["INK_PERF_REGRESSION"] = "1"
rows = []
samples = {}
for i in range(a.runs + 1):
    out = work / ("seed" if i == 0 else f"run-{i:02}")
    state = work / ("seed-state" if i == 0 else f"state-{i:02}")
    state.mkdir(exist_ok=True)
    if i:
        shutil.copyfile(work / "seed-state/user.img", state / "user.img")
    cmd = [*common, *mode, "--out", str(out), "--flash-image", str(state / "user.img"), "--sd-dir", str(state / "sd")]
    result = subprocess.run(cmd, cwd=ROOT, env={**env, **({"INK_PERF_BOOT_ONLY": "1"} if i == 0 else {})},
                            capture_output=True, text=True, timeout=150)
    out.mkdir(exist_ok=True)
    (out / "run.log").write_text(result.stdout + result.stderr)
    data = json.loads((out / "metrics.json").read_text())
    m = data["metrics"]
    failures = m.get("test.fail", 0)
    print(out.name, f"exit={result.returncode} checks={m.get('test.pass', 0)} failures={failures}", flush=True)
    if result.returncode or failures or m.get("test.finished") != 1:
        print(result.stderr[-5000:])
        raise SystemExit(1)
    if not i:
        continue
    if a.regression and data.get("warnings"):
        raise SystemExit(str(data["warnings"]))
    rows.append(data)
    if not a.regression:
        trace = out / "trace.json"
        for e in json.loads(trace.read_text()):
            name = e.get("name", "")
            if name.startswith("perf.") and "value" in e.get("args", {}):
                samples.setdefault(name, []).append(e["args"]["value"])
        trace.unlink()  # retain the small measured samples, not the waveform trace

# Each run has one boot/open/page sample. Multi-sample path distributions stay per run.
names = sorted(k for k in rows[0]["metrics"] if k.startswith("perf.") or k in
               ("ui.action_to_first_change_ms", "ui.action_to_visible_ms", "reader.page_render_ms"))
summary = {"runs": a.runs, "cpu_mode": rows[0]["cpu_mode"], "metrics": {}}
for name in names:
    distributions = [r["metrics"][name] for r in rows if name in r["metrics"]]
    if name in samples:
        values = sorted(samples[name])
        summary["metrics"][name] = {"count": len(values),
            "p50": values[max(0, math.ceil(len(values) * .50) - 1)],
            "p95": values[max(0, math.ceil(len(values) * .95) - 1)]}
    else:
        summary["metrics"][name] = {"per_run": [{k: d[k] for k in ("count", "p50", "p95")} for d in distributions]}
summary["warnings"] = sum(len(r.get("warnings", [])) for r in rows)
(work / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
(work / "samples.json").write_text(json.dumps(samples, indent=2) + "\n")
print(json.dumps(summary, ensure_ascii=False, indent=2))
if not a.out and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT))
