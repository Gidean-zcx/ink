#!/usr/bin/env python3
"""Phase-0 read-out: identify the chip and take a verified full flash backup.

Everything runs over one connection, so the firmware never runs between the
two reads (the factory firmware writes NVS on every boot) and the port is
never reopened. The steps are fixed here and are all reads:

  1. Listen to the console until the e-paper panel is idle (see PanelIdle in
     serial_log.py: a reset mid-refresh would keep the panel's high voltage
     on during download mode).
  2. Reset into download mode, read the security info, and stop (hard reset
     back to the firmware) if flash encryption, secure boot or secure
     download mode is on.
  3. flash-id, read-mac, eFuse summary (text and JSON).
  4. Read the whole flash twice; the SHA-256 of both must match.
  5. verify-flash against the device, then hard reset into the firmware.

Any failure resets the chip back into its firmware. --resume picks up a chip
that an interrupted run left in the flasher stub: it connects without a
reset and starts at step 3.

  readout.py --port '/dev/cu.usbmodem*' --out-dir private
"""

import argparse
import datetime
import hashlib
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import esptool  # noqa: E402
import serial  # noqa: E402
from espefuse.efuse_interface import init_commands  # noqa: E402
from esptool.cmds import (  # noqa: E402
    attach_flash, connect_esp, detect_chip, reset_chip, run_stub)
from serial_log import PanelIdle, find_port, open_port  # noqa: E402


class Stop(Exception):
    """A check failed; the chip goes back to its firmware and nothing more is read."""


def wait_idle(pattern, out, quiet, timeout):
    """Return the port path once the panel is idle. Waits for the port to
    appear (the factory firmware drops USB in standby) and starts over
    whenever it drops off."""
    t0 = time.monotonic()
    s = None
    panel = PanelIdle(quiet)
    with open(out, "a", buffering=1, encoding="utf-8") as f:
        f.write(f"# wait_idle start {datetime.datetime.now()}\n")
        while True:
            now = time.monotonic()
            if now - t0 > timeout:
                f.write("# wait_idle timeout\n")
                if s is not None:
                    s.close()
                return None
            if s is None:
                path = find_port(pattern)
                try:
                    if path is None:
                        raise OSError("no such port")
                    s = open_port(path, 115200)
                except (serial.SerialException, OSError):
                    s = None
                    time.sleep(0.2)
                    continue
                f.write(f"# port open {path} {datetime.datetime.now()}\n")
                panel.restart()
                buf = b""
                continue
            try:
                buf += s.read(4096)
            except (serial.SerialException, OSError) as e:
                f.write(f"# port lost: {e}\n")
                try:
                    s.close()
                except (serial.SerialException, OSError):
                    pass
                s = None
                continue
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.rstrip(b"\r").decode("utf-8", "replace")
                f.write(f"[{datetime.datetime.now():%H:%M:%S.%f}] {line}\n")
                panel.feed(line)
            if panel.idle():
                f.write(f"# idle {datetime.datetime.now()}\n")
                s.close()
                return path


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def step(title):
    print(f"\n######## {title} ({datetime.datetime.now():%H:%M:%S})", flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--quiet", type=float, default=3.0)
    ap.add_argument("--idle-timeout", type=float, default=300.0)
    ap.add_argument("--resume", action="store_true",
                    help="the chip is already in the flasher stub")
    a = ap.parse_args()
    out = lambda name: os.path.join(a.out_dir, name)  # noqa: E731

    if a.resume:
        port = find_port(a.port)
        step(f"resume: connect to the running stub on {port}")
        # name the chip: auto-detection asks for security info, which the stub may not answer
        esp = connect_esp(port, chip="esp32s31", before="no-reset")
        if not esp.sync_stub_detected:
            esp._port.close()
            sys.exit("--resume: the chip is not in the flasher stub; nothing was done")
    else:
        step("wait for the port and for the panel to be idle")
        port = wait_idle(a.port, out("readout_console.txt"), a.quiet, a.idle_timeout)
        if port is None:
            sys.exit("port never appeared or panel never went idle; nothing was done")
        step(f"download mode + security info on {port}")
        esp = detect_chip(port, connect_mode="default-reset")
    try:
        readout(esp, out, a.resume)
    except BaseException as e:
        # never leave the chip parked in download mode (click exits count too)
        print(f"\n{'STOP' if isinstance(e, Stop) else 'ERROR'}: resetting the chip "
              "back to its firmware", flush=True)
        reset_chip(esp, "hard-reset")
        esp._port.close()
        if isinstance(e, Stop):
            sys.exit(f"STOP: {e}")
        raise


def readout(esp, out, resume):
    if not resume:
        si = esp.get_security_info()
        with open(out("security_info.json"), "w") as f:
            json.dump(si, f, indent=2, default=str)
        esptool.main(["--no-stub", "--after", "no-reset", "get-security-info"], esp=esp)
        problems = []
        if bin(si["flash_crypt_cnt"]).count("1") % 2:
            problems.append("flash encryption is on")
        if si["parsed_flags"]["SECURE_BOOT_EN"]:
            problems.append("secure boot is on")
        if esp.secure_download_mode:
            problems.append("secure download mode is on")
        if problems:
            raise Stop(", ".join(problems))

    stub = run_stub(esp)
    keep = ["--no-stub", "--after", "no-reset-stub"]

    step("flash-id, read-mac")
    esptool.main(keep + ["flash-id"], esp=stub)
    esptool.main(keep + ["read-mac"], esp=stub)

    step("eFuse summary")
    # espefuse.main() drops an external connection (5.6), so use its API
    efuses = init_commands(esp=stub)
    with open(out("efuse_summary.txt"), "w") as f:
        efuses.summary(file=f)
    with open(out("efuse_summary.json"), "w") as f:
        efuses.summary(format="json", file=f)

    # read-flash cannot size "ALL" on a reused connection: use the JEDEC capacity byte
    attach_flash(stub)
    fid = stub.flash_id()
    size = 1 << ((fid >> 16) & 0xFF)
    if not 1 << 20 <= size <= 1 << 27:
        raise Stop(f"implausible flash size from JEDEC id 0x{fid:06x}")
    print(f"JEDEC id 0x{fid:06x}: {size} bytes")

    digests = []
    for n in (1, 2):
        step(f"read-flash 0 {size:#x} -> flash_full_{n}.bin")
        t = time.monotonic()
        esptool.main(keep + ["read-flash", "0", hex(size), out(f"flash_full_{n}.bin")],
                     esp=stub)
        digests.append(sha256(out(f"flash_full_{n}.bin")))
        print(f"took {time.monotonic() - t:.1f} s, sha256 {digests[-1]}")
    if digests[0] != digests[1]:
        raise Stop("the two reads differ")

    step("verify-flash 0 flash_full_1.bin, then hard reset")
    esptool.main(["--no-stub", "--after", "hard-reset", "verify-flash", "0",
                  out("flash_full_1.bin")], esp=stub)
    stub._port.close()
    print("\nREADOUT OK: both reads match and the device verifies against them")


if __name__ == "__main__":
    main()
