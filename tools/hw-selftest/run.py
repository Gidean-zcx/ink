#!/usr/bin/env python3
"""Run the hardware self-test from ota_1 and put the device back as it was.

Only two places are ever written: otadata (0xd000, 8 KiB) and ota_1 (0x580000, 5 MiB). Never a whole
erase, never any other address. Writing needs a checked full backup and the user's consent (CLAUDE.md).

  run.py --port '/dev/cu.usbmodem*' --backup flash_full.bin --image build/ink-hw-selftest.bin --out DIR [--dry-run]

 1. The backup is 16 MiB; its partition table (MD5 checked) has otadata, ota_0 and ota_1 where expected.
 2. otadata and ota_1 are copied out of the backup into DIR.
 3. An otadata that selects ota_1 is made from the factory one (its highest sequence number plus one, in
    the other sector; ESP-IDF format). After the panel is idle: write the image to ota_1 and that
    otadata, then verify both against the device.
 4. Reset into the self-test and log its console until HWTEST_DONE (after a full replay) or --seconds;
    the port may drop and come back (light sleep, then the self-test restarts itself once): it is
    reopened and logging goes on until the last boot's replay ends with HWTEST_DONE.
 5. Restore: the factory otadata and the part of ota_1 the image covered, from the backup; verify
    otadata and the whole of ota_1 against the backup (if ota_1 differs, all of it is written back).
 6. Reset and log the boot until the factory firmware is loaded from ota_0 again.

Any failure in 3 or 4 goes straight to 5. --dry-run does 1 and 2 and prints the device commands only.
Exit codes: 0 all done; 1 restore NOT verified (stop and look); 2 stopped before writing anything;
3 restored and verified, but the test did not finish or the factory boot line was not seen.
"""

import argparse
import binascii
import datetime
import glob
import hashlib
import os
import re
import shlex
import struct
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
BRINGUP = os.path.join(HERE, "..", "bringup")

FLASH_SIZE = 16 * 1024 * 1024
PT_OFFSET, PT_SIZE = 0x8000, 0xC00
OTADATA = (0xD000, 0x2000)
OTA_0 = (0x80000, 0x500000)
OTA_1 = (0x580000, 0x500000)
WRITABLE = (OTADATA, OTA_1)
SECTOR = 0x1000
OTA_VALID = 2  # ESP_OTA_IMG_VALID
OTA_DEAD = (3, 4)  # INVALID, ABORTED: the bootloader skips such an entry
START, DONE, REPLAY, SAFETY_FAIL = "HWTEST_START", "HWTEST_DONE", "HWTEST_REPLAY", "SAFETY_FAIL"
BOOT_LINE = "Loaded app from partition at offset 0x%x" % OTA_0[0]
RESTORE_TRIES = 30


class Stop(Exception):
    """A check failed before anything was written."""


# ------------------------------------------------------------------------- partition table

def parse_partitions(table):
    """ESP-IDF binary partition table: 32-byte entries (magic 0x50AA), an optional MD5 entry (0xEBEB)."""
    parts, seen = [], b""
    for at in range(0, min(len(table), PT_SIZE), 32):
        e = table[at:at + 32]
        if e[:2] == b"\xff\xff":
            break
        if e[:2] == b"\xeb\xeb":
            if e[16:32] != hashlib.md5(seen).digest():
                raise Stop("partition table MD5 does not match")
            continue
        if e[:2] != b"\xaa\x50":
            raise Stop("not a partition table")
        ptype, sub, off, size = struct.unpack_from("<BBII", e, 2)
        parts.append({"label": e[12:28].rstrip(b"\0").decode("ascii", "replace"), "type": ptype, "subtype": sub,
                      "offset": off, "size": size})
        seen += e
    return parts


def check_layout(parts):
    """otadata, ota_0 and ota_1 exactly where this script writes and expects, and no other app slot."""
    def one(ptype, sub, name):
        hits = [p for p in parts if p["type"] == ptype and p["subtype"] == sub]
        if len(hits) != 1:
            raise Stop(f"{name}: {len(hits)} entries in the partition table")
        return (hits[0]["offset"], hits[0]["size"])
    if one(1, 0x00, "otadata") != OTADATA:
        raise Stop("otadata is not where expected")
    if one(0, 0x10, "ota_0") != OTA_0:
        raise Stop("ota_0 is not where expected")
    if one(0, 0x11, "ota_1") != OTA_1:
        raise Stop("ota_1 is not where expected")
    apps = [p for p in parts if p["type"] == 0]
    if len(apps) != 2:
        raise Stop(f"{len(apps)} app partitions, expected ota_0 and ota_1 only")
    for p in parts:
        for start, size in WRITABLE:
            overlap = p["offset"] < start + size and start < p["offset"] + p["size"]
            if overlap and (p["offset"], p["size"]) != (start, size):
                raise Stop(f"partition {p['label']} overlaps a region this script writes")


