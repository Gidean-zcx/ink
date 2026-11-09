"""Exercise capture, paging, real pixels, deletion, isolation and restart."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--negative", action="store_true")
p.add_argument("--visual", action="store_true")
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix="camera-gallery-", dir=build))
binary = build / "sim-camera-gallery"
if a.negative:
    original = (ROOT / "system/apps/gallery/gallery.c").read_text()
    old = "if (!found) { c->rc.status = INK_RC_REJECTED; return; }"
    assert original.count(old) == 1
    mutant = work / "gallery.c"
    mutant.write_text(original.replace(old, "if (false && !found) { c->rc.status = INK_RC_REJECTED; return; }"))
    source = (ROOT / "tests/camera_gallery/camera_gallery.c").read_text()
    for relative in ("../../system/apps/camera/camera.c", "../../system/boot/main.c", "../integration/whole_product.h"):
        absolute = (ROOT / "tests/camera_gallery" / relative).resolve()
        source = source.replace(f'"{relative}"', f'"{absolute}"')
    source = source.replace('"../../system/apps/gallery/gallery.c"', f'"{mutant}"')
    driver = work / "driver.c"
    driver.write_text(source)
    flags = {}
    for line in (build / "CMakeFiles/sim-camera-gallery.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-camera-gallery.dir/link.txt").read_text())
    obj = work / "driver.o"
    command = [link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
               "-I", str(ROOT / "system/apps/gallery"), "-c", str(driver), "-o", str(obj)]
    if subprocess.run(command, cwd=build).returncode:
        raise SystemExit("negative-control compile failed")
    link = [str(obj) if part.endswith("tests/camera_gallery/camera_gallery.c.o") else part for part in link]
    binary = work / "sim-negative"
    link[link.index("-o") + 1] = str(binary)
    if subprocess.run(link, cwd=build).returncode:
        raise SystemExit("negative-control link failed")

phases = [("write", "Camera"), ("restart", "")]
if a.visual:
    phases += [("write", "Gallery"), ("write", "Photo")]
if a.negative:
    phases = [("write", "")]
for phase, scene in phases:
    label = scene or phase
    target = work / label
    state = work / ("normal" if scene in ("", "Camera") else scene)
    state.mkdir(exist_ok=True)
    sd = state / "sd"
    sd.mkdir(exist_ok=True)
    command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
               "--script", str(ROOT / "tests/camera_gallery/camera.script"),
               "--assets", str(build / "assets"), "--content", str(build / "test-content"),
               "--out", str(target), "--flash-image", str(state / "user.img"), "--sd-dir", str(sd),
               "--quiet", "--no-random-outages", "--budget", str(ROOT / "tests/ui_scene.budget.json")]
    result = subprocess.run(command, env={**os.environ, "INK_CAMERA_PHASE": phase, "INK_CAMERA_SCENE": scene},
                            capture_output=True, text=True, timeout=150)
    (work / f"{label}.log").write_text(result.stdout + result.stderr)
    metrics = json.loads((target / "metrics.json").read_text()) if (target / "metrics.json").exists() else {}
    counts = metrics.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
    print(label, "PASS" if passed else "FAIL", "exit", result.returncode,
          "assertions", counts.get("test.pass", 0), "failures", counts.get("test.fail", 0), flush=True)
    if a.negative:
        evidence = result.stdout + result.stderr + json.dumps(metrics.get("warnings", []))
        expected = "child cannot open an adult photo by known id"
        assert not passed and counts.get("test.finished") == 1 and expected in evidence, evidence[-4000:]
        print("PASS: removing the photo ownership check fails the unchanged child-isolation assertions")
        break
    if not passed:
        print("Evidence:", work.relative_to(ROOT))
        raise SystemExit(json.dumps(metrics.get("warnings", []), ensure_ascii=False) + result.stderr[-2500:])
    photos = list(sd.glob("content/.m/owner/照片/*.png"))
    assert photos, "no persisted adult camera files"
    for photo in photos:
        with Image.open(photo) as im:
            assert im.format == "PNG" and im.mode == "RGB" and im.size == (640, 480)
            assert len(im.getcolors(640 * 480) or []) > 2, "capture lost camera colors"
    if scene:
        compare = build / "camera-gallery-compare" / scene
        compare.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(target / "snap-scene.png", compare / "snap-scene.png")
        shutil.copyfile(target / "metrics.json", compare / "metrics.json")
if os.environ.get("INK_KEEP_EVIDENCE"):
    print("Evidence:", work.relative_to(ROOT))
else:
    shutil.rmtree(work)
