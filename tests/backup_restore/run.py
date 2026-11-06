"""Device backup/clear/restore with a persistent synthetic hub; no external network or credentials."""
import argparse
import json
import hashlib
import struct
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--binary", type=Path)
p.add_argument("--quick", action="store_true")
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / "sim-backup-restore"
work = Path(tempfile.mkdtemp(prefix="backup-restore-", dir=build))
peer = work / "hub"
peer.mkdir()


def boot(area, phase, cut=None, server=None, length=None):
    area.mkdir(parents=True, exist_ok=True)
    server = server or peer
    server.mkdir(exist_ok=True)
    out = area / (phase + (f"-{cut}" if cut else ""))
    cmd = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"),
           "--out", str(out), "--end", "500s", "--quiet", "--no-random-outages",
           "--flash-image", str(area / "user.img"), "--sd-dir", str(area / "sd")]
    env = {**os.environ, "INK_BACKUP_PHASE": phase, "INK_SIM_BACKUP_DIR": str(server),
           "UBSAN_OPTIONS": "halt_on_error=1"}
    if length:
        env["INK_BACKUP_BYTES"] = str(length)
    if cut:
        env["INK_BACKUP_CUT"] = str(cut)
    run = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=90, env=env)
    out.mkdir(exist_ok=True)
    (out / "stdout.log").write_text(run.stdout)
    (out / "stderr.log").write_text(run.stderr)
    data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
    metrics = data.get("metrics", {})
    done = metrics.get("test.finished") == 1
    if phase in ("upload-cut", "restore-cut", "collect-cut"):
        done = metrics.get("test.checkpoint") == 1
    elif phase == "wipe":
        done = metrics.get("system.factory_resets") == 1 and metrics.get("power.reboot") == 1
    elif phase == "flash-cut":
        done = metrics.get("flash.torn_ops", 0) > 0 or metrics.get("backup.restored", 0) > 0
    ok = run.returncode == 0 and done and metrics.get("test.fail", 0) == 0 and not data.get("warnings", [])
    print(phase, cut or "", "PASS" if ok else "FAIL", "exit=" + str(run.returncode),
          "checks=" + str(metrics.get("test.pass", 0)), flush=True)
    if not ok:
        print(run.stderr[-6000:])
        print(json.dumps(data.get("warnings", []), ensure_ascii=False))
        print({k: metrics.get(k, 0) for k in ("test.finished", "test.fail", "backup.saved", "backup.restored", "backup.retries")})
        raise SystemExit(1)
    return metrics


def clone(source, label):
    target = work / label
    target.mkdir()
    shutil.copy2(source / "user.img", target / "user.img")
    shutil.copytree(source / "sd", target / "sd")
    return target


area = work / "roundtrip"
boot(area, "seed")
boot(clone(area, "receipt"), "receipt-reboot")
boot(area, "wipe")
boot(area, "rejoin")
empty = clone(area, "empty")
boot(area, "restore-cut")
boot(area, "restore-resume")
boot(area, "verify-reboot")
if not a.quick:
    api = work / "api"
    boot(api, "api")
    boot(api, "api-reboot")
    for label in ("foreign", "identity", "legacy", "traversal", "duplicate"):
        server = work / (label + "-hub")
        shutil.copytree(peer, server)
        member_dir = next(d for d in (server / "owner").iterdir() if (d / "latest").exists())
        sha, _ = (member_dir / "latest").read_text().split()
        manifest = bytearray((member_dir / (sha + ".blob")).read_bytes())
        if label == "foreign":
            manifest[12:28] = b"kid" + bytes(13)
        elif label == "identity":
            manifest[40:56] = bytes(16)
        elif label == "legacy":
            struct.pack_into("<I", manifest, 4, 1)
        elif label == "traversal":
            manifest[56:184] = b"../sys/unsafe" + bytes(115)
        else:
            manifest.extend(manifest[56:264])
            struct.pack_into("<I", manifest, 8, struct.unpack_from("<I", manifest, 8)[0] + 1)
        digest = hashlib.sha256(manifest).hexdigest()
        (member_dir / (digest + ".blob")).write_bytes(manifest)
        (member_dir / "latest").write_text(f"{digest} {len(manifest)}\n")
        boot(clone(empty, label), "malicious", server=server)
    for phase in ("stale", "member-confirm", "corrupt", "restore-network", "pin", "prepare-recover"):
        boot(clone(empty, phase), phase)
    for origin, label in ((area, "identity-local"), (empty, "identity-clean")):
        target = clone(origin, label)
        server = work / (label + "-hub")
        shutil.copytree(peer, server)
        boot(target, "reused-id", server=server)
        boot(target, "identity-reboot", server=server)
    boot(work / "deep", "deep", server=work / "deep-hub")
    for length in (2097152, 8388608):
        target = work / ("large-" + str(length))
        server = work / ("large-hub-" + str(length))
        boot(target, "large-seed", server=server, length=length)
        boot(target, "large-restore", server=server, length=length)
    boot(clone(empty, "legacy-journal"), "legacy-journal", server=work / "legacy-journal-hub")
    boot(work / "wrong-receipt", "wrong-receipt", server=work / "wrong-receipt-hub")
    for phase in ("collect-cut", "collect-member", "collect-held"):
        target = work / phase
        server = work / (phase + "-hub")
        boot(target, phase, server=server, length=2097152)
        if phase == "collect-cut":
            boot(target, "collect-resume", server=server, length=2097152)
    interrupted = work / "upload-restart"
    boot(interrupted, "upload-cut", server=work / "upload-hub")
    boot(interrupted, "upload-resume", server=work / "upload-hub")
    boot(work / "network", "network", server=work / "network-hub")
    boot(work / "member-resume", "member-resume", server=work / "member-hub")
    torn = 0
    for nth in (1, 8, 20, 45, 75, 120):
        target = clone(empty, f"flash-{nth}")
        metrics = boot(target, "flash-cut", nth)
        torn += metrics.get("flash.torn_ops", 0) > 0
        boot(target, "flash-verify")
    if torn < 3:
        raise SystemExit(f"FAIL: only {torn} flash cuts actually tore a write")
print("Evidence:", work.relative_to(ROOT))