# ------------------------------------------------------------------------- otadata

def seq_crc(seq):
    """CRC of an otadata entry: esp_rom_crc32_le(UINT32_MAX, &ota_seq, 4) (bootloader_common_loader.c)."""
    return binascii.crc32(struct.pack("<I", seq), 0xFFFFFFFF) & 0xFFFFFFFF


def ota_entries(otadata):
    """(seq, state, valid) of the two sectors' entries, as the bootloader judges them."""
    out = []
    for sector in (0, 1):
        seq, _label, state, crc = struct.unpack_from("<I20sII", otadata, sector * SECTOR)
        valid = seq != 0xFFFFFFFF and state not in OTA_DEAD and crc == seq_crc(seq)
        out.append((seq, state, valid))
    return out


def boot_slot(otadata, slots=2):
    """The OTA slot the bootloader picks: (highest valid seq - 1) % slots; None without a valid entry."""
    valid = [seq for seq, _state, ok in ota_entries(otadata) if ok]
    return (max(valid) - 1) % slots if valid else None


def select_ota1(factory):
    """otadata that boots ota_1: the factory entry kept, seq + 1 (VALID) written into the other sector."""
    if len(factory) != OTADATA[1]:
        raise Stop(f"otadata is {len(factory)} bytes, expected {OTADATA[1]}")
    entries = ota_entries(factory)
    valid = [(seq, sector) for sector, (seq, _state, ok) in enumerate(entries) if ok]
    if not valid:
        raise Stop("the factory otadata has no valid entry")
    top, sector = max(valid)
    if (top - 1) % 2 != 0:
        raise Stop("the factory otadata does not select ota_0")
    seq = top + 1
    other = 1 - sector
    out = bytearray(factory)
    out[other * SECTOR:(other + 1) * SECTOR] = b"\xff" * SECTOR
    struct.pack_into("<I20sII", out, other * SECTOR, seq, b"\xff" * 20, OTA_VALID, seq_crc(seq))
    if boot_slot(bytes(out)) != 1:
        raise Stop("made an otadata that does not select ota_1")
    return bytes(out)


# ------------------------------------------------------------------------- device commands

def check_write(addr, size):
    """Inside otadata or ota_1, sector aligned; anything else stops the run."""
    if addr % SECTOR or size <= 0:
        raise Stop(f"refusing to touch 0x{addr:x}: not sector aligned")
    if not any(start <= addr and addr + size <= start + length for start, length in WRITABLE):
        raise Stop(f"refusing to touch 0x{addr:x} (+{size} bytes): outside otadata and ota_1")


def esptool_cmd(a, after, op, pairs):
    """esptool write-flash or verify-flash of (address, file) pairs, each checked."""
    if op not in ("write-flash", "verify-flash"):
        raise Stop(f"refusing esptool {op}")
    cmd = a.esptool + ["--chip", "esp32s31", "--port", a.port_now, "--baud", str(a.baud), "--before", "default-reset",
                       "--after", after, op]
    if op == "write-flash":
        cmd += ["--flash-mode", "keep", "--flash-freq", "keep", "--flash-size", "keep"]
    for addr, path in pairs:
        check_write(addr, os.path.getsize(path))
        cmd += ["0x%x" % addr, path]
    if any("erase" in t for t in cmd):
        raise Stop("refusing an erase")
    return cmd


class Run:
    def __init__(self, a):
        self.a = a
        self.log = open(os.path.join(a.out, "run_log.txt"), "a", buffering=1, encoding="utf-8")

    def say(self, text):
        print(text, flush=True)
        self.log.write(f"[{datetime.datetime.now():%H:%M:%S}] {text}\n")

    def step(self, title):
        self.say(f"\n######## {title}")

    def esptool(self, after, op, pairs):
        cmd = esptool_cmd(self.a, after, op, pairs)
        self.say("run: " + " ".join(shlex.quote(t) for t in cmd))
        if self.a.dry_run:
            return 0
        rc = subprocess.call(cmd)
        self.say(f"exit {rc}")
        return rc


