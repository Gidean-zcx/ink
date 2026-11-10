"""Run committed fixtures, preserve screenshots and reject credential leaks in textual evidence."""
import argparse
import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--negative", action="store_true", help="remove Wi-Fi redaction in a temporary object")
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix="qr-scan-", dir=build))
images = work / "images"
images.mkdir()
(work / "sd").mkdir()
binary = build / "sim-qr-scan"
if a.negative:
    text = (ROOT / "system/apps/camera/scan.c").read_text()
    original = 'snprintf(p->content, sizeof p->content, "%s", name);'
    assert text.count(original) == 1
    mutant = work / "scan.c"
    mutant.write_text(text.replace(original, 'snprintf(p->content, sizeof p->content, "%.300s", text);'))
    text = (ROOT / "tests/qr_scan/device.c").read_text()
    for relative in ("../../system/apps/camera/scan.c", "../../system/boot/main.c", "../integration/whole_product.h"):
        target = mutant if relative.endswith("camera/scan.c") else (ROOT / "tests/qr_scan" / relative).resolve()
        text = text.replace(f'"{relative}"', f'"{target}"')
    driver = work / "device.c"
    driver.write_text(text)
    flags = {}
    for line in (build / "CMakeFiles/sim-qr-scan.dir/flags.make").read_text().splitlines():
        if " = " in line:
            k, v = line.split(" = ", 1)
            flags[k] = shlex.split(v)
    link = shlex.split((build / "CMakeFiles/sim-qr-scan.dir/link.txt").read_text())
    obj = work / "negative.o"
    subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"], "-c", str(driver), "-o", str(obj)], check=True)
    link = [str(obj) if s.endswith("tests/qr_scan/device.c.o") else s for s in link]
    binary = work / "sim-negative"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
for src in (ROOT / "tests/qr_scan/images").glob("*.png"):
    gray = Image.open(src).convert("L").tobytes()
    pixels = bytearray(2 * len(gray))
    for i, g in enumerate(gray):
        value = (g >> 3) << 11 | (g >> 2) << 5 | (g >> 3)
        pixels[2*i] = value >> 8
        pixels[2*i+1] = value & 255
    (images / (src.stem + ".rgb565")).write_bytes(pixels)
command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--script", str(ROOT / "tests/qr_scan/scan.script"), "--assets", str(build / "assets"),
           "--content", str(build / "test-content"), "--out", str(work / "out"),
           "--flash-image", str(work / "user.img"), "--sd-dir", str(work / "sd"),
           "--quiet", "--no-random-outages", "--budget", str(ROOT / "tests/ui_scene.budget.json")]
r = subprocess.run(command, env={**os.environ, "INK_QR_IMAGES": str(images)}, capture_output=True, text=True, timeout=180)
(work / "run.log").write_text(r.stdout + r.stderr)
if not (work / "out/metrics.json").exists():
    raise SystemExit(f"exit {r.returncode}\n" + r.stdout + r.stderr)
metrics = json.loads((work / "out/metrics.json").read_text())
counts = metrics.get("metrics", {})
print("qr-scan", "exit", r.returncode, "assertions", counts.get("test.pass", 0), "failures", counts.get("test.fail", 0))
if a.negative:
    assert r.returncode and counts.get("test.fail", 0) and "AI reply contains no Wi-Fi credential" in json.dumps(metrics.get("warnings", []))
    print("PASS: removing Wi-Fi redaction fails the unchanged privacy assertions")
else:
    for path in [work / "run.log", *(work / "out").glob("*.json"), *(work / "out").glob("*.jsonl"), *(work / "out").glob("*.ndjson")]:
        assert "FAKE-qr-secret" not in path.read_text() and "FAKE;secret" not in path.read_text(), f"credential leak: {path.name}"
if not a.negative and (r.returncode or counts.get("test.fail", 0) or counts.get("test.finished") != 1):
    print("Evidence:", work.relative_to(ROOT))
    raise SystemExit(json.dumps(metrics.get("warnings", []), ensure_ascii=False) + r.stderr[-2500:])
screens = build / "qr-scan-screens"
screens.mkdir(exist_ok=True)
for src in (() if a.negative else (work / "out").glob("snap-*.png")):
    shutil.copyfile(src, screens / src.name)
if os.environ.get("INK_KEEP_EVIDENCE"):
    print("Evidence:", work.relative_to(ROOT))
else:
    shutil.rmtree(work)
