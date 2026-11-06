"""Build isolated mutant archives to prove the restore guards; never change product sources/build archives."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
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
    ("cleanup-not-queued", "system/services/data/datastore.c", "h.log_gen = old_gen + 2;", "h.log_gen = old_gen + 2;",
     None, ""),
    ("log-generation", "system/services/data/datastore.c", "h.log_gen = old_gen + 2;", "h.log_gen = old_gen;",
     "api-reboot", "old higher-version logs"),
    ("storage-cas", "system/services/storage/storage_service.c", "if (memcmp(hash, j->expected, 32)) return -3;", "if (false) return -3;",
     "api", "storage queue checks hash"),
    ("file-cas", "system/services/file/file_service.c", "if (!r && memcmp(hash, expected_sha256, 32)) r = INK_FS_ERR_EXISTS;", "if (false) r = INK_FS_ERR_EXISTS;",
     "api", "card edit lock refuses"),
    ("directory-member", "system/services/file/file_service.c",
     "int ink_fs_mkdir_member_generation(const char *member, uint32_t generation, const char *path) {\n    file_context_t c = member_context(member, generation);",
     "int ink_fs_mkdir_member_generation(const char *member, uint32_t generation, const char *path) {\n    file_context_t c = current_context();",
     "api", "new member receives no directory"),
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
    source = source.replace(old, new)
    if label in ("cleanup-not-queued", "log-generation"):
        # Cleanup may legitimately fail to queue. A committed snapshot must stand on its own.
        source = source.replace("if (log_path(db, old_snap_gen,", "if (false && log_path(db, old_snap_gen,")
        source = source.replace("if (old_gen != old_snap_gen && log_path(db, old_gen,", "if (false && old_gen != old_snap_gen && log_path(db, old_gen,")
    mutant_source.write_text(source)
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
    phases = ("api", "api-reboot") if failing_phase == "api-reboot" or failing_phase is None else ("api",)
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

# Recreate the idle-task resource regression against the existing whole-product budget.
directory = work / "idle-start"
directory.mkdir()
source_path = ROOT / "system/services/backup/backup_service.c"
original = source_path.read_bytes()
source = original.decode()
anchor = "if (pending_for(NULL)) wake(NULL);"
if source.count(anchor) != 1:
    raise SystemExit("idle start mutation anchor is ambiguous")
mutant = directory / source_path.name
mutant.write_text(source.replace(anchor, "wake(NULL);"))
obj = directory / (source_path.name + ".o")
run([compiler, *compile_flags, "-c", str(mutant), "-o", str(obj)], directory / "compile.log")
archive = directory / "libink_system.a"
shutil.copy2(build / "libink_system.a", archive)
run([archive_tool, "r", str(archive), str(obj)], directory / "archive.log")
command = shlex.split((build / "CMakeFiles/ink-sim.dir/link.txt").read_text())
binary = directory / "sim-mutant"
command[command.index("-o") + 1] = str(binary)
for i, token in enumerate(command):
    if token.endswith("libink_system.a"):
        command[i] = str(archive)
run(command, directory / "link.log")
configured_ctest = next(line.split("=", 1)[1] for line in cache.splitlines() if line.startswith("CMAKE_CTEST_COMMAND:"))
ctest = next((c for c in (shutil.which("ctest"), str(Path(sys.executable).with_name("ctest")), configured_ctest) if c and Path(c).is_file()), None)
if not ctest:
    raise SystemExit("use the Python environment containing ctest")
registry = json.loads(subprocess.check_output([ctest, "--test-dir", str(build), "--show-only=json-v1"], text=True))
scenario = next(t for t in registry["tests"] if t["name"] == "scenario-ai_ask-metalio-plus-pqbn")
command = list(scenario["command"])
command[0] = str(binary)
out = directory / "out"
command[command.index("--out") + 1] = str(out)
result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=90)
(directory / "budget.log").write_text(result.stdout + result.stderr)
metrics = json.loads((out / "metrics.json").read_text())["metrics"]
minimum = metrics["mem.internal_total.min_free_bytes"]
if result.returncode != 1 or minimum >= 32768 or source_path.read_bytes() != original:
    raise SystemExit("idle-start resource regression was not rejected")
print("idle-start whole-product budget rejected", f"internal_min={minimum}, required=32768", flush=True)
print("Evidence:", work.relative_to(ROOT))
