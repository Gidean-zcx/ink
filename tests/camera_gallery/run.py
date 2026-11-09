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
p.add_argument("--revert", choices=("api", "names", "wait"))
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix="camera-gallery-", dir=build))
binary = build / "sim-camera-gallery"
if a.negative or a.revert:
    relative = "../../system/apps/gallery/gallery.c" if a.negative else "../../system/services/camera/camera_service.c" if a.revert == "wait" else "../../system/apps/camera/camera.c"
    original = (ROOT / "tests/camera_gallery" / relative).resolve().read_text()
    if a.negative:
        old = "if (!found) c->rc.status = INK_RC_REJECTED;"
        assert original.count(old) == 1
        changed = original.replace(old, "if (false && !found) c->rc.status = INK_RC_REJECTED;")
    elif a.revert == "api":
        changed = original.replace('#include "ink/sys/camera.h"', 'uint8_t *ink_camera_read_rgb(ink_call_t *c, int *w, int *h);')
        changed = changed.replace("ink_camera_read_rgb(&w, &h)", "ink_camera_read_rgb(c, &w, &h)")
    elif a.revert == "names":
        start = original.index("        unsigned seq = 0, n;")
        end = original.index("        ink_fs_dir_close(dir);", start) + len("        ink_fs_dir_close(dir);")
        changed = original[:start] + "        static unsigned seq;" + original[end:]
    else:
        start = original.index("uint8_t *ink_camera_read_rgb(")
        changed = original[:start] + original[start:].replace("osal_mutex_lock(CAM.lock, OSAL_WAIT_FOREVER);", "while (osal_mutex_lock(CAM.lock, 0) != OSAL_OK) osal_task_yield();", 1)
    mutant = work / Path(relative).name
    mutant.write_text(changed)
    source = (ROOT / "tests/camera_gallery/camera_gallery.c").read_text()
    for include in ("../../system/apps/camera/camera.c", "../../system/apps/gallery/gallery.c", "../../system/services/camera/camera_service.c", "../../system/boot/main.c", "../integration/whole_product.h"):
        absolute = mutant if include == relative else (ROOT / "tests/camera_gallery" / include).resolve()
        source = source.replace(f'"{include}"', f'"{absolute}"')
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
               "-I", str(ROOT / "system/apps/gallery"), "-I", str(ROOT / "system/apps/camera"), "-c", str(driver), "-o", str(obj)]
    compiled = subprocess.run(command, cwd=build, capture_output=True, text=True)
    (work / "compile.log").write_text(compiled.stdout + compiled.stderr)
    if a.revert == "api":
        assert compiled.returncode and "conflicting types" in compiled.stderr and "ink_camera_read_rgb" in compiled.stderr, compiled.stderr
        print("PASS: private capability-dependent declaration violates the public RGB interface")
        if os.environ.get("INK_KEEP_EVIDENCE"):
            print("Evidence:", work.relative_to(ROOT))
        else:
            shutil.rmtree(work)
        raise SystemExit(0)
    if compiled.returncode:
        print(compiled.stderr)
        raise SystemExit("negative-control compile failed")
    link = [str(obj) if part.endswith("tests/camera_gallery/camera_gallery.c.o") else part for part in link]
    binary = work / "sim-negative"
    link[link.index("-o") + 1] = str(binary)
    if subprocess.run(link, cwd=build).returncode:
        raise SystemExit("negative-control link failed")

phases = [("write", "Camera"), ("restart", ""), ("service", ""), ("many", ""), ("many-restart", "")]
if a.visual:
    phases += [("write", "Gallery"), ("write", "Photo")]
if a.negative:
    phases = [("write", "")]
elif a.revert:
    phases = [("many", "")] if a.revert == "names" else [("service", "")]
for phase, scene in phases:
    label = scene or phase
    target = work / label
    state = work / ("many-state" if phase.startswith("many") else "service-state" if phase == "service" else "normal" if scene in ("", "Camera") else scene)
    state.mkdir(exist_ok=True)
    sd = state / "sd"
    sd.mkdir(exist_ok=True)
    if phase == "many":
        folder = sd / "content/.m/owner/照片"
        folder.mkdir(parents=True)
        fixture = next((work / "normal/sd/content/.m/owner/照片").glob("*.png"), None)
        for i in range(1, 301):
            target_photo = folder / f"照片-{i}.png"
            if fixture:
                shutil.copyfile(fixture, target_photo)
            else:
                Image.new("RGB", (1, 1), "black").save(target_photo)
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
    if a.negative or a.revert:
        evidence = result.stdout + result.stderr + json.dumps(metrics.get("warnings", []))
        expected = "child cannot open an adult photo by known id" if a.negative else "300 existing photos require at most two exclusive file opens" if a.revert == "names" else "camera contention blocks without yielding in a busy loop"
        assert not passed and counts.get("test.finished") == 1 and expected in evidence, evidence[-4000:]
        print("PASS: removing the fix fails the unchanged regression assertion:", expected)
        break
    if not passed:
        print("Evidence:", work.relative_to(ROOT))
        raise SystemExit(json.dumps(metrics.get("warnings", []), ensure_ascii=False) + result.stderr[-2500:])
    if phase == "service":
        continue
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
