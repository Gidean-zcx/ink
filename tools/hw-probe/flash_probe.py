#!/usr/bin/env python3
"""Put the hw-probe firmware into ota_1 and back again; refuses to write anywhere else.

  install  wait until the panel is idle, reset into download mode, write the probe image to
           ota_1 and an otadata that selects ota_1, verify both against the device, reset into
           the probe and log its console for --log-seconds.
  restore  write the factory otadata back (ota_0 boots again) and return the part of ota_1 the
           probe used to its backed-up content (erased), verify, reset into the factory firmware.

Only otadata (0xd000, 8 KiB) and ota_1 (0x580000, 5 MiB) can be written: any other address
stops the run before the device is touched. Writing needs --approved, given only after the user
agreed to this plan (docs/hardware/phase1-plan.md). ota_0, the bootloader, the partition table,
NVS and eFuses are never written.

  flash_probe.py install --port '/dev/cu.usbmodem*' --dir ink-hw/phase1 --approved
  flash_probe.py restore --port '/dev/cu.usbmodem*' --dir ink-hw/phase1 --backup private/flash_full_1.bin --approved
"""

import argparse
import datetime
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bringup"))

import esptool  # noqa: E402
from esptool.cmds import detect_chip, reset_chip, run_stub  # noqa: E402
from readout import wait_idle  # noqa: E402

OTADATA = (0xD000, 0x2000)
OTA_1 = (0x580000, 0x500000)
SECTOR = 0x1000


def allowed(addr, size):
    return any(start <= addr and addr + size <= start + length for start, length in (OTADATA, OTA_1))


def step(title):
    print(f"\n######## {title} ({datetime.datetime.now():%H:%M:%S})", flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("mode", choices=("install", "restore"))
    ap.add_argument("--port", required=True)
    ap.add_argument("--dir", required=True, help="holds ink-hw-probe.bin, otadata_ota1.bin, otadata_factory.bin")
    ap.add_argument("--backup", help="restore: full flash backup (source of ota_1's original bytes)")
    ap.add_argument("--log-seconds", type=float, default=60.0)
    ap.add_argument("--approved", action="store_true", help="the user approved this write")
    a = ap.parse_args()
    if not a.approved:
        sys.exit("refusing to write: pass --approved only after the user agreed to the plan")

    probe = os.path.join(a.dir, "ink-hw-probe.bin")
    used = (os.path.getsize(probe) + SECTOR - 1) // SECTOR * SECTOR
    if a.mode == "install":
        writes = [(OTA_1[0], probe), (OTADATA[0], os.path.join(a.dir, "otadata_ota1.bin"))]
    else:
        if not a.backup:
            sys.exit("restore needs --backup")
        original = os.path.join(a.dir, "ota_1_original.bin")
        with open(a.backup, "rb") as f:
            f.seek(OTA_1[0])
            open(original, "wb").write(f.read(used))
        writes = [(OTADATA[0], os.path.join(a.dir, "otadata_factory.bin")), (OTA_1[0], original)]
    for addr, path in writes:
        if not allowed(addr, os.path.getsize(path)):
            sys.exit(f"refusing to write {path} at 0x{addr:x}: outside otadata and ota_1")
        print(f"plan: 0x{addr:06x} <- {path} ({os.path.getsize(path)} bytes)")

    step("wait for the port and for the panel to be idle")
    port = wait_idle(a.port, os.path.join(a.dir, f"{a.mode}_console.txt"), 3.0, 300.0)
    if port is None:
        sys.exit("port never appeared or panel never went idle; nothing was written")
    step(f"download mode on {port}")
    esp = detect_chip(port, connect_mode="default-reset")
    try:
        stub = run_stub(esp)
        args = []
        for addr, path in writes:
            args += [hex(addr), path]
        step("write")
        esptool.main(["--no-stub", "--after", "no-reset-stub", "write-flash"] + args, esp=stub)
        step("verify against the device, then reset")
        esptool.main(["--no-stub", "--after", "hard-reset", "verify-flash"] + args, esp=stub)
    except BaseException:
        print("\nERROR: resetting the chip", flush=True)
        reset_chip(esp, "hard-reset")
        esp._port.close()
        raise
    esp._port.close()

    if a.mode == "install" and a.log_seconds > 0:
        step(f"probe console for {a.log_seconds:.0f} s")
        from serial_log import find_port, open_port  # noqa: E402
        out = os.path.join(a.dir, "probe_console.txt")
        t0 = time.monotonic()
        with open(out, "a", buffering=1, encoding="utf-8") as f:
            s, buf = None, b""
            while time.monotonic() - t0 < a.log_seconds:
                if s is None:
                    p = find_port(a.port)
                    try:
                        s = open_port(p, 115200) if p else None
                    except OSError:
                        s = None
                    if s is None:
                        time.sleep(0.1)
                        continue
                try:
                    buf += s.read(4096)
                except OSError:
                    s = None
                    continue
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    text = line.rstrip(b"\r").decode("utf-8", "replace")
                    f.write(f"[{datetime.datetime.now():%H:%M:%S.%f}] {text}\n")
        print(f"console log: {out}")
    print(f"\n{a.mode.upper()} OK")


if __name__ == "__main__":
    main()
