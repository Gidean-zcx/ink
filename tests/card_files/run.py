#!/usr/bin/env python3
"""Card files across boots (auto-provision-1): the same card and flash over several boots, a fake install tool with
faults on the USB console, and the card, the tool's log and the probes checked after each boot."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
FONT, OFL = "content/fonts/InkSC.ttf", "content/fonts/InkSC.OFL.txt"


def card(area, name):
    sd = area / name
    books = sd / "content/.m/owner/books"
    books.mkdir(parents=True)
    shutil.copy(ROOT / "tests/fixtures/content/books/星星邮差.txt", books / "书.txt")
    return sd


def boot(build, area, phase, sd, tool=None, binary=None):
    out = area / f"out-{phase}"
    cmd = [str(binary or build / "sim-card-files"), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--script", str(ROOT / "tests/card_files/scenario.script"), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
           "--assets", str(build / "assets"), "--out", str(out), "--flash-image", str(sd) + ".img", "--sd-dir", str(sd),
           "--quiet", "--no-random-outages"]
    log = area / f"tool-{phase}.log"
    if tool is not None:
        cmd += ["--installer", " ".join([sys.executable, str(ROOT / "tests/card_files/fake_host.py"),
                                         "--bundle", str(build / "card-files"), "--log", str(log)] + tool)]
    result = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, errors="replace", timeout=240,
                            env={**os.environ, "INK_CF_PHASE": phase, "UBSAN_OPTIONS": "halt_on_error=1"})
    data = json.loads((out / "metrics.json").read_text(errors="replace")) if (out / "metrics.json").exists() else {}
    counts = data.get("metrics", {})
    if result.returncode or counts.get("test.finished") != 1 or counts.get("test.fail"):
        raise SystemExit(f"{phase}: FAIL exit={result.returncode}\n{result.stdout[-3000:]}\n{result.stderr[-3000:]}\n"
                         f"{data.get('warnings', [])}")
    lines = log.read_text().splitlines() if log.exists() else []
    print(f"{phase}: PASS checks={counts.get('test.pass')}", flush=True)
    return counts, [l[2:] for l in lines if l.startswith("> ")], out


def same(sd, build):
    for path, n in ((FONT, 0), (OFL, 1)):
        got = (sd / path).read_bytes() if (sd / path).exists() else b""
        assert got == (build / f"card-files/files/{n}").read_bytes(), f"{path} differs from the bundle"


def parts(sd):
    return sorted(p.name for p in (sd / "content/fonts").glob("*.part")) if (sd / "content/fonts").exists() else []


def gets(sent):
    return [l.split(" ") for l in sent if l.startswith("INKCF1 GET ")]


def run(build, only=None, binary=None):
    area = build / "test-out" / "card-files"
    if area.exists():
        shutil.rmtree(area)
    area.mkdir(parents=True)
    digest = json.loads((build / "card-files/manifest.json").read_text())["digest"]
    shots = area / "shots"
    started = time.monotonic()
    phases = only or ["A", "B", "C", "H", "D", "E", "E2", "E3", "F", "G"]

    def want(p):
        return p in phases

    one = card(area, "one")
    if want("A") or want("B") or want("C") or want("H"):
        counts, sent, out = boot(build, area, "A", one, binary=binary)
        assert not parts(one) and not counts.get("card_files.written"), (parts(one), counts.get("card_files.written"))
        for name in ("notice-default", "notice-largest"):
            png = next(out.glob(f"clean-{name}.png"))
            assert png.stat().st_size <= 300 * 1024, png
            shots.mkdir(exist_ok=True)
            shutil.copy(png, shots / f"{name}.png")
    if want("B") or want("C") or want("H"):
        counts, sent, out = boot(build, area, "B", one, ["--corrupt-chunk", "5", "--silent-chunk", "9"], binary=binary)
        same(one, build)
        assert counts.get("card_files.chunk_retry") == 2, counts.get("card_files.chunk_retry")
        assert not parts(one)
    if want("C") or want("H"):
        before = [(one / p).stat() for p in (FONT, OFL)]
        counts, sent, out = boot(build, area, "C", one, [], binary=binary)
        assert sent and all(l == f"INKCF1 HELLO {digest} 0" for l in sent), sent
        assert not gets(sent) and not counts.get("card_files.written")
        after = [(one / p).stat() for p in (FONT, OFL)]
        assert [(s.st_size, s.st_mtime_ns) for s in before] == [(s.st_size, s.st_mtime_ns) for s in after]
    if want("H"):
        counts, sent, out = boot(build, area, "H", one, binary=binary)
        shots.mkdir(exist_ok=True)
        shutil.copy(next(out.glob("clean-notice-lent.png")), shots / "notice-lent.png")
    if want("D") or want("E"):
        two = card(area, "two")
        counts, sent, out = boot(build, area, "D", two, ["--unplug-chunk", "300"], binary=binary)
        assert parts(two) == ["InkSC.ttf.part"] and not (two / FONT).exists(), parts(two)
        counts, sent, out = boot(build, area, "E", two, [], binary=binary)
        first = gets(sent)[0]
        assert first[2] == "0" and int(first[4]) > 0 and int(first[4]) % 65536 == 0, first
        same(two, build)
        assert not parts(two) and counts.get("card_files.restart", 0) == 0
    if want("E2"):
        three = card(area, "three")
        boot(build, area, "D2", three, ["--unplug-chunk", "300"], binary=binary)
        part = three / (FONT + ".part")
        data = bytearray(part.read_bytes())
        data[1000] ^= 0x55  # an old list's .part: wrong within its first 64 KiB
        part.write_bytes(bytes(data))
        counts, sent, out = boot(build, area, "E2", three, [], binary=binary)
        assert counts.get("card_files.restart") == 1, counts.get("card_files.restart")
        offsets = [int(g[4]) for g in gets(sent) if g[2] == "0"]
        assert offsets[0] > 0 and 0 in offsets[1:], offsets[:3]
        same(three, build)
    if want("E3"):
        four = card(area, "four")
        (four / "content/fonts").mkdir(parents=True)
        shutil.copy(build / "card-files/files/0", four / (FONT + ".part"))
        shutil.copy(build / "card-files/files/1", four / (OFL + ".part"))
        counts, sent, out = boot(build, area, "E3", four, binary=binary)
        same(four, build)
        assert not parts(four) and not counts.get("card_files.written"), (parts(four), counts.get("card_files.written"))
    if want("F"):
        five = card(area, "five")
        counts, sent, out = boot(build, area, "F", five, ["--bad-file"], binary=binary)
        assert not (five / FONT).exists() and not parts(five), parts(five)
        assert counts.get("card_files.bad_hash") == 1, counts.get("card_files.bad_hash")
        shots.mkdir(exist_ok=True)
        shutil.copy(next(out.glob("clean-notice-failed.png")), shots / "notice-failed.png")
    if want("G"):
        for phase, tool in (("G1", ["--wrong-digest"]), ("G2", ["--data-on-hello"])):
            six = card(area, "six-" + phase)
            counts, sent, out = boot(build, area, phase, six, tool, binary=binary)
            assert not (six / "content/fonts").exists() or not any((six / "content/fonts").iterdir()), phase
            assert not gets(sent) and not counts.get("card_files.written"), phase
    took = time.monotonic() - started
    print(f"card files across boots: PASS in {took:.0f} s", flush=True)
    assert took <= 300, took
    return area


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build", type=Path, required=True)
    p.add_argument("--only", nargs="*")
    p.add_argument("--binary", type=Path)
    p.add_argument("--shots", type=Path, help="copy the notice pictures here")
    a = p.parse_args()
    area = run(a.build.resolve(), a.only, a.binary)
    if a.shots:
        a.shots.mkdir(parents=True, exist_ok=True)
        for png in (area / "shots").glob("*.png"):
            shutil.copy(png, a.shots / png.name)
    if not os.environ.get("INK_KEEP_EVIDENCE"):
        shutil.rmtree(area)
