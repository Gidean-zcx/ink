#!/usr/bin/env python3
"""Run a trial build of the whole system on the device, then put the device back exactly as the backup has it.

Writes ota_1 (the app), font_data (the font pack built with that app) and an otadata that selects ota_1. While it
runs the firmware itself writes model (/user), the log ring at the end of font_data and, after a crash, coredump.
Afterwards otadata, ota_1, model, font_data and coredump are all written back from the verified backup and checked
byte for byte, the factory firmware is booted and the chip is parked in download mode (it does not sleep there).
Never an erase, never ota_0, nvs, phy_init, resources, the bootloader or the partition table
(docs/hardware/flash-layout.md). The device runs the factory firmware only between runs; writing needs the user's
consent (CLAUDE.md).

  trial_run.py --port '/dev/cu.usbmodem*' --backup flash_full.bin --image ink-s31.bin --fonts ink-fonts.bin \\
               --out DIR --seconds N [--until TEXT]

--install writes and verifies the trial build and leaves it running (the delivery; the backup stays the way back).
  Then it serves the card files (D-138): the install bundle next to the image (or --card-files DIR) goes onto the card
  over the console (DIR/card_files.txt). Without the bundle it stops before writing, unless --no-card-files.
--restore-only writes the five regions back from the backup, checks them and boots the factory firmware.
--card-files-only [--reset] only serves the card files to the running build: no backup, no image, no esptool.

--image needs --fonts, the pack built with that image: otherwise whatever font_data the device holds stays, and an
app reads only the pack of its own build (images before font-pack-1 cannot read the compressed INKF v2 fonts, so
they draw no Chinese, and look for the icons in font_data, which a pack since then no longer holds).

Exit codes: 0 ran, restored and verified (--install: installed and the card files all there); 1 restore NOT verified
(stop and look); 2 stopped before writing anything; 3 restored and verified, but the factory boot line was not seen or
parking failed; 4 --install: written and verified but the console showed no system start; 5 installed (or served) and
running, but the card files are not all on the card (see card_files.txt).
"""
import argparse
import hashlib
import json
import os
import shlex
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "hw-selftest"))
import run as base  # noqa: E402  the self-test runner's serial, otadata and esptool helpers
sys.path.insert(0, HERE)
import card_files_serve  # noqa: E402  no pyserial or esptool at import

MODEL = (0x10000, 0x64000)
FONT_DATA = (0xAE4000, 0x500000)
FONT_PACK_MAX = 0x480000  # the log ring has the last 512 KiB of font_data
COREDUMP = (0xFE4000, 0x10000)
REGIONS = {"otadata": base.OTADATA, "ota_1": base.OTA_1, "model": MODEL, "font_data": FONT_DATA, "coredump": COREDUMP}
EXPECT = {"model": (0x01, 0x82), "font_data": (0x01, 0x40), "coredump": (0x01, 0x03)}
base.WRITABLE = tuple(REGIONS.values())  # base.esptool_cmd refuses anything outside these


def check_bundle(a):
    try:
        with open(os.path.join(a.card_files or "", "manifest.json")) as f:
            json.load(f)
    except (OSError, ValueError) as e:
        raise base.Stop(f"no install bundle (card files) at {a.card_files}: {e}; give --card-files DIR or --no-card-files")


def open_console(a, reset):
    """Opens the console without a reset (one RTS pulse first with reset): pyserial only from here."""
    sys.path.insert(0, base.BRINGUP)
    import serial_log  # noqa: E402  (pyserial)
    pending = [reset]

    def opener():
        path = serial_log.find_port(a.port)
        s = serial_log.open_port(path, 115200) if path else None
        if s is not None and pending[0]:
            serial_log.reset(s)
            pending[0] = False
        return s
    return opener


def serve_card_files(r, reset=False):
    r.step(f"card files: serve the install bundle {r.a.card_files}")
    if r.a.dry_run:
        r.say("would serve the card files (dry run)")
        return "all-present"
    with open(os.path.join(r.a.out, "card_files.txt"), "a", buffering=1, encoding="utf-8") as log:
        result = card_files_serve.Server(r.a.card_files).serve(open_console(r.a, reset), log)
    r.say(f"card files: {result}")
    return result


def card_files_ok(result):
    return result == "all-present" or (result.startswith("done ok=") and result.endswith(" failed=0"))