# ------------------------------------------------------------------------- the serial side (real runs only)

def serial_helpers():
    sys.path.insert(0, BRINGUP)
    import serial_log  # noqa: E402  (pyserial)
    from readout import wait_idle  # noqa: E402  (esptool)
    return serial_log, wait_idle


def wait_port_idle(r, name):
    """The port's path once the factory firmware is not refreshing the panel (a reset mid-refresh leaves
    the panel's high voltage on); None if it never came."""
    if r.a.dry_run:
        r.say(f"would wait for the port and for the panel to be idle ({name})")
        return r.a.port
    _, wait_idle = serial_helpers()
    path = wait_idle(r.a.port, os.path.join(r.a.out, f"{name}_wait_console.txt"), 3.0, 300.0)
    r.say(f"port {path or 'never came'}")
    return path


def capture(r, name, seconds, until, armed_by=None, reset=False, disarm_by=None, quiet=30.0):
    """Log the console into DIR/<name>_console.txt until a line holds `until` (after one holding
    `armed_by`, if given) or `seconds` pass. True if `until` was seen.

    The port may vanish and come back (a reset, the self-test's light sleep and the esp_restart after
    it): it is reopened and the log goes on; that is not a failure and triggers nothing. A port that
    stays open but says nothing for `quiet` seconds is reopened too (after the device re-enumerates
    the old handle can go silent instead of failing). A line cut by a drop is dropped; `until` counts
    only after a fresh `armed_by` line, and a `disarm_by` line (a new boot) starts over, so only the
    last boot's whole replay ends the log."""
    serial_log, _ = serial_helpers()
    out = os.path.join(r.a.out, f"{name}_console.txt")
    t0 = time.monotonic()
    armed, seen, did_reset, lost, drops, reopens = armed_by is None, False, not reset, False, 0, 0
    with open(out, "a", buffering=1, encoding="utf-8") as f:
        s, buf, heard = None, b"", time.monotonic()

        def drop(why):
            nonlocal s, buf, lost, armed
            f.write(f"# {why} {datetime.datetime.now():%H:%M:%S.%f}\n")
            try:
                s.close()
            except OSError:
                pass
            s, buf, lost = None, b"", True
            armed = armed_by is None

        while time.monotonic() - t0 < seconds and not seen:
            if s is None:
                path = serial_log.find_port(r.a.port)
                try:
                    s = serial_log.open_port(path, 115200) if path else None
                except OSError:
                    s = None
                if s is None:
                    time.sleep(0.1)
                    continue
                heard = time.monotonic()
                if lost:
                    f.write(f"# port back {datetime.datetime.now():%H:%M:%S.%f}\n")
                    lost = False
                if not did_reset:
                    serial_log.reset(s)  # RTS pulse: a normal boot
                    did_reset = True
                    f.write("# reset\n")
            try:
                chunk = s.read(4096)
            except OSError as e:  # serial.SerialException is an OSError
                drop(f"port lost: {e}")
                drops += 1
                continue
            if chunk:
                buf += chunk
                heard = time.monotonic()
            elif time.monotonic() - heard > quiet:
                drop(f"port silent for {quiet:.0f} s: reopening")
                reopens += 1
                continue
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.rstrip(b"\r").decode("utf-8", "replace")
                f.write(f"[{datetime.datetime.now():%H:%M:%S.%f}] {text}\n")
                if disarm_by and disarm_by in text:
                    armed = armed_by is None
                if armed_by and armed_by in text:
                    armed = True
                if until in text and armed:
                    seen = True
                    break
        if s is not None:
            try:
                s.close()
            except OSError:
                pass
    r.say(f"console log {out}: {'saw ' + until if seen else 'NOT seen: ' + until}; "
          f"the port dropped {drops} times and was reopened after silence {reopens} times")
    return seen


PRIVATE = (re.compile(r"(?:[0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}"), re.compile(r"\d{14,}"))


