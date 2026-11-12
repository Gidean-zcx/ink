#!/usr/bin/env python3
"""Compile an unchanged upstream UI unit against declarations, never firmware."""
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
DEPS = Path(os.environ["INK_DEPS"])
UP = DEPS.parent / "ink-hw/upstream/oss/kiikoread"
REV = "83942eb45d13064a47aca7ff04c2faf8fd51f98a"
OUT = ROOT / "work/replan-arch/nav-probe"
OUT.mkdir(parents=True, exist_ok=True)
(OUT / "src").mkdir(exist_ok=True)
(OUT / "include").mkdir(exist_ok=True)
assert subprocess.check_output(["git", "-C", str(UP), "rev-parse", "HEAD"], text=True).strip() == REV

sources = {}
for rel in ("main/ui/ui_nav.c", "main/ui/ui_nav.h", "main/ui/ui_nav_layout.h"):
    data = (UP / rel).read_bytes()
    assert data == subprocess.check_output(["git", "-C", str(UP), "show", REV + ":" + rel])
    (OUT / "src" / Path(rel).name).write_bytes(data)
    sources[rel] = {"sha256": hashlib.sha256(data).hexdigest(), "lines": len(data.splitlines())}
shutil.copyfile(HERE / "compat.h", OUT / "include/compat.h")
for name in ("app.h", "app_registry.h", "read_pico_pmu.h", "read_pico_pmu_protocol.h",
             "read_pico_transfer.h", "ui_kit.h", "settings.h", "ui_click_feedback.h",
             "metalio_hw.h", "ttf_font.h", "ui_font.h"):
    (OUT / "include" / name).write_text('#include "compat.h"\n')
shutil.copyfile(UP / "licenses/Apache-2.0.txt", OUT / "Apache-2.0.txt")

def clean(text):
    return text.replace(str(UP), "$INK_DEPS/../ink-hw/upstream/oss/kiikoread").replace(str(ROOT), "$REPO")

def run(args, expected_success):
    result = subprocess.run(["taskpolicy", "-c", "utility", *args], cwd=OUT, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    item = {"command": clean(shlex.join(["taskpolicy", "-c", "utility", *args])),
            "exit": result.returncode, "output": clean(result.stdout)}
    assert (result.returncode == 0) == expected_success, item
    return item

cc = os.environ.get("CC", "cc")
flags = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-DKIIKO_BOARD_METALIO47=1"]
results = [run(flags + ["-I" + str(UP / "main/app"), "-fsyntax-only", str(UP / "main/ui/ui_nav.c")], False),
           run(flags + ["-Iinclude", "-Isrc", "-c", "src/ui_nav.c", "-o", "ui_nav.o"], True),
           run(flags + ["-DINK_ARCH_OMIT_NETWORK=1", "-Iinclude", "-Isrc",
                        "-fsyntax-only", "src/ui_nav.c"], False),
           run(["nm", "-u", "ui_nav.o"], True)]
symbols = sorted({line.split()[-1].removeprefix("_") for line in results[-1]["output"].splitlines() if line.strip()})
report = {"source_revision": REV, "sources": sources,
          "compiler": clean(subprocess.check_output([cc, "--version"], text=True).splitlines()[0]),
          "compat_header_lines": len((HERE / "compat.h").read_bytes().splitlines()),
          "undefined_symbols": symbols, "runs": results,
          "boundary": "Compile-only: no link, graphics, service implementation, product build or device execution."}
(OUT / "result.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(report, ensure_ascii=False, indent=2))
