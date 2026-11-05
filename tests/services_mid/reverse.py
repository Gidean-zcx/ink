"""Run regressions against read-only copies of the pre-fix sources."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--build", default="build")
args = parser.parse_args()
build = (ROOT / args.build).resolve()
work = Path(tempfile.mkdtemp(prefix="services-mid-reverse-", dir=build))

def old_source(revision, path):
    target = work / Path(path).name
    target.write_bytes(subprocess.check_output(["git", "show", revision + ":" + path], cwd=ROOT))
    return target

def run(label, command, timeout=180):
    result = subprocess.run(command, cwd=build, capture_output=True, text=True, timeout=timeout,
                            env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
    output = result.stdout + result.stderr
    (work / (label + ".log")).write_text(output)
    print(label, "exit=" + str(result.returncode), flush=True)
    return result.returncode, output

source = old_source("2c844243", "simulator/environment/hub_bridge.c")
flags = {}
target_dir = build / "CMakeFiles/hub-bridge-lifecycle.dir"
for line in (target_dir / "flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
link = shlex.split((target_dir / "link.txt").read_text())
obj = work / "bridge.o"
compile_command = [link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                   "-I" + str(ROOT / "simulator/environment"),
                   '-DINK_TEST_BRIDGE_SOURCE="' + str(source) + '"',
                   "-c", str(ROOT / "tests/hub/bridge_lifecycle.c"), "-o", str(obj)]
subprocess.run(compile_command, cwd=build, check=True)
link = [str(obj) if word.endswith("/bridge_lifecycle.c.o") else word for word in link]
link[link.index("-o") + 1] = str(work / "bridge")
subprocess.run(link, cwd=build, check=True)
code, output = run("R4", [str(work / "bridge")], timeout=10)
if code != 1 or "expected=0" not in output or "FAIL line" not in output:
    raise SystemExit("R4 did not reproduce the unreclaimed connection")

design = old_source("2c844243", "docs/system/system-services-design.md")
code, output = run("AUD18-404", [sys.executable, str(ROOT / "tests/services_mid/check_capability_doc.py"), "--design", str(design)])
if code != 1 or not all(value in output for value in ("missing=8", "job.running", "job.finished")):
    raise SystemExit("AUD18-404 did not reproduce the document omissions")

code, output = run("existing-fixes", [sys.executable, str(ROOT / "tests/services_finish/run.py"),
                    "--build", str(build), "--baseline", "6f689264^",
                    "update_history", "offpanel", "text_window", "keyboard_snapshot"])
if code != 1 or "Evidence directory:" not in output:
    raise SystemExit("Existing service reverse checks did not finish normally")
for suite in ("update_history", "offpanel", "text_window", "keyboard_snapshot"):
    if not any(line.startswith(suite) and "FAIL" in line and "exit=0" in line for line in output.splitlines()):
        raise SystemExit(suite + " did not detect the old behavior")
print("PASS: all six reverse checks rejected the old behavior")
print("Evidence directory:", work.relative_to(ROOT))
