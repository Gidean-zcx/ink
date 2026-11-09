"""Prove the existing power-cut regression catches the original startup order."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / "factory-review-reverse"
area.mkdir(exist_ok=True)
entry = ROOT / "system/boot/main.c"
before = hashlib.sha256(entry.read_bytes()).hexdigest()
source = entry.read_text()
fixed = "    ink_backup_start(); /* Replay only after all collection definitions are registered. */\n"
old_site = "    ink_pairing_start();\n"
assert source.count(fixed) == source.count(old_site) == 1
main = area / "main.c"
main.write_text(source.replace(fixed, "").replace(old_site, old_site + "    ink_backup_start();\n"))
product = (ROOT / "tests/services_finish/product.h").read_text().replace(
    '#include "../../system/boot/main.c"', f'#include "{main}"')
unit = ROOT / "tests/backup_restore/device.c"
source = unit.read_text().replace('#include "../services_finish/product.h"', product)
flags = (build / "CMakeFiles/sim-backup-restore.dir/flags.make").read_text()
options = []
for key in ("C_DEFINES", "C_INCLUDES", "C_FLAGS"):
    options += shlex.split(re.search("^" + key + r" = (.*)$", flags, re.M)[1])
link = shlex.split((build / "CMakeFiles/sim-backup-restore.dir/link.txt").read_text())
obj = area / "device.o"
command = [link[0], *options, "-iquote", str(unit.parent), "-x", "c", "-", "-c", "-o", str(obj)]
subprocess.run(command, cwd=build, input=source, text=True, check=True)
index = next(i for i, part in enumerate(link) if part.endswith("/tests/backup_restore/device.c.o"))
link[index] = str(obj)
binary = area / "sim-backup-old-order"
link[link.index("-o") + 1] = str(binary)
subprocess.run(link, cwd=build, check=True)
result = subprocess.run([sys.executable, str(ROOT / "tests/backup_restore/run.py"),
    "--build", str(build), "--binary", str(binary)], cwd=ROOT, text=True,
    capture_output=True, timeout=240, env={**os.environ, "INK_KEEP_EVIDENCE": "1"})
(area / "backup-restore.log").write_text(result.stdout + result.stderr)
assert result.returncode == 1, result.stdout + result.stderr
assert "flash-cut 45 PASS" in result.stdout and "flash-verify  FAIL" in result.stdout, result.stdout
assert "torn-flash restore finishes" in result.stdout and "预览后资料有改动" in result.stdout, result.stdout
data = Path(re.search(r"report (.+)/flash-45/flash-verify/metrics.json", result.stdout)[1])
target = area / "same-cut"
if target.exists():
    shutil.rmtree(target)
target.mkdir()
shutil.copy2(data / "empty/user.img", target / "user.img")
shutil.copytree(data / "empty/sd", target / "sd")
for phase, executable in (("flash-cut", binary), ("flash-verify", build / "sim-backup-restore")):
    out = target / phase
    command = [str(executable), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--out", str(out),
        "--end", "500s", "--quiet", "--no-random-outages", "--flash-image", str(target / "user.img"),
        "--sd-dir", str(target / "sd")]
    replay = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90,
        env={**os.environ, "INK_BACKUP_PHASE": phase, "INK_BACKUP_CUT": "45", "INK_SIM_BACKUP_DIR": str(data / "hub")})
    (area / (phase + ".log")).write_text(replay.stdout + replay.stderr)
    metrics = json.loads((out / "metrics.json").read_text())
    assert replay.returncode == 0 and not metrics.get("warnings"), replay.stderr + str(metrics.get("warnings"))
    m = metrics["metrics"]
    assert m.get("test.fail", 0) == 0, m
    if phase == "flash-cut":
        assert m.get("flash.torn_ops", 0) == 1, m
        (area / "torn-image.sha256").write_text(hashlib.sha256((target / "user.img").read_bytes()).hexdigest() + "\n")
    else:
        assert m.get("test.finished") == 1 and m.get("backup.restored") == 1, m
assert hashlib.sha256(entry.read_bytes()).hexdigest() == before
print("original startup order: flash-45 restore fails as expected; product source unchanged")
print("fixed startup order: the same flash image torn by the original binary restores successfully")
