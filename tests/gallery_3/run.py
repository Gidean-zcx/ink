"""gallery-3 on the simulator: synthetic photos on a host card folder with file times set here, the walks in
gallery.c, and the refreshes counted from the display trace (as tests/control_center.py).

--screens copies the clean pictures to docs/tasks/gallery-3/. --reverse runs the four reverse checks: each source
change must make an unchanged assertion fail. --phase runs one walk.
"""
import argparse
import datetime
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import fixtures  # noqa: E402

WAVES = ('GC16', 'GL16', 'DU', 'A2')
TZ = datetime.timezone(datetime.timedelta(hours=8))
RUNS = [("walk", "1"), ("walk", "3"), ("ai", "1"), ("kid", "1"), ("more", "1"), ("purge", "1"), ("empty", "1"), ("empty", "3"), ("more", "3")]
# step: (most refreshes, must there be one, the box name); the box is published by gallery.c
STEPS = {"open": (3, True), "turn": (3, True), "turn_late": (3, True), "months": (2, True), "albums": (2, True), "open_photo": (2, True), "tap1": (0, False),
         "zoom2": (1, True), "drag": (1, True), "pan": (1, True), "rotate": (1, True), "fit": (1, True), "next": (1, True),
         "info": (1, True), "info_back": (1, True), "trash": (1, True), "undo": (2, True), "restore": (2, True), "book": (0, False)}
REVERSE = {
    "exif": ("system/services/media/image.c", "    if (info.orientation > 1) {", "    if (0) {", "more", "exif_orientation"),
    "trash": ("system/apps/gallery/gallery.c", "ink_fs_move_if_revision(m->from, m->to, revision, NULL)", "ink_fs_remove(m->from)", "walk", "trash_no_confirm"),
    "return": ("system/apps/gallery/gallery.c", "    if (at < G.count) { G.position.page = at / PHOTO_SLOTS;", "    if (false) { G.position.page = at / PHOTO_SLOTS;", "walk", "return_position"),
    "purge": ("system/apps/gallery/gallery.c", "    if (!s->all && !ink_time_valid()) { osal_free(s); return; }", "    if (false) { osal_free(s); return; }", "purge", "purge_time_invalid"),
}


def when(month, day, hour=10, minute=0):
    return datetime.datetime(2026, month, day, hour, minute, tzinfo=TZ).timestamp()


def png(path, shade, size=(120, 90)):
    im = Image.new("L", size, 255)
    d = ImageDraw.Draw(im)
    d.rectangle((10, 10, size[0] - 10, size[1] - 10), fill=shade)
    d.line((0, 0, size[0], size[1]), fill=0, width=3)
    im.save(path)


def card(sd, phase):
    me = sd / "content/.m/owner"
    trip, photos = me / "旅行", me / "照片"
    trip.mkdir(parents=True)
    photos.mkdir(parents=True)
    files = []
    if phase != "empty":
        for i in range(4):
            files += [(trip / f"sep-{i + 1:02}.png", when(9, 20 + i)), (photos / f"sep-{i + 5:02}.png", when(9, 3 + i)),
                      (photos / f"oct-{i + 1:02}.png", when(10, 2 + i)), (trip / f"oct-{i + 5:02}.png", when(10, 10 + i))]
        for i in range(8):
            files.append((photos / f"nov-{i + 1:02}.png", when(11, 1 + i, 9)))
        for k, (path, _) in enumerate(files):
            png(path, 40 + 20 * (k % 10))
        words = Image.new("L", (600, 400), 255)
        d = ImageDraw.Draw(words)
        d.rectangle((40, 40, 300, 220), fill=60)
        d.text((444, 333), "SMALL WORDS 12345", fill=0)
        words.save(trip / "字条.png")
        files.append((trip / "字条.png", when(9, 28)))
        (photos / "竖拍.jpg").write_bytes(fixtures.jpeg(fixtures.Image.new("L", (300, 200), 128), 6, "2026:11:02 10:20:00"))
        (photos / "大图.jpg").write_bytes(fixtures.big_jpeg())
        (photos / "动图.webp").write_bytes(fixtures.webp_head())
        files += [(photos / "竖拍.jpg", when(11, 9)), (photos / "大图.jpg", when(11, 10)), (photos / "动图.webp", when(11, 11))]
    if phase == "more":
        (photos / "坏的.png").write_bytes(b"not really a picture, only a synthetic broken file")
        longer = me / "长路径" / ("b" * 55)  # each name under 64 bytes, the whole path 110: in 最近删除 it would be 134
        longer.mkdir(parents=True)
        png(longer / ("a" * 36 + ".png"), 90)
        files += [(photos / "坏的.png", when(11, 12)), (longer / ("a" * 36 + ".png"), when(11, 13))]
    kid = sd / "content/.m/kid/照片"
    kid.mkdir(parents=True)
    png(kid / "kid-1.png", 30)
    files.append((kid / "kid-1.png", when(11, 15)))
    for path, t in files:
        os.utime(path, (t, t))


