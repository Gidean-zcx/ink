#!/usr/bin/env python3
"""Offline audit only; logs here, bulk simulator artifacts in build-audit/."""
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
BUILD = ROOT / "build-audit"
os.chdir(ROOT)
results = []
for name in ("storage_cache", "tca_shadow", "input_fault", "touch_blocks_power",
             "camera_cut", "camera_timeout", "audio_unpowered", "sleep_rails", "display_fault", "nt26_unicode"):
    if sys.argv[1:] and name not in sys.argv[1:]:
        continue
    cmd = [str(BUILD / "audit-probe"), "--profile", "profiles/metalio-plus.json",
           "--out", str(BUILD / "audit-probes" / name), "--end", "20s", "--no-random-outages"]
    with (HERE / (name + ".log")).open("w") as f:
        p = subprocess.run(cmd, env={**os.environ, "AUDIT_PROBE": name}, stdout=f,
                           stderr=subprocess.STDOUT, timeout=45)
    snippets = [x for x in (HERE / (name + ".log")).read_text().splitlines() if "AUDIT " in x]
    results.append(dict(name=name, command=cmd, exit_code=p.returncode, output=snippets))
    print(name, p.returncode, *snippets, sep="\n", flush=True)
for name in ("ai_held_dismiss", "ai_held_control"):
    if sys.argv[1:] and name not in sys.argv[1:]:
        continue
    cmd = [str(BUILD / "ink-sim"), "--profile", "profiles/metalio-plus.json",
           "--script", str(HERE / (name + ".script")), "--assets", str(BUILD / "assets"),
           "--content", str(BUILD / "test-content"), "--out", str(BUILD / "audit-scenarios" / name),
           "--no-random-outages"]
    with (HERE / (name + ".log")).open("w") as f:
        p = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, timeout=45)
    results.append(dict(name=name, command=cmd, exit_code=p.returncode))
    print(name, p.returncode, flush=True)
dest = HERE / "probe-results.json"
previous = json.loads(dest.read_text()) if dest.exists() else []
by_name = {x["name"]: x for x in previous}
by_name.update({x["name"]: x for x in results})
dest.write_text(json.dumps(list(by_name.values()), indent=2, ensure_ascii=False) + "\n")