def prepare(r):
    a = r.a
    r.step("1. check the backup, its partition table and the images")
    if a.install and not a.no_card_files:
        check_bundle(a)
    if os.path.getsize(a.backup) != base.FLASH_SIZE:
        raise base.Stop(f"backup is {os.path.getsize(a.backup)} bytes, expected {base.FLASH_SIZE}")
    with open(a.backup, "rb") as f:
        backup = f.read()
    parts = base.parse_partitions(backup[base.PT_OFFSET:base.PT_OFFSET + base.PT_SIZE])
    base.check_layout(parts)
    for name, (ptype, sub) in EXPECT.items():
        hits = [p for p in parts if p["label"] == name and p["type"] == ptype and p["subtype"] == sub]
        if len(hits) != 1 or (hits[0]["offset"], hits[0]["size"]) != REGIONS[name]:
            raise base.Stop(f"{name} is not where docs/hardware/flash-layout.md says")
    if base.boot_slot(backup[base.OTADATA[0]:base.OTADATA[0] + base.OTADATA[1]]) != 0:
        raise base.Stop("the backup's otadata does not boot ota_0")
    image = b"\xe9"
    if a.image:
        with open(a.image, "rb") as f:
            image = f.read()
    if not image or image[0] != 0xE9 or len(image) > base.OTA_1[1]:
        raise base.Stop("the app image is not an ESP app image that fits ota_1")
    fonts = None
    if a.fonts:
        with open(a.fonts, "rb") as f:
            fonts = f.read()
        if not fonts or len(fonts) > FONT_PACK_MAX:
            raise base.Stop(f"the font pack is {len(fonts)} bytes; at most {FONT_PACK_MAX}")
    r.say("partition table as expected; image %d bytes sha256 %s...%s" % (
        len(image), hashlib.sha256(image).hexdigest()[:16],
        "" if fonts is None else "; fonts %d bytes sha256 %s..." % (len(fonts), hashlib.sha256(fonts).hexdigest()[:16])))

    r.step("2. copy the regions out of the backup")
    p = {}
    for name, (start, size) in REGIONS.items():
        p[name] = os.path.join(a.out, f"{name}_backup.bin")
        with open(p[name], "wb") as f:
            f.write(backup[start:start + size])
    p["select"] = os.path.join(a.out, "otadata_ota1.bin")
    with open(p["select"], "wb") as f:
        f.write(base.select_ota1(backup[base.OTADATA[0]:base.OTADATA[0] + base.OTADATA[1]]))
    p["image"], p["fonts"] = a.image, a.fonts
    return p


def install_and_run(r, p):
    r.step("3. write the app (and fonts), select ota_1")
    port = base.wait_port_idle(r, "install")
    if port is None:
        raise RuntimeError("no port")
    r.a.port_now = port
    r.touched = True
    pairs = [(base.OTA_1[0], p["image"])] + ([(FONT_DATA[0], p["fonts"])] if p["fonts"] else []) + \
            [(base.OTADATA[0], p["select"])]
    if r.esptool("no-reset", "write-flash", pairs):
        raise RuntimeError("write failed")
    if r.esptool("hard-reset", "verify-flash", pairs):
        raise RuntimeError("verify after writing failed")
    r.step(f"4. console for {r.a.seconds:.0f} s" + (f" or until {r.a.until!r}" if r.a.until else ""))
    if r.a.dry_run:
        return True
    return base.capture(r, "trial", r.a.seconds, r.a.until or "\0never\0")


LOG_RING = (FONT_DATA[0] + FONT_DATA[1] - 0x80000, 0x80000)


def save_device(r):
    """Read the device's own /user (model), core dump and log ring before a restore overwrites them.

    2026-11-12 a restore wiped the user's data: the *_backup.bin files are copies of the factory backup,
    not reads of the device. Read-only; nothing is written if any read fails."""
    saved = os.path.join(r.a.out, "device-before-restore")
    os.makedirs(saved, exist_ok=True)
    for name, (start, size) in (("model", MODEL), ("coredump", COREDUMP), ("logs", LOG_RING)):
        cmd = r.a.esptool + ["--chip", "esp32s31", "--port", r.a.port_now, "--baud", str(r.a.baud),
                             "--before", "default-reset", "--after", "no-reset", "read-flash",
                             "0x%x" % start, "0x%x" % size, os.path.join(saved, f"{name}.bin")]
        r.say("run: " + " ".join(shlex.quote(t) for t in cmd))
        if not r.a.dry_run and subprocess.call(cmd) != 0:
            raise RuntimeError(f"could not save the device's {name} region; nothing written")
    r.say(f"device regions saved to {saved}")


