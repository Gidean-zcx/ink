#!/usr/bin/env python3
"""Timestamped serial logger that survives the USB port going away.

Opens the port without resetting the chip: the USB Serial/JTAG controller
treats RTS asserted with DTR released as reset, and pyserial would release
DTR before RTS on open. With --reset it pulses RTS once for a normal boot, to
capture the boot log, but only once the e-paper panel is idle (PanelIdle).
Each line is written with wall-clock time and seconds since start. If the
device drops off the bus (reset, re-enumeration, standby) the port is
reopened.

  serial_log.py --port '/dev/cu.usbmodem*' --out boot.txt --seconds 40 --reset
"""

import argparse
import datetime
import glob
import re
import time

import serial

REFRESH_START = re.compile(r"PowerHw: cpu ")
REFRESH_DONE = re.compile(r"EpdRefresh: .*cost=")


class PanelIdle:
    """Whether the factory firmware is refreshing the e-paper panel.

    The firmware powers the TPS65185 through the TCA9555 expander for each
    refresh, and an ESP reset leaves the expander as it is: a reset during a
    refresh would leave the panel's high voltage on. In the console log a
    refresh starts with a "PowerHw: cpu" line and ends with "EpdRefresh: ...
    cost=".
    """

    def __init__(self, quiet):
        self.quiet = quiet
        self.restart()

    def restart(self):
        """Forget what was seen, e.g. after reconnecting."""
        self.last, self.busy_since = time.monotonic(), None

    def feed(self, line):
        now = time.monotonic()
        if REFRESH_START.search(line):
            self.last = self.busy_since = now
        elif REFRESH_DONE.search(line):
            self.last, self.busy_since = now, None

    def idle(self):
        """No refresh running, none for `quiet` seconds, and away from minute
        boundaries (the home-screen clock redraws then)."""
        now = time.monotonic()
        if self.busy_since is not None and now - self.busy_since > 3.0:
            self.busy_since = None  # a cpu-frequency change without a refresh
        second = datetime.datetime.now().second
        return (self.busy_since is None and now - self.last >= self.quiet
                and 4 <= second <= 55)


def find_port(pattern):
    matches = sorted(glob.glob(pattern))
    return matches[0] if matches else None


def open_port(path, baud):
    s = serial.Serial()
    s.port = path
    s.baudrate = baud
    s.timeout = 0.05
    s.dsrdtr = True  # only effect on POSIX: open() leaves DTR alone
    s.rts = False
    s.open()
    s.dtr = False
    return s


def reset(s):
    s.dtr = False
    s.rts = True
    time.sleep(0.1)
    s.rts = False


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--port", required=True, help="device path or glob")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", required=True)
    ap.add_argument("--seconds", type=float, default=30.0)
    ap.add_argument("--reset", action="store_true",
                    help="pulse RTS once, as soon as the panel is idle")
    ap.add_argument("--quiet", type=float, default=3.0,
                    help="seconds without a refresh before --reset fires")
    a = ap.parse_args()

    t0 = time.monotonic()
    panel = PanelIdle(a.quiet)
    with open(a.out, "a", buffering=1, encoding="utf-8") as f:
        def emit(text):
            stamp = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
            f.write(f"[{stamp} +{time.monotonic() - t0:8.3f}] {text}\n")

        emit(f"# serial_log start port={a.port} baud={a.baud}")
        s, buf, did_reset, lost = None, b"", False, False
        while time.monotonic() - t0 < a.seconds:
            if s is None:
                path = find_port(a.port)
                try:
                    if path is None:
                        raise OSError("no such port")
                    s = open_port(path, a.baud)
                except (serial.SerialException, OSError):
                    s = None
                    time.sleep(0.05)
                    continue
                emit(f"# port open {path}" + (" (reconnected)" if lost else ""))
                panel.restart()
            if a.reset and not did_reset and panel.idle():
                reset(s)
                did_reset = True
                emit("# reset (RTS pulse, panel idle)")
            try:
                data = s.read(4096)
            except (serial.SerialException, OSError) as e:
                emit(f"# port lost: {e}")
                lost = True
                try:
                    s.close()
                except (serial.SerialException, OSError):
                    pass
                s = None
                continue
            if data:
                buf += data
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    text = line.rstrip(b"\r").decode("utf-8", "replace")
                    emit(text)
                    panel.feed(text)
        if buf:
            emit(buf.decode("utf-8", "replace"))
        if s is not None:
            s.close()
        emit("# serial_log end")


if __name__ == "__main__":
    main()
