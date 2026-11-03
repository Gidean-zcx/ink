#!/usr/bin/env python3
"""Build audit-only probes against existing unmodified build-audit archives."""
from pathlib import Path
import subprocess
import os
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
os.chdir(ROOT)
includes = ["platform/include", "system/include", "ports/sim", "sim", "sim/vtk", "sim/mem",
            "drivers", "drivers/epd", "third_party/jsmn", "system/ui",
            "/Users/bytedance/Documents/Workspace/ink/build/_deps/lvgl-src"]
libs = ["ink_system", "ink_lvgl", "ink_port_sim", "ink_sim_core", "ink_tlsf", "ink_cjson",
        "ink_littlefs", "ink_stb"]
cmd = ["cc", "-O2", "-g", "-std=gnu11", "-mmacosx-version-min=15.7", "-DLV_CONF_INCLUDE_SIMPLE"]
cmd += ["-I" + p for p in includes]
cmd += ["ports/sim/sim_main.c", str(HERE / "probe.c")]
cmd += ["build-audit/lib" + x + ".a" for x in libs]
cmd += ["-lm", "-pthread", "-o", "build-audit/audit-probe"]
with (HERE / "probe-build.log").open("w") as f:
    f.write("Command: " + repr(cmd) + "\n")
    f.flush()
    subprocess.run(cmd, check=True, stdout=f, stderr=subprocess.STDOUT)
cmd = ["cc", "-O2", "-mmacosx-version-min=15.7", "-Iplatform/include", "-Isim/vtk", "-Isim/mem",
       "-Ithird_party/tlsf", str(HERE / "memory_probe.c"), "build-audit/libink_tlsf.a",
       "-o", "build-audit/audit-memory-probe"]
subprocess.run(cmd, check=True)
