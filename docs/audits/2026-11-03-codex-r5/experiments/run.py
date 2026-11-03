#!/usr/bin/env python3
"""Offline reproducible audit harness. All large/runtime files stay in build-audit/."""
import json
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
BUILD = ROOT / "build-audit"
os.chdir(ROOT)
SAN = "--asan" in sys.argv
PQ = "--pqbn" in sys.argv
BEFORE = "--before" in sys.argv
args = [s for s in sys.argv[1:] if s not in ("--asan", "--pqbn", "--before")]
libdir = BUILD / "before" if BEFORE else ROOT / "build-audit-asan" if SAN else BUILD
exe = BUILD / ("r5-probe-before" if BEFORE else "r5-probe-asan" if SAN else "r5-probe")
includes = ["platform/include", "system/include", "simulator", "simulator/desktop", "simulator/runtime", "simulator/kernel/vtk", "simulator/memory", "boards", "drivers/chips", "drivers/display/pqbn", "third_party/jsmn", "system/ui", "/Users/bytedance/Documents/Workspace/ink/build/_deps/lvgl-src"]
libs = ["ink_apps", "ink_system", "ink_lvgl", "ink_port_sim", "ink_board_metalio_plus", "ink_drivers", "ink_sim_core", "ink_tlsf", "ink_cjson", "ink_littlefs", "ink_stb"]
cmd = ["cc", "-O1" if SAN else "-O2", "-g", "-std=gnu11", "-mmacosx-version-min=15.7", "-DLV_CONF_INCLUDE_SIMPLE"]
if SAN: cmd += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]

if BEFORE: cmd += ["-DAUDIT_BEFORE"]
cmd += ["-I" + p for p in includes]
cmd += ["simulator/runtime/sim_main.c", str(HERE / "probe.c")]
cmd += [str(libdir / ("lib" + x + ".a")) for x in libs]
cmd += ["-lm", "-pthread", "-o", str(exe)]
compiled = subprocess.run(cmd, capture_output=True, text=True)
if compiled.returncode:
    print(compiled.stderr, file=sys.stderr)
    raise SystemExit(compiled.returncode)
names = args or ["storage_cache", "tca_shadow", "input_fault", "touch_blocks_power", "camera_cut", "camera_timeout", "audio_unpowered", "sleep_rails", "display_fault", "nt26_unicode", "r2_display_cut", "r2_camera_deadline", "r2_audio_sleep_race", "r2_wake_read_fault", "r2_timer_rearm_delete"]
results = []
for name in names:
    out = BUILD / "r5-probes" / (name + ("-pqbn" if PQ else "") + ("-asan" if SAN else "") + ("-before" if BEFORE else ""))
    out.mkdir(parents=True, exist_ok=True)
    cmd = [str(exe), "--profile", "simulator/profiles/metalio-plus-pqbn.json" if PQ else "simulator/profiles/metalio-plus.json", "--out", str(out), "--end", os.environ.get("AUDIT_END", "20s"), "--assets", str(BUILD / "assets"), "--content", str(BUILD / "test-content"), "--no-random-outages"]
    if name in ("r2_input_boot_fault", "r3_boot_audio"): cmd += ["--i2c-faults", "1:io-expander"]
    env = {**os.environ, "AUDIT_PROBE": name, "ASAN_OPTIONS": "detect_leaks=0"}
    with (out / "run.log").open("w") as log:
        try:
            p = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=40)
            rc = p.returncode
        except subprocess.TimeoutExpired:
            rc = "timeout"
    lines = (out / "run.log").read_text(errors="replace").splitlines()
    snippets = [s for s in lines if "AUDIT " in s or "AUD3 " in s or "AUD4 " in s or "AUD5 " in s or "AUD6 " in s or "runtime error:" in s or "ERROR: AddressSanitizer" in s or "SUMMARY: " in s]
    row = dict(name=name, sanitizer=SAN, pqbn=PQ, before=BEFORE, command=cmd, exit_code=rc, output=snippets)
    results.append(row)
    print(json.dumps(row, ensure_ascii=False), flush=True)
dest = HERE / "results.json"
old = json.loads(dest.read_text()) if dest.exists() else []
by_id = {(x["name"], x["sanitizer"], x.get("pqbn", False), x.get("before", False)): x for x in old}
by_id.update({(x["name"], x["sanitizer"], x.get("pqbn", False), x.get("before", False)): x for x in results})
dest.write_text(json.dumps(list(by_id.values()), ensure_ascii=False, indent=2) + "\n")