def within(u, box):
    return not (u['x'] < box[0] - 1 or u['y'] < box[1] - 1 or u['x'] + u['w'] - 1 > box[2] + 1 or u['y'] + u['h'] - 1 > box[3] + 1)


def inside(trace, metrics, step):
    """Refreshes between g3.<step>.before and .after: how many, those outside test.<step>.* (native pixels), those
    while the finger was down. Press feedback inside test.<step>_press.* is counted apart. The display's own periodic
    clean (refresh_policy.c: after enough of the screen changed, the next big change flashes the whole panel once)
    stands in for the step's one refresh: it is counted, not taken as drawing outside."""
    marks = {e['name']: e['ts'] for e in trace if e.get('name', '').startswith('g3.' + step + '.')}
    t0, t1, held = marks.get('g3.%s.before' % step), marks.get('g3.%s.after' % step), marks.get('g3.%s.held' % step)
    box = [metrics.get('test.%s.%s' % (step, edge), {}).get('max') for edge in ('x1', 'y1', 'x2', 'y2')]
    press = [metrics.get('test.%s_press.%s' % (step, edge), {}).get('max') for edge in ('x1', 'y1', 'x2', 'y2')]
    if t0 is None or t1 is None or None in box:
        return None
    updates = [e for e in trace if e.get('name') in WAVES and t0 <= e.get('ts', -1) <= t1 and 'args' in e]
    pressed = [u for u in updates if None not in press and within(u['args'], press)]
    updates = [u for u in updates if u not in pressed]
    panel = max((u['args']['w'] * u['args']['h'] for u in updates), default=0)
    cleans = [u for u in updates if u['name'] == 'GC16' and u['args']['x'] == 0 and u['args']['y'] == 0 and
              u['args']['w'] * u['args']['h'] == panel and panel >= 1216 * 684]
    outside = [u['args'] for u in updates if not within(u['args'], box) and u not in cleans]
    during = [u for u in updates if held is not None and u['ts'] <= held]
    return len(updates), outside, len(during), len(pressed), len(cleans)


def mutant(build, work, name):
    """The sim-gallery-3 driver compiled against one changed source file."""
    relative, old, new, _, _ = REVERSE[name]
    text = (ROOT / relative).read_text()
    assert text.count(old) == 1, (name, old)
    changed = work / Path(relative).name
    changed.write_text(text.replace(old, new))
    driver = (ROOT / "tests/gallery_3/gallery.c").read_text()
    for include in ("../../system/apps/gallery/gallery.c", "../../system/services/media/image.c", "../../system/boot/main.c", "../integration/whole_product.h"):
        source = (ROOT / "tests/gallery_3" / include).resolve()
        driver = driver.replace(f'"{include}"', f'"{changed if source == (ROOT / relative).resolve() else source}"')
    (work / "driver.c").write_text(driver)
    flags = {}
    for line in (build / "CMakeFiles/sim-gallery-3.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-gallery-3.dir/link.txt").read_text())
    obj = work / "driver.o"
    r = subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"], "-I", str(ROOT / "system/apps/gallery"),
                        "-I", str(ROOT / "system/services/media"), "-c", str(work / "driver.c"), "-o", str(obj)], cwd=build, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(r.stderr[-3000:])
    link = [str(obj) if part.endswith("tests/gallery_3/gallery.c.o") else part for part in link]
    binary = work / "sim-mutant"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    return binary


