#!/usr/bin/env python3
"""The install tool against a fake port (auto-provision-1): card_files_serve.Server and trial_run.py's card-file steps,
with a small bundle made here. No fontTools, pyserial or esptool."""
import base64
import contextlib
import hashlib
import io
import json
import sys
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/device"))
sys.path.insert(0, str(ROOT / "tools"))
import card_files  # noqa: E402
import card_files_serve as cfs  # noqa: E402
import trial_run  # noqa: E402


def bundle(where, tamper=False):
    files = [b"first file " * 300, bytes(range(256)) * 20, b"third"]
    entries = []
    (where / "files").mkdir(parents=True)
    for n, data in enumerate(files):
        (where / f"files/{n}").write_bytes(data + (b"!" if tamper and n == 2 else b""))
        entries.append({"path": f"/content/fonts/f{n}", "size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                        "app": "mogo", "file": f"files/{n}"})
    (where / "manifest.json").write_text(json.dumps({"version": 1, "digest": card_files.digest(entries), "entries": entries}))
    return files, entries, card_files.digest(entries)


class Port:
    """What the device says, in pieces; what the tool answers is kept. drop_after: raise once after that many reads."""
    def __init__(self, text, drop_after=None):
        self.data, self.sent, self.drop_after, self.reads = text.encode(), [], drop_after, 0

    def read(self, n):
        self.reads += 1
        if self.drop_after is not None and self.reads > self.drop_after:
            self.drop_after = None
            raise OSError("device reset")
        piece, self.data = self.data[:5], self.data[5:]
        return piece

    def write(self, b):
        self.sent.append(b.decode())


def check(cond, what):
    if not cond:
        raise SystemExit(f"FAIL: {what}")


def main():
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        files, entries, digest = bundle(tmp / "b", tamper=True)
        s = cfs.Server(tmp / "b")
        p = "INKCF1 "
        check(s.handle(f"{p}HELLO {digest} 2\r\n") == f"{p}SERVE {digest}", "same digest: SERVE")
        check(s.handle(f"{p}HELLO {digest} 2") == f"{p}SERVE {digest}" and s.hellos == 2, "a repeated HELLO is answered again")
        check(cfs.Server(tmp / "b").handle(f"{p}HELLO 0000000000000000 2") == f"{p}NOBUNDLE {digest}", "another digest: NOBUNDLE")
        check(cfs.Server(tmp / "b").handle(f"{p}HELLO {digest} 0") == f"{p}ACK", "need 0: ACK")
        sha0, sha1 = entries[0]["sha256"], entries[1]["sha256"]
        check(s.handle(f"{p}GET 7 {sha0} 0 16") == f"{p}NO 7 unknown", "no such index")
        check(s.handle(f"{p}GET 0 {sha1} 0 16") == f"{p}NO 0 sha", "another file's hash")
        check(s.handle(f"{p}GET 0 {sha0} {len(files[0]) - 8} 16") == f"{p}NO 0 range", "past the end")
        check(s.handle(f"{p}GET 0 {sha0} 0 2049") == f"{p}NO 0 len", "longer than 2048")
        check(s.handle(f"{p}GET 2 {entries[2]['sha256']} 0 5") == f"{p}NO 2 sha", "a bundle file that does not match is not served")
        reply = s.handle(f"{p}GET 1 {sha1} 1000 2048").split(" ")
        chunk = base64.b64decode(reply[6])
        check(reply[1:5] == ["DATA", "1", "1000", "2048"] and chunk == files[1][1000:3048], "a chunk from the middle")
        check(int(reply[5], 16) == zlib.crc32(chunk) and len(reply[5]) == 8, "its CRC is zlib's, 8 lower-case hex digits")
        check(s.handle(f"{p}DONE 1 ok") == f"{p}ACK" and s.handle("I (123) boot: a log line") is None, "DONE: ACK; log lines: none")
        check(s.handle(f"{p}BYE 1 0") == f"{p}ACK" and s.state == "done ok=1 failed=0", "BYE ends it")

        # serve(): a whole conversation on a port that drops once and comes back
        good = Path(tmp / "good")
        files, entries, digest = bundle(good)
        talk = (f"boot log line\r\n{p}HELLO {digest} 1\r\n{p}GET 0 {entries[0]['sha256']} 0 100\r\n"
                f"{p}DONE 0 ok\r\n{p}BYE 1 0\r\n")
        ports = [Port(talk[:40], drop_after=8), Port(talk[40:])]
        log = io.StringIO()
        result = cfs.Server(good).serve(lambda: ports.pop(0) if ports else None, log, wait_s=5, idle_s=5)
        check(result == "done ok=1 failed=0", f"served across a dropped port: {result}")
        check("boot log line\n" in log.getvalue() and "# port lost" in log.getvalue(), "raw lines and the drop are logged")
        port = Port(f"{p}HELLO {digest} 0\n")
        check(cfs.Server(good).serve(lambda: port, io.StringIO(), wait_s=5) == "all-present" and port.sent == [f"{p}ACK\n"],
              "need 0: ACK and done at once")
        check(cfs.Server(good).serve(lambda: Port("only logs\n"), io.StringIO(), wait_s=0.3) == "no-hello", "a silent device")

        # trial_run.py: --install stops before writing without the bundle; --card-files-only needs no backup, no esptool
        def no_esptool(*args, **kw):
            raise SystemExit("FAIL: esptool was run")
        trial_run.base.Run.esptool = no_esptool
        trial_run.subprocess.call = no_esptool
        (tmp / "img").mkdir()
        (tmp / "img/ink-s31.bin").write_bytes(b"\xe9" + bytes(100))
        (tmp / "img/ink-fonts.bin").write_bytes(b"INKF" + bytes(60))
        (tmp / "backup.bin").write_bytes(bytes(16))
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = trial_run.main(["--port", "/dev/null-fake", "--backup", str(tmp / "backup.bin"), "--image",
                                 str(tmp / "img/ink-s31.bin"), "--fonts", str(tmp / "img/ink-fonts.bin"), "--out",
                                 str(tmp / "o1"), "--install", "--dry-run"])
        check(rc == 2 and "STOP before writing anything: no install bundle" in out.getvalue(),
              f"--install without the bundle: exit 2 before writing ({rc})\n{out.getvalue()[-500:]}")
        trial_run.open_console = lambda a, reset: (lambda: Port(f"{p}HELLO {digest} 0\n"))
        with contextlib.redirect_stdout(io.StringIO()):
            rc = trial_run.main(["--port", "/dev/null-fake", "--out", str(tmp / "o2"), "--card-files-only", "--card-files", str(good)])
        check(rc == 0 and "all-present" in (tmp / "o2/run_log.txt").read_text(), f"--card-files-only: served ({rc})")
        trial_run.open_console = lambda a, reset: (lambda: Port(f"{p}HELLO {digest} 1\n{p}BYE 0 1\n"))
        with contextlib.redirect_stdout(io.StringIO()):
            rc = trial_run.main(["--port", "/dev/null-fake", "--out", str(tmp / "o3"), "--card-files-only", "--card-files", str(good)])
        check(rc == 5, f"--card-files-only, a file failed: exit 5 ({rc})")
    print("card files install tool: Server, serve() and trial_run.py card-file steps: PASS")


if __name__ == "__main__":
    main()