def collect_results(r):
    """The last full replay's JSON lines into DIR/results.jsonl; flags anything that looks private."""
    path = os.path.join(r.a.out, "selftest_console.txt")
    if not os.path.exists(path):
        return False, False
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = [re.sub(r"^\[[^\]]*\] ", "", ln.rstrip("\n")) for ln in f]
    last = max((i for i, ln in enumerate(lines) if ln == REPLAY), default=-1)
    block = lines[last + 1:] if last >= 0 else lines
    results = [ln for ln in block if ln.startswith("{") and ln.endswith("}")]
    with open(os.path.join(r.a.out, "results.jsonl"), "w", encoding="utf-8") as f:
        f.write("".join(ln + "\n" for ln in results))
    for i, ln in enumerate(results):
        if any(p.search(ln) for p in PRIVATE):
            r.say(f"PRIVACY_CHECK: result line {i + 1} looks like it holds an identifier; do not publish it")
    summary = next((ln for ln in reversed(results) if '"test":"summary"' in ln), "")
    r.say("summary: " + (summary or "none"))
    return '"ok":true' in summary, any(ln == SAFETY_FAIL for ln in block)


# ------------------------------------------------------------------------- the steps

def prepare(r):
    """Steps 1 and 2 (files only). Returns the paths the device steps use."""
    a = r.a
    r.step("1. check the backup and its partition table")
    if os.path.getsize(a.backup) != FLASH_SIZE:
        raise Stop(f"backup is {os.path.getsize(a.backup)} bytes, expected {FLASH_SIZE}")
    with open(a.backup, "rb") as f:
        f.seek(PT_OFFSET)
        table = f.read(PT_SIZE)
        f.seek(OTADATA[0])
        factory = f.read(OTADATA[1])
        f.seek(OTA_1[0])
        ota1 = f.read(OTA_1[1])
    check_layout(parse_partitions(table))
    r.say("partition table: otadata, ota_0 and ota_1 as expected")
    with open(a.image, "rb") as f:
        image = f.read()
    if not image or image[0] != 0xE9:
        raise Stop("the self-test image is not an ESP app image")
    if len(image) > OTA_1[1]:
        raise Stop(f"the self-test image ({len(image)} bytes) does not fit ota_1")
    if boot_slot(factory) != 0:
        raise Stop("the backup's otadata does not boot ota_0")
    r.say(f"image {len(image)} bytes, sha256 {hashlib.sha256(image).hexdigest()[:16]}...")

    r.step("2. copy otadata and ota_1 out of the backup")
    p = {k: os.path.join(a.out, v) for k, v in (("factory", "otadata_factory.bin"), ("ota1", "otadata_ota1.bin"),
                                               ("orig", "ota_1_original.bin"), ("part", "ota_1_restore.bin"),
                                               ("tail", "ota_1_tail.bin"))}
    span = (len(image) + SECTOR - 1) // SECTOR * SECTOR
    # The light-sleep build keeps its progress mark in ota_1's last sector (selftest.c, mark_*): put it back too.
    for key, data in (("factory", factory), ("orig", ota1), ("part", ota1[:span]), ("ota1", select_ota1(factory)),
                      ("tail", ota1[-SECTOR:])):
        with open(p[key], "wb") as f:
            f.write(data)
    seq = max(s for s, _st, ok in ota_entries(select_ota1(factory)) if ok)
    r.say(f"otadata for ota_1: seq {seq}; restore covers {span} bytes of ota_1, verify covers all of it")
    p["image"] = a.image
    return p


def install_and_test(r, p):
    r.step("3. write the self-test to ota_1 and select it")
    port = wait_port_idle(r, "install")
    if port is None:
        raise RuntimeError("no port")
    r.a.port_now = port
    r.touched = True
    pairs = [(OTA_1[0], p["image"]), (OTADATA[0], p["ota1"])]
    if r.esptool("no-reset", "write-flash", pairs):
        raise RuntimeError("write failed")
    if r.esptool("hard-reset", "verify-flash", pairs):
        raise RuntimeError("verify after writing failed")
    r.step(f"4. self-test console until {DONE} (at most {r.a.seconds:.0f} s)")
    if r.a.dry_run:
        r.say(f"would log the console until {DONE} after {REPLAY}")
        return True
    return capture(r, "selftest", r.a.seconds, DONE, armed_by=REPLAY, disarm_by=START)


