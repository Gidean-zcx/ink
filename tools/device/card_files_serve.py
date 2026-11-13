#!/usr/bin/env python3
"""Serve the card files (D-138) to a device on its USB serial console, protocol INKCF1 (docs/system/card-files.md).

The device asks; this answers exactly one line for every protocol line it sends. What it sends comes only from the
install bundle that the build made next to the firmware (build-s31/card-files), and only files whose bytes still match
the bundle's manifest. No pyserial or esptool here: the caller passes a function that opens the port.

  card_files_serve.py --stdio --bundle DIR    one reply line on stdout per stdin line (#NONE: no reply), for the simulator
"""
import argparse
import base64
import datetime
import hashlib
import json
import sys
import time
import zlib
from pathlib import Path

PREFIX = "INKCF1 "
CHUNK = 2048  # INK_CARD_CHUNK


class Server:
    def __init__(self, bundle_dir):
        bundle = Path(bundle_dir)
        manifest = json.loads((bundle / "manifest.json").read_text())
        self.digest, self.entries = manifest["digest"], manifest["entries"]
        self.data = []  # bytes, "read" (cannot read it) or "sha" (not what the manifest says)
        for e in self.entries:
            try:
                data = (bundle / e["file"]).read_bytes()
            except OSError:
                self.data.append("read")
                continue
            ok = len(data) == e["size"] and hashlib.sha256(data).hexdigest() == e["sha256"]
            self.data.append(data if ok else "sha")
        self.state, self.hellos, self.gets, self.done = None, 0, 0, {}

    def chunk(self, idx, offset, length):
        """The bytes the device asked for; a fake host may override this to inject faults."""
        return self.data[idx][offset:offset + length]

    def get(self, f):
        self.gets += 1
        try:
            idx, sha, offset, length = int(f[1]), f[2], int(f[3]), int(f[4])
        except (ValueError, IndexError):
            return f"{PREFIX}NO {f[1] if len(f) > 1 else '-'} unknown"
        if not 0 <= idx < len(self.entries):
            return f"{PREFIX}NO {idx} unknown"
        e = self.entries[idx]
        if sha != e["sha256"]:
            return f"{PREFIX}NO {idx} sha"
        if not isinstance(self.data[idx], bytes):
            return f"{PREFIX}NO {idx} {self.data[idx]}"
        if length <= 0 or length > CHUNK:
            return f"{PREFIX}NO {idx} len"
        if offset < 0 or offset + length > e["size"]:
            return f"{PREFIX}NO {idx} range"
        data = self.chunk(idx, offset, length)
        crc = zlib.crc32(data) & 0xFFFFFFFF
        return f"{PREFIX}DATA {idx} {offset} {length} {crc:08x} {base64.b64encode(data).decode()}"

    def handle(self, line):
        """The reply to one line from the device, or None (not a protocol line: only logged)."""
        line = line.rstrip("\r\n")
        if not line.startswith(PREFIX):
            return None
        f = line[len(PREFIX):].split(" ")
        if f[0] == "HELLO" and len(f) == 3:
            self.hellos += 1
            if f[2] == "0":
                self.state = "all-present"
                return f"{PREFIX}ACK"
            if f[1] == self.digest:
                return f"{PREFIX}SERVE {self.digest}"
            self.state = "nobundle"
            return f"{PREFIX}NOBUNDLE {self.digest}"
        if f[0] == "GET":
            return self.get(f)
        if f[0] == "DONE" and len(f) == 3:
            self.done[f[1]] = f[2]
            return f"{PREFIX}ACK"
        if f[0] == "BYE" and len(f) == 3:
            self.state = f"done ok={f[1]} failed={f[2]}"
            return f"{PREFIX}ACK"
        return None

    def serve(self, open_port, log, wait_s=90.0, idle_s=30.0):
        """Answer the device until its session ends. open_port() returns an object with read(n) and write(b) (a serial
        port); a port that drops (a reset, re-enumeration) is opened again. Every raw line goes into `log`.
        Returns all-present, done ok=N failed=M, no-hello, nobundle or idle."""
        port, buf, start, heard = None, b"", time.monotonic(), None
        while True:
            now = time.monotonic()
            if self.state:
                return self.state
            if heard is None and now - start > wait_s:
                return "no-hello"
            if heard is not None and now - heard > idle_s:
                return "idle"
            if port is None:
                try:
                    port = open_port()
                except OSError:
                    port = None
                if port is None:
                    time.sleep(0.1)
                    continue
                log.write(f"# port open {datetime.datetime.now():%H:%M:%S.%f}\n")
            try:
                data = port.read(4096)
            except OSError as e:  # serial.SerialException is an OSError
                log.write(f"# port lost: {e}\n")
                port, buf = None, b""
                continue
            buf += data or b""
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                text = raw.rstrip(b"\r").decode("utf-8", "replace")
                log.write(text + "\n")
                reply = self.handle(text)
                if reply is None:
                    continue
                heard = time.monotonic()
                try:
                    port.write((reply + "\n").encode())
                except OSError as e:
                    log.write(f"# port lost: {e}\n")
                    port, buf = None, b""
                    break
                if self.state:
                    return self.state


def stdio(server, inp=sys.stdin, out=sys.stdout):
    for line in inp:
        reply = server.handle(line)
        out.write((reply if reply is not None else "#NONE") + "\n")
        out.flush()


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--bundle", required=True, help="the install bundle: the build's card-files directory")
    ap.add_argument("--stdio", action="store_true", help="answer stdin lines on stdout (the simulator's installer)")
    a = ap.parse_args(argv)
    if not a.stdio:
        ap.error("only --stdio here; trial_run.py serves a real port")
    stdio(Server(a.bundle))


if __name__ == "__main__":
    main()