def run(build, work, binary, phase, size):
    name = f"{phase}-{size}"
    out = work / name
    sd = work / (name + "-sd")
    shutil.rmtree(out, ignore_errors=True)
    shutil.rmtree(sd, ignore_errors=True)
    out.mkdir(parents=True)
    card(sd, phase)
    env = {**os.environ, "INK_G3_PHASE": phase, "INK_G3_SIZE": size}
    r = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"), "--end", "900s",
                        "--budget", str(ROOT / "tests/ui_scene.budget.json"), "--assets", str(build / "assets"),
                        "--content", str(build / "test-content"), "--out", str(out), "--sd-dir", str(sd),
                        "--flash-image", str(work / (name + ".img")), "--quiet", "--no-random-outages", "--trace"],
                       env=env, capture_output=True, text=True, timeout=600)
    (out / "test.log").write_text(r.stdout + r.stderr)
    data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
    m = data.get("metrics", {})
    passed = r.returncode == 0 and m.get("test.finished") == 1 and not m.get("test.fail", 0)
    counts, problems = {}, []
    if (out / "trace.json").exists():
        trace = json.loads((out / "trace.json").read_text())
        trace = trace.get("traceEvents", []) if isinstance(trace, dict) else trace
        for step, (most, needed) in STEPS.items():
            got = inside(trace, m, step)
            if got is None:
                continue
            n, outside, during, pressed, cleans = got
            counts[step] = n if not pressed and not cleans else f"{n} (+{pressed} press feedback, {cleans} full panel)"
            if n > most or (needed and not n) or outside or during or pressed > 2:
                problems.append(f"{step}: {n} refreshes (at most {most}), {len(outside)} outside {outside[:2]}, {during} while held")
    print(f"gallery-3[{name}]: exit={r.returncode} pass={m.get('test.pass', 0)} fail={m.get('test.fail', 0)} refreshes={counts}", flush=True)
    warnings = [str(w)[:300] for w in data.get("warnings", [])]
    return {"name": name, "pass": passed and not problems, "finished": m.get("test.finished"), "failed": m.get("test.fail", 0),
            "warnings": warnings, "problems": problems, "refreshes": counts, "out": out}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build", type=Path, required=True)
    p.add_argument("--phase")
    p.add_argument("--screens", action="store_true")
    p.add_argument("--reverse", action="store_true")
    a = p.parse_args()
    build = a.build.resolve()
    work = Path(tempfile.mkdtemp(prefix="gallery-3-", dir=build))
    ok = True
    if a.reverse:
        for name, (_, _, _, phase, expected) in REVERSE.items():
            (work / name).mkdir()
            binary = mutant(build, work / name, name)
            result = run(build, work / name, binary, phase, "1")
            text = " ".join(result["warnings"])
            failed = result["finished"] == 1 and result["failed"] and expected in text
            print(f"reverse {name}: {'PASS' if failed else 'FAIL'}: removing the change fails '{expected}'", flush=True)
            ok &= bool(failed)
    else:
        for phase, size in RUNS:
            if a.phase and phase != a.phase:
                continue
            result = run(build, work, build / "sim-gallery-3", phase, size)
            if not result["pass"]:
                ok = False
                print("\n".join(["  " + w for w in result["warnings"][:12]] + ["  " + q for q in result["problems"]]))
            if a.screens:
                for source in sorted(result["out"].glob("clean-*.png")):
                    target = ROOT / "docs/tasks/gallery-3" / source.name.removeprefix("clean-")
                    with Image.open(source) as im:
                        im.convert("L").save(target, optimize=True)
                    assert target.stat().st_size < 300 * 1024, target.name
            (work / f"{result['name']}-refreshes.json").write_text(json.dumps(result["refreshes"], ensure_ascii=False))
    if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
        shutil.rmtree(work)
    else:
        print("Evidence:", work.relative_to(ROOT), flush=True)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