def restore(r, p):
    r.step("5. save the device's own data, then write otadata, ota_1, model, font_data and coredump back from the backup")
    pairs = [(REGIONS[n][0], p[n]) for n in ("otadata", "ota_1", "model", "font_data", "coredump")]
    saved = False
    for _attempt in range(1 if r.a.dry_run else base.RESTORE_TRIES):
        try:
            port = base.wait_port_idle(r, "restore")
        except Exception as e:  # noqa: BLE001  a port that drops while waiting: try again
            r.say(f"port error: {e}")
            port = None
        if port is not None:
            r.a.port_now = port
            if not saved:
                save_device(r)
                saved = True
            if r.esptool("no-reset", "write-flash", pairs) == 0 and r.esptool("no-reset", "verify-flash", pairs) == 0:
                r.say("would verify (dry run)" if r.a.dry_run else "RESTORE VERIFIED")
                return True
        r.say("cannot restore yet; trying again in 10 s")
        time.sleep(10)
    return False


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", required=True)
    ap.add_argument("--backup", help="verified full 16 MiB flash backup of this device (not with --card-files-only)")
    ap.add_argument("--image", help="the app image for ota_1 (not with --restore-only)")
    ap.add_argument("--fonts", help="the font pack for font_data (0xae4000), built with --image (needed with it)")
    ap.add_argument("--out", required=True, help="output directory (outside the repository)")
    ap.add_argument("--seconds", type=float, default=120.0)
    ap.add_argument("--until", help="stop logging early once a console line holds this text")
    ap.add_argument("--baud", type=int, default=460800)
    ap.add_argument("--python", default=sys.executable, help="the Python that has esptool")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--install", action="store_true", help="write, verify, log the boot and leave the trial running")
    ap.add_argument("--restore-only", action="store_true", help="only write the regions back and boot the factory firmware")
    ap.add_argument("--card-files", help="the install bundle (default: card-files next to --image)")
    ap.add_argument("--no-card-files", action="store_true", help="--install without the bundle: nothing goes onto the card")
    ap.add_argument("--card-files-only", action="store_true", help="only serve the card files to the running build")
    ap.add_argument("--reset", action="store_true", help="with --card-files-only: reset first, so the device checks again")
    a = ap.parse_args(argv)
    if not a.card_files_only and not a.backup:
        ap.error("--backup is needed unless --card-files-only")
    if not a.restore_only and not a.card_files_only and not a.image:
        ap.error("--image is needed unless --restore-only or --card-files-only")
    if a.image and not a.fonts:
        beside = os.path.join(os.path.dirname(os.path.abspath(a.image)), "ink-fonts.bin")
        ap.error("--fonts is needed with --image: the app reads only the font pack built with it (an image before "
                 "font-pack-1 cannot read today's compressed fonts and would draw no Chinese)"
                 + (f"; the pack beside the image is {beside}" if os.path.isfile(beside) else ""))
    a.card_files = a.card_files or (os.path.join(os.path.dirname(os.path.abspath(a.image)), "card-files") if a.image else None)
    a.esptool = [a.python, "-m", "esptool"]
    a.port_now = a.port
    os.makedirs(a.out, exist_ok=True)
    r = base.Run(a)
    r.touched = False
    try:
        try:
            p = check_bundle(a) if a.card_files_only else prepare(r)
        except base.Stop as e:
            r.say(f"STOP before writing anything: {e}")
            return 2
        if a.card_files_only:  # nothing is written to flash, esptool is not run
            return 0 if card_files_ok(serve_card_files(r, a.reset)) else 5
        if a.restore_only:
            r.touched = True
        elif a.install:
            a.until = a.until or "starting system services"
            started = None
            try:
                started = install_and_run(r, p)
            except Exception as e:  # noqa: BLE001  a failed write or verify: put the device back
                r.say(f"ERROR: {e}; restoring")
            if started is not None:
                r.say("INSTALLED: the trial build is running; the way back is --restore-only" + ("" if started else
                      " (the console did not show the system start: look at trial_console.txt)"))
                return 4 if not started else 0 if a.no_card_files or card_files_ok(serve_card_files(r)) else 5
        else:
            try:
                install_and_run(r, p)
            except Exception as e:  # noqa: BLE001  whatever went wrong, restore
                r.say(f"ERROR: {e}; restoring")
            except KeyboardInterrupt:
                r.say("interrupted; restoring")
        if not r.touched:
            r.say("nothing was written")
            return 2
        if not restore(r, p):
            r.say("RESTORE NOT VERIFIED: stop and look")
            return 1
        booted = base.boot_check(r)
        if a.restore_only:  # the user takes the device back: leave the factory firmware running
            r.say(f"done: restore verified, factory boot {'seen' if booted else 'NOT seen'}")
            return 0 if booted else 3
        parked = base.park(r)
        r.say(f"done: restore verified, factory boot {'seen' if booted else 'NOT seen'}, "
              f"{'parked' if parked else 'NOT parked'}")
        return 0 if booted and parked else 3
    finally:
        r.log.close()


if __name__ == "__main__":
    sys.exit(main())
