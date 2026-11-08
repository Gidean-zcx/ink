"""An external process uploads real bytes over loopback TCP into the simulator."""
import argparse
import http.client
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.parse

p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--sim", type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
build = a.build.resolve()
pattern = b"SYNTHETIC\0UPLOAD\n"
names = {"dotdot": "..", "absolute": "/outside.txt", "nested": "../outside.txt",
         "control": "bad\x01.txt", "nul": "bad\0.txt", "backslash": "bad\\file.txt"}

def request(session, name, body, code=None):
    conn = http.client.HTTPConnection("127.0.0.1", session["port"], timeout=5)
    try:
        conn.request("POST", "/upload", body, {"X-Ink-Code": code or session["code"], "X-Ink-Name": name})
        response = conn.getresponse()
        return response.status, response.read().decode()
    finally:
        conn.close()

def begin(session, name, size):
    sock = socket.create_connection(("127.0.0.1", session["port"]), timeout=5)
    head = f"POST /upload HTTP/1.1\r\nX-Ink-Code: {session['code']}\r\nX-Ink-Name: {name}\r\nContent-Length: {size}\r\n\r\n"
    sock.sendall(head.encode("ascii") + b"SYNTHETIC")
    return sock

with tempfile.TemporaryDirectory(prefix="wireless-", dir=build) as temp:
    out = Path(temp)
    script = out / "network.script"
    script.write_text("@0 net wifi\n@400s end\n")
    command = [str(a.sim.resolve() if a.sim else build / "sim-wireless-transfer"), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
               "--assets", str(build / "assets"), "--content", str(build / "test-content"),
               "--out", str(out), "--script", str(script), "--realtime=20", "--quiet", "--no-random-outages"]
    saved = None
    with (out / "sim.log").open("w+") as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
        try:
            sequence = 0
            deadline = time.monotonic() + 50
            while process.poll() is None:
                assert time.monotonic() < deadline, "simulator/client deadline exceeded"
                try:
                    stage = json.loads((out / "session.json").read_text())
                except (FileNotFoundError, json.JSONDecodeError):
                    time.sleep(0.005)
                    continue
                if stage["sequence"] == sequence:
                    time.sleep(0.005)
                    continue
                sequence = stage["sequence"]
                case, session = stage["case"], stage["session"]
                if case == "success":
                    conn = http.client.HTTPConnection("127.0.0.1", session["port"], timeout=5)
                    conn.request("GET", "/")
                    response = conn.getresponse()
                    assert response.status == 200 and "<form>" in response.read().decode()
                    conn.close()
                    assert request(session, urllib.parse.quote("合成.txt"), pattern * 4096) == (201, "文件已收到")
                    try:
                        request(session, "replay.txt", pattern)
                    except (OSError, http.client.HTTPException):
                        pass
                    else:
                        raise AssertionError("used authorization accepted a replay")
                elif case in names or case == "utf8":
                    encoded = "%FF.txt" if case == "utf8" else urllib.parse.quote(names[case], safe="")
                    assert request(session, encoded, pattern)[0] == 400
                elif case == "wrong_code":
                    assert request(session, "wrong.txt", pattern, "000000")[0] == 403
                elif case == "raw_nul":
                    raw = socket.create_connection(("127.0.0.1", session["port"]), timeout=5)
                    head = f"POST /upload HTTP/1.1\r\nX-Ink-Code: {session['code']}\r\nContent-Length: 16\r\nX-Ink-Name: "
                    raw.sendall(head.encode() + b"bad\0.txt\r\n\r\n" + pattern)
                    response = http.client.HTTPResponse(raw)
                    response.begin()
                    assert response.status == 400
                    response.read()
                    raw.close()
                elif case == "disconnect":
                    saved = begin(session, "disconnect.txt", 1000)
                    saved.close()
                elif case == "full":
                    assert request(session, "full.txt", pattern)[0] == 507
                elif case == "midfull":
                    saved = begin(session, "midfull.txt", 1000)
                elif case == "midfull_finish":
                    saved.sendall(b"X" * 100)
                    assert b"507" in saved.recv(4096)
                    saved.close()
                elif case == "idle":
                    saved = begin(session, "idle.txt", 1000)
                elif case in ("expired", "switched"):
                    if saved:
                        saved.close()
                    try:
                        request(session, "expired.txt", pattern)
                    except (OSError, http.client.HTTPException):
                        pass
                    else:
                        raise AssertionError("expired/member-switched session still listens")
                elif case == "child":
                    assert request(session, "child.txt", pattern)[0] == 201
                else:
                    raise AssertionError(case)
                (out / f"ack-{sequence}").touch()
            assert process.returncode == 0
            metrics = json.loads((out / "metrics.json").read_text())
            counters = metrics["metrics"]
            assert counters.get("test.finished") == 1 and counters.get("test.fail", 0) == 0, metrics.get("warnings")
            print(f"PASS: external HTTP upload, {sequence} client stages, {int(counters['test.pass'])} device checks")
        except BaseException:
            if process.poll() is None:
                process.kill()
            process.wait()
            log.seek(0)
            print(log.read()[-10000:])
            raise
        finally:
            if saved:
                saved.close()
