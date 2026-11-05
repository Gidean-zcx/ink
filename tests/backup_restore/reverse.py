"""Build isolated mutant archives to prove the restore guards; never change product sources/build archives."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", required=True, type=Path)
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix="backup-reverse-", dir=build))
cache = (build / "CMakeCache.txt").read_text()
compiler = next(line.split("=", 1)[1] for line in cache.splitlines() if line.startswith("CMAKE_C_COMPILER:FILEPATH="))
archive_tool = shutil.which("ar")
if not archive_tool:
    raise SystemExit("ar is required")
flags = {}
for line in (build / "CMakeFiles/ink_system.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
compile_flags = flags["C_DEFINES"] + flags["C_INCLUDES"] + flags["C_FLAGS"]
link = shlex.split((build / "CMakeFiles/sim-backup-restore.dir/link.txt").read_text())

cases = [
    ("revision", "system/services/data/datastore.c", "db->last_ver != expected_ver", "false",
     "api", "stale expected revision"),
    ("log-generation", "system/services/data/datastore.c", "h.log_gen = old_gen + 2;", "h.log_gen = old_gen;",
     "api-reboot", "old higher-version logs"),
    ("storage-cas", "system/services/storage/storage_service.c", "if (memcmp(hash, j->expected, 32)) return -3;", "if (false) return -3;",
     "api", "storage queue checks hash"),
    ("file-cas", "system/services/file/file_service.c", "if (!r && memcmp(hash, expected_sha256, 32)) r = INK_FS_ERR_EXISTS;", "if (false) r = INK_FS_ERR_EXISTS;",
     "api", "card edit lock refuses"),
]


def run(cmd, log):
    result = subprocess.run(cmd, cwd=build, capture_output=True, text=True, timeout=90)
    log.write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(f"command failed ({result.returncode}), see {log.relative_to(ROOT)}")


for label, source_name, old, new, failing_phase, evidence in cases:
    directory = work / label
    directory.mkdir()
    original_path = ROOT / source_name
    original = original_path.read_bytes()
    source = original.decode()
    if source.count(old) != 1:
        raise SystemExit(f"mutation anchor is ambiguous: {label}")
    mutant_source = directory / original_path.name
    mutant_source.write_text(source.replace(old, new))
    obj = directory / (original_path.name + ".o")
    run([compiler, *compile_flags, "-I", str(original_path.parent), "-c", str(mutant_source), "-o", str(obj)], directory / "compile.log")
    archive = directory / "libink_system.a"
    shutil.copy2(build / "libink_system.a", archive)
    run([archive_tool, "r", str(archive), str(obj)], directory / "archive.log")
    binary = directory / "sim-mutant"
    command = list(link)
    command[command.index("-o") + 1] = str(binary)
    replaced = 0
    for i, token in enumerate(command):
        if token.endswith("libink_system.a"):
            command[i] = str(archive)
            replaced += 1
    if replaced != 1:
        raise SystemExit("link must reference the product archive exactly once")
    run(command, directory / "link.log")
    phases = ("api", "api-reboot") if failing_phase == "api-reboot" else ("api",)
    for phase in phases:
        out = directory / phase
        cmd = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
               "--assets", str(build / "assets"), "--content", str(build / "test-content"),
               "--out", str(out), "--end", "90s", "--quiet", "--no-random-outages",
               "--flash-image", str(directory / "user.img"), "--sd-dir", str(directory / "sd")]
        result = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=90,
                                env={**os.environ, "INK_BACKUP_PHASE": phase})
        (directory / (phase + ".log")).write_text(result.stdout + result.stderr)
        data = json.loads((out / "metrics.json").read_text())
        failures = data["metrics"].get("test.fail", 0)
        warnings = "\n".join(data.get("warnings", []))
        if result.returncode or (phase != failing_phase and failures) or (phase == failing_phase and (not failures or evidence not in warnings)):
            raise SystemExit(f"negative control not demonstrated: {label}/{phase}")
        print(label, phase, "guard removal rejected" if phase == failing_phase else "setup passed", f"checks_failed={failures}", flush=True)
    if original_path.read_bytes() != original:
        raise SystemExit("product source unexpectedly changed")
print("Evidence:", work.relative_to(ROOT))
