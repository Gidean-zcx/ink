#!/usr/bin/env python3
"""Run bounded audit probes, or collect already completed ordinary runs.
All compilation output and simulator output stay in the selected build directory.
"""
import argparse
import json
from pathlib import Path
import subprocess
from build_probe import ROOT, build_probe

HERE = Path(__file__).resolve().parent
PROBES = {
    "net_recovery": ("net_recovery.script", "metalio-plus"),
    "ai_lost_capture": ("ai_lost_capture.script", "metalio-plus"),
    "remote_feedback": ("short.script", "metalio-plus"),
    "audio_failed_start": ("audio_failed_start.script", "metalio-plus-handsfree"),
    "ui_queue_probe": ("audio_failed_start.script", "metalio-plus"),
    "hal_switch": ("hal_switch.script", "metalio-plus"),
}

parser = argparse.ArgumentParser()
parser.add_argument("build", choices=["build-audit", "build-audit-asan"])
parser.add_argument("--only", nargs="+", choices=list(PROBES), default=list(PROBES))
parser.add_argument("--collect", action="store_true", help="collect an earlier successful run, without rerunning")
parser.add_argument("--hal-mutant", action="store_true", help="reintroduce the old HAL assignment in an in-memory copy")
args = parser.parse_args()
build = ROOT / args.build
result_path = HERE / "results.json"
results = json.loads(result_path.read_text()) if result_path.exists() else []

for probe in args.only:
    script, profile = PROBES[probe]
    name = "hal_switch_mutant" if args.hal_mutant else probe
    directory = "ui_queue" if name == "ui_queue_probe" else name
    out = build / "audit12" / directory
    logname = directory.replace("_", "-")
    prefix = "ink-audit12-asan-" if args.build.endswith("-asan") else "ink-audit12-"
    log = Path("/tmp") / (prefix + logname + ".log")
    cmd = [str(build/name), "--profile", str(ROOT/f"simulator/profiles/{profile}.json"),
           "--script", str(HERE/script), "--assets", str(build/"assets"),
           "--content", str(build/"test-content"), "--out", str(out),
           "--quiet", "--no-random-outages"]
    if probe == "hal_switch":
        cmd += ["--budget", str(HERE/"hal_switch.budget.json")]
    exit_code = 0  # --collect is only for prior runs whose exit 0 was observed separately.
    if not args.collect:
        extras = []
        if args.hal_mutant:
            assert args.only == ["hal_switch"]
            p = ROOT/"ports/sim/hal/hal_net_sim.c"
            text = p.read_text()
            old = "        set_link(t, env_ok(t)); /* from the old link"
            assert text.count(old) == 1
            extras = [(p, text.replace(old, "        N.type = t;\n" + old))]
        with log.open("w") as f:
            build_probe(build, HERE/(probe+".c"), name, extra_sources=extras)
            completed = subprocess.run(cmd, stdout=f, stderr=f, timeout=60)
            exit_code = completed.returncode
    m = json.loads((out/"metrics.json").read_text())
    item = dict(build=args.build, probe=name, exit=exit_code,
                command=cmd, log=str(log),
                observations=[x for x in log.read_text().splitlines() if x.startswith("AUDIT")],
                metrics={k:v for k,v in m["metrics"].items() if k.startswith(("audit.", "test.", "warnings.", "panic."))},
                warnings=m.get("warnings", []), budget_violations=m.get("budget_violations", []))
    results = [r for r in results if (r["build"], r["probe"]) != (args.build, name)] + [item]
    result_path.write_text(json.dumps(results, ensure_ascii=False, indent=2)+"\n")
    print(json.dumps({k:v for k,v in item.items() if k not in ("command",)}, ensure_ascii=False), flush=True)