def restore(r, p):
    r.step("5. restore otadata and ota_1 from the backup")
    for _attempt in range(1 if r.a.dry_run else RESTORE_TRIES):
        try:
            port = wait_port_idle(r, "restore")
        except Exception as e:  # noqa: BLE001  a port that drops while waiting: try again
            r.say(f"port error: {e}")
            port = None
        if port is not None:
            r.a.port_now = port
            rc = r.esptool("no-reset", "write-flash", [(OTADATA[0], p["factory"]), (OTA_1[0], p["part"]),
                                                       (OTA_1[0] + OTA_1[1] - SECTOR, p["tail"])])
            if rc == 0:
                rc = r.esptool("no-reset", "verify-flash", [(OTADATA[0], p["factory"]), (OTA_1[0], p["orig"])])
                if rc != 0:  # something else in ota_1 changed: put all of it back
                    r.say("ota_1 differs from the backup beyond the image: writing all of ota_1 back")
                    rc = r.esptool("no-reset", "write-flash", [(OTA_1[0], p["orig"])])
                    if rc == 0:
                        rc = r.esptool("no-reset", "verify-flash", [(OTADATA[0], p["factory"]), (OTA_1[0], p["orig"])])
            if rc == 0:
                r.say("would verify the restore (dry run)" if r.a.dry_run else "RESTORE VERIFIED")
                return True
        r.say("USB_STUCK: cannot restore yet; if this repeats, replug the USB cable. Trying again.")
        time.sleep(10)
    return False


def boot_check(r):
    r.step("6. reset into the factory firmware")
    if r.a.dry_run:
        r.say("would reset and log the boot until the factory firmware loads from ota_0")
        return True
    return capture(r, "boot", 60.0, BOOT_LINE, reset=True)


def park(r):
    """Keep the link: once the factory firmware has booted and the panel is idle, reset into download mode and stay
    there. The factory firmware sleeps after 180 s idle and USB goes; download mode does not sleep. Read only."""
    r.step("7. park in download mode")
    if r.a.dry_run:
        r.say("would wait for an idle panel and park the chip in download mode")
        return True
    time.sleep(12)  # past the bootloader's watchdog window and the start of the boot refresh
    path = wait_port_idle(r, "park")
    if not path:
        return False
    cmd = r.a.esptool + ["--chip", "esp32s31", "--port", path, "--before", "default-reset", "--after", "no-reset", "chip-id"]
    rc = subprocess.call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    r.say("parked in download mode" if rc == 0 else f"could not park (exit {rc})")
    return rc == 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", required=True, help="serial port path or glob, e.g. '/dev/cu.usbmodem*'")
    ap.add_argument("--backup", required=True, help="verified full 16 MiB flash backup of this device")
    ap.add_argument("--image", required=True, help="tools/hw-selftest/build/ink-hw-selftest.bin")
    ap.add_argument("--out", required=True, help="output directory (outside the repository)")
    ap.add_argument("--seconds", type=float, default=300.0, help="how long to wait for HWTEST_DONE")
    ap.add_argument("--baud", type=int, default=460800)
    ap.add_argument("--python", default=sys.executable, help="the Python that has esptool")
    ap.add_argument("--dry-run", action="store_true", help="files only; print the device commands")
    a = ap.parse_args(argv)
    a.esptool = [a.python, "-m", "esptool"]
    a.port_now = a.port
    os.makedirs(a.out, exist_ok=True)
    r = Run(a)
    try:
        return steps(r)
    finally:
        r.log.close()


def steps(r):
    a = r.a
    r.touched = False
    try:
        p = prepare(r)
    except Stop as e:
        r.say(f"STOP before writing anything: {e}")
        return 2
    if a.dry_run:
        r.say("DRY RUN: nothing below touches the device")
    tested = False
    try:
        tested = install_and_test(r, p)
    except Stop as e:
        r.say(f"STOP: {e}")
        if not r.touched:
            return 2
    except Exception as e:  # noqa: BLE001  whatever went wrong, restore
        r.say(f"ERROR: {e}; restoring")
    except KeyboardInterrupt:
        r.say("interrupted; restoring")
    if not r.touched:
        r.say("nothing was written")
        return 2
    restored = restore(r, p)
    if not restored:
        r.say("RESTORE NOT VERIFIED: stop and look; the backup has otadata and ota_1")
        return 1
    booted = boot_check(r)
    parked = booted and park(r)
    if a.dry_run:
        r.say("DRY RUN done: the device was not touched")
        return 0
    passed, unsafe = collect_results(r)
    if unsafe:
        r.say(f"the self-test said {SAFETY_FAIL}: it ran nothing after the safety step")
    r.say(f"done: test {'finished' if tested else 'NOT finished'}, results {'all ok' if passed else 'see results.jsonl'}, "
          f"restore verified, factory boot {'seen' if booted else 'NOT seen'}, {'parked in download mode' if parked else 'NOT parked'}")
    return 0 if tested and booted else 3


if __name__ == "__main__":
    sys.exit(main())
