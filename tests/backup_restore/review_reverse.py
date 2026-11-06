"""Isolated negative controls for the backup review fixes. Product archives/sources stay unchanged."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--evidence", type=Path, required=True, help="successful run.py evidence directory")
a = p.parse_args()
build, evidence = a.build.resolve(), a.evidence.resolve()
work = Path(tempfile.mkdtemp(prefix="backup-review-reverse-", dir=build))
flags = {}
for line in (build / "CMakeFiles/ink_system.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
link = shlex.split((build / "CMakeFiles/sim-backup-restore.dir/link.txt").read_text())
originals = {}
archive_hash = hashlib.sha256((build / "libink_system.a").read_bytes()).digest()


def command(args, log):
    r = subprocess.run(args, cwd=build, capture_output=True, text=True, timeout=90)
    log.write_text(r.stdout + r.stderr)
    if r.returncode:
        raise SystemExit(f"command exited {r.returncode}: {log.relative_to(ROOT)}")


def variant(label, filename, mutate):
    area = work / label
    area.mkdir()
    path = ROOT / filename
    originals[path] = path.read_bytes()
    source = mutate(originals[path].decode())
    if source == originals[path].decode():
        raise SystemExit(f"mutation not applied: {label}")
    altered = area / path.name
    altered.write_text(source)
    obj = area / (path.name + ".o")
    command([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
             "-I", str(path.parent), "-c", str(altered), "-o", str(obj)], area / "compile.log")
    archive = area / "libink_system.a"
    shutil.copy2(build / "libink_system.a", archive)
    command([shutil.which("ar"), "r", str(archive), str(obj)], area / "archive.log")
    args = list(link)
    binary = area / "sim-mutant"
    args[args.index("-o") + 1] = str(binary)
    for i, value in enumerate(args):
        if value.endswith("libink_system.a"):
            args[i] = str(archive)
    command(args, area / "link.log")
    return area, binary


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise SystemExit("mutation anchor is not unique: " + old[:90])
    return source.replace(old, new)


def boot(area, binary, phase, server, *, failure=None, warning=None, length=None):
    area.mkdir(exist_ok=True)
    server.mkdir(exist_ok=True)
    out = area / phase
    env = {**os.environ, "INK_BACKUP_PHASE": phase, "INK_SIM_BACKUP_DIR": str(server)}
    if length:
        env["INK_BACKUP_BYTES"] = str(length)
    args = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
            "--assets", str(build / "assets"), "--content", str(build / "test-content"),
            "--out", str(out), "--end", "500s", "--quiet", "--no-random-outages",
            "--flash-image", str(area / "user.img"), "--sd-dir", str(area / "sd")]
    r = subprocess.run(args, cwd=ROOT, env=env, capture_output=True, text=True, timeout=90)
    (area / (phase + ".log")).write_text(r.stdout + r.stderr)
    data = json.loads((out / "metrics.json").read_text())
    m, warnings = data["metrics"], "\n".join(data.get("warnings", []))
    rejected = m.get("test.fail", 0) > 0 if failure else False
    if failure:
        rejected = rejected and failure in warnings
    if warning:
        rejected = warning in warnings
    if r.returncode or m.get("test.finished") != 1 or ((failure or warning) and not rejected) or (not failure and not warning and (m.get("test.fail", 0) or warnings)):
        raise SystemExit(f"control not demonstrated: {area.name}/{phase}, see {out.relative_to(ROOT)}")
    print(area.name, phase, "rejected" if failure or warning else "setup passed",
          "failures=" + str(m.get("test.fail", 0)), "timeouts=" + str(m.get("cap.timeouts", 0)), flush=True)


def clone(source, target):
    target.mkdir()
    shutil.copy2(source / "user.img", target / "user.img")
    shutil.copytree(source / "sd", target / "sd")


members = "system/policy/members.c"
backup = "system/services/backup/backup_service.c"
area, binary = variant("reusable-identity", members, lambda s: replace_once(
    s, '            hal_random_fill(s->identity, sizeof s->identity);\n            for (unsigned i = 0; i < 4; i++) s->identity[i] = (uint8_t)(serial >> (i * 8));',
    '            uint8_t hash[32]; ink_sha256(id, strlen(id), hash); memcpy(s->identity, hash, 16);'))
server = area / "hub"
boot(area, binary, "seed", server)
boot(area, binary, "receipt-reboot", server)
boot(area, binary, "reused-id", server, failure="replacement child cannot preview removed adult snapshot")

area, binary = variant("identity-not-persisted", members, lambda s: replace_once(
    s, "    memcpy(r.identity, s->identity, sizeof r.identity);", "    memset(r.identity, 0, sizeof r.identity);"))
server = area / "hub"
boot(area, binary, "seed", server)
boot(area, binary, "receipt-reboot", server, failure="upload receipt survives reboot")

area, binary = variant("manifest-identity", backup, lambda s: replace_once(
    s, " || memcmp(B.m->identity, B.j.identity, 16)", ""))
server = area / "hub"
shutil.copytree(evidence / "identity-hub", server)
clone(evidence / "empty", area / "device")
boot(area / "device", binary, "malicious", server, failure="foreign member or unsafe path rejected before preview")

area, binary = variant("receipt-identity", backup, lambda s: replace_once(
    s, '    if (ink_json_str(j, ink_json_find(j, 0, "identity"), received, sizeof received) != 32 || strcmp(received, identity)) return -2;',
    '    (void)received;'))
boot(area, binary, "wrong-receipt", area / "hub", failure="wrong identity in transfer receipt rejected")

# Restore the previous recursive directory/copy implementation inside the capture phase.
old = subprocess.check_output(["git", "show", "cf258b3b:" + backup], cwd=ROOT, text=True)
old_hash = old[old.index("static int hash_file("):old.index("\nstatic bool physical")].replace("hash_file(", "sync_hash_file(")
old_walk = old[old.index("static int copy_personal("):old.index("\ntypedef struct { char name[ITEMS]")].replace("hash_file(", "sync_hash_file(")


def recursive(s):
    s = replace_once(s, "static int capture_step(void) {", old_hash + "\n" + old_walk + "\nstatic int capture_step(void) {")
    return replace_once(s, "    int rc = 0;\n    if (C->mode == 0)", """    int rc = 0;
    if (C->mode == 3) {
        while (C->depth) hal_fs_closedir(C->dirs[--C->depth]);
        rc = walk_personal("", 0);
        if (rc) return -2;
        C->mode = 4; memcpy(&C->manifest, B.m, sizeof *B.m); return 1;
    }
    if (C->mode == 0)""")


area, binary = variant("recursive-capture", backup, recursive)
boot(area, binary, "deep", area / "hub", warning="may overflow its 8192-byte stack")
boot(area / "large", binary, "large-seed", area / "large-hub", length=8388608,
     failure="progress query stays responsive during large-file work")


def synchronous_hash(s):
    start, end = s.index("static int hash_file("), s.index("\nstatic bool physical")
    function = old_hash.replace("sync_hash_file(", "hash_file(").replace(
        "    int64_t len = hal_fs_size(path);", "    H.revision = ink_fs_revision();\n    int64_t len = hal_fs_size(path);")
    return s[:start] + function + s[end:]


area, binary = variant("synchronous-hash", backup, synchronous_hash)
boot(area, binary, "large-seed", area / "hub", length=8388608)
boot(area, binary, "large-restore", area / "hub", length=8388608,
     failure="progress query stays responsive during large-file work")

area, binary = variant("revision-cas", "system/services/file/file_service.c", lambda s: replace_once(
    s, "    if (!r && revision && !inverse_ready(revision)) r = INK_FS_ERR_EXISTS;", "    if (false) r = INK_FS_ERR_EXISTS;"))
boot(area, binary, "api", area / "hub", failure="revision restore refuses intervening same-size edit")

def no_preview_recovery(s):
    start = s.index('    if (load_manifest("preview", false)) {', s.index("static int restore_step("))
    end = s.index("    if (B.j.next > B.m->count)", start)
    return s[:start] + '    if (load_manifest("preview", false)) return -2;\n' + s[end:]


area, binary = variant("preview-checkpoint", backup, no_preview_recovery)
clone(evidence / "empty", area / "device")
boot(area / "device", binary, "prepare-recover", evidence / "hub",
     failure="unapproved partial preview rebuilds from identity-checked remote manifest")

if hashlib.sha256((build / "libink_system.a").read_bytes()).digest() != archive_hash:
    raise SystemExit("product archive changed")
for path, original in originals.items():
    if path.read_bytes() != original:
        raise SystemExit("product source changed: " + str(path.relative_to(ROOT)))
print("Evidence:", work.relative_to(ROOT))
