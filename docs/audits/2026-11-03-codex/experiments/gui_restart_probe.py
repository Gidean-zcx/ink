#!/usr/bin/env python3
"""Headless restart observation; never opens host microphone or camera."""
import json
import os
from pathlib import Path
import subprocess
import time

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
out = ROOT / "build-audit/audit-gui-restart"
out.mkdir(parents=True, exist_ok=True)
cmd = [str(ROOT / "build-audit/ink-sim-gui"), "--data", str(out), "--speed", "4",
       "--no-random-outages", "--gui-click", "1070,133@4", "--gui-exit-after", "15",
       "--gui-screenshot", str(out / "window.png")]
samples = []
with (HERE / "gui-restart.log").open("w") as log:
    proc = subprocess.Popen(cmd, cwd=ROOT, env={**os.environ, "SDL_VIDEODRIVER": "dummy"},
                            stdout=log, stderr=subprocess.STDOUT)
    try:
        until = time.monotonic() + 25
        while proc.poll() is None and time.monotonic() < until:
            time.sleep(0.35)
            text = (HERE / "gui-restart.log").read_text()
            boots = text.count("ink-sim: CloudZao")
            result = subprocess.run(["/usr/sbin/lsof", "-a", "-p", str(proc.pid), "-d", "0-999", "-Fft"],
                                    capture_output=True, text=True, timeout=3)
            pipes = result.stdout.count("tPIPE\n")
            samples.append(dict(boots=boots, pipe_fds=pipes, lsof_rc=result.returncode))
            if boots >= 4:
                break
    finally:
        if proc.poll() is None:
            proc.terminate()
        proc.wait(timeout=5)
(HERE / "gui-restart-results.json").write_text(json.dumps(dict(command=cmd, samples=samples,
    termination="SIGTERM sent only to this audit subprocess after bounded observation", exit_code=proc.returncode), indent=2) + "\n")
print(samples)
