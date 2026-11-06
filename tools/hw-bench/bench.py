#!/usr/bin/env python3
"""One command: check the backup, install the bench into ota_1, let it measure, restore, verify, report.

Without --approved it only checks the backup and prints what it would do; nothing touches the device.
With it, a failure at any point after writing still restores. Raw logs stay in ink-hw, outside the repo.
"""
import argparse, datetime, hashlib, os, shutil, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
HW = os.path.join(ROOT, "ink-hw")
BACKUP = os.path.join(HW, "bringup-20261103", "private", "flash_full_1.bin")
SECOND = os.path.expanduser("~/ink-hw-backup/bringup-20261103/flash_full_1.bin")
BACKUP_SHA = "ba58546360dba4fa"
OTADATA = (0xD000, 0x2000)


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def check_backup():
    for path in (BACKUP, SECOND):
        if not os.path.exists(path):
            sys.exit(f"no backup at {path}: nothing will be written")
        digest = sha(path)
        if not digest.startswith(BACKUP_SHA):
            sys.exit(f"backup {path} is not the verified one: nothing will be written")
    print(f"backup verified twice: sha256 {BACKUP_SHA}...")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--approved", action="store_true", help="the user agreed to docs/hardware/phase2-plan.md")
    ap.add_argument("--port", default="/dev/cu.usbmodem*")
    ap.add_argument("--seconds", type=float, default=420.0)
    ap.add_argument("--lab", action="store_true", help="the image that tries our own refresh on the panel")
    a = ap.parse_args()
    check_backup()
    image = os.path.join(HERE, "build-lab" if a.lab else "build", "ink-hw-bench.bin")
    if not os.path.exists(image):
        sys.exit("build it first: tools/hw-bench/build.sh" + (" lab" if a.lab else ""))
    print(f"image {image}: {os.path.getsize(image)} bytes, sha256 {sha(image)[:16]}...")
    print("writes only ota_1 (0x580000) and otadata (0xd000); restores both and verifies against the backup")
    if not a.approved:
        print("dry run: nothing touched the device")
        return
    work = os.path.join(HW, "bench-" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
    os.makedirs(work)
    shutil.copy(image, os.path.join(work, "ink-hw-probe.bin"))
    with open(BACKUP, "rb") as f:
        f.seek(OTADATA[0])
        factory = f.read(OTADATA[1])
    open(os.path.join(work, "otadata_factory.bin"), "wb").write(factory)
    py = os.path.join(HW, "venv", "bin", "python")
    tool = os.path.join(HERE, "..", "hw-probe")
    subprocess.check_call([py, os.path.join(tool, "make_otadata.py"), os.path.join(work, "otadata_factory.bin"),
                           "--slot", "1", "--slots", "2", "-o", os.path.join(work, "otadata_ota1.bin")])
    flash = [py, os.path.join(tool, "flash_probe.py")]
    common = ["--port", a.port, "--dir", work]
    installed = subprocess.call(flash + ["install"] + common + ["--log-seconds", str(a.seconds), "--until", "BENCH_DONE", "--after", "BENCH_REPLAY",
                                                                "--approved"])
    print("install exit", installed)
    # The USB link can die while the panel starts, and then only replugging brings it back (second run).
    for attempt in range(40):
        restored = subprocess.call(flash + ["restore"] + common + ["--backup", BACKUP, "--approved"])
        if restored == 0:
            break
        print("USB_STUCK: cannot reach the chip to restore; replug the USB cable. Trying again.", flush=True)
        time.sleep(10)
    verified = subprocess.call(flash + ["verify"] + common + ["--backup", BACKUP]) if restored == 0 else 1
    console = os.path.join(work, "probe_console.txt")
    if os.path.exists(console):
        day = datetime.date.today().isoformat()
        out = os.path.join(ROOT, "ink", "docs", "hardware", f"{day}-bench.md")
        for n in range(2, 20):
            if not os.path.exists(out):
                break
            out = os.path.join(ROOT, "ink", "docs", "hardware", f"{day}-bench-{n}.md")
        subprocess.call([sys.executable, os.path.join(HERE, "report.py"), console, "-o", out,
                         "--restored", "yes" if verified == 0 else "NO"])
        print("report", out)
    if verified != 0:
        sys.exit("RESTORE NOT VERIFIED: stop and look; the backup and docs/hardware/phase2-plan.md say what to do")
    print("device restored and verified")


if __name__ == "__main__":
    main()
