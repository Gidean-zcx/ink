"""Run SMS bubble fixtures and the isolated one-message-per-page negative control."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--reverse", action="store_true")
parser.add_argument("--reverse-refresh", action="store_true")
parser.add_argument("--screenshots", action="store_true")
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
build = args.build.resolve()
control = args.reverse or args.reverse_refresh
output = build / "test-out" / ("sms-thread-2-reverse-refresh" if args.reverse_refresh else "sms-thread-2-reverse" if args.reverse else "sms-thread-2")
output.mkdir(parents=True, exist_ok=True)
binary = build / "sim-sms"
if control:
    flags = {}
    for line in (build / "CMakeFiles/sim-sms.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-sms.dir/link.txt").read_text())
    app = (root / "system/apps/sms/sms.c").read_text()
    if args.reverse:
        assert app.count("n == SMS_VISIBLE") == 1
        app = app.replace("n == SMS_VISIBLE", "n == 1")
    else:
        before = 'else { lv_obj_delete(S.field); reply_field(lv_screen_active()); parts_note(NULL); lv_label_set_text(S.notice, ""); refresh_thread(); }'
        assert app.count(before) == 1
        app = app.replace(before, "else ink_ui_redraw();")
    source = (root / "tests/sms/sms.c").read_text().replace('#include "../../system/apps/sms/sms.c"', app)
    obj = output / "one-message.o"
    subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                    "-iquote", str(root / "tests/sms"), "-x", "c", "-c", "-", "-o", str(obj)],
                   input=source, text=True, check=True, cwd=build)
    binary = output / "sim-one-message"
    link = [str(obj) if part.endswith("tests/sms/sms.c.o") else part for part in link]
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)

phases = ("thread-1",) if args.reverse_refresh else ("thread-5",) if args.reverse else tuple(f"thread-{n}{suffix}" for n in (1, 5, 20) for suffix in ("", "-large"))
failed = False
for phase in phases:
    out = output / phase
    out.mkdir(exist_ok=True)
    image = out / "user.img"
    if image.exists():
        image.unlink()
    command = [str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
               "--script", str(root / "tests/sms/sms.script"), "--budget", str(root / "tests/ui_scene.budget.json"),
               "--assets", str(build / "assets"), "--content", str(build / "test-content"),
               "--flash-image", str(image), "--sd-dir", str(out / "sd"), "--out", str(out),
               "--quiet", "--no-random-outages"]
    result = subprocess.run(command, env=dict(os.environ, INK_SMS_PHASE=phase), capture_output=True, text=True)
    (out / "process.log").write_text(result.stdout + result.stderr)
    if not (out / "metrics.json").exists():
        raise SystemExit(f"{phase}: exit={result.returncode}, no metrics\n{result.stderr[-3000:]}")
    metrics = json.loads((out / "metrics.json").read_text())
    counts = metrics.get("metrics", {})
    print(f'{phase}: exit={result.returncode} pass={counts.get("test.pass", 0)} fail={counts.get("test.fail", 0)} finished={counts.get("test.finished", 0)}', flush=True)
    for warning in metrics.get("warnings", []):
        print(warning)
    if control:
        assert result.returncode == 1 and counts.get("test.fail", 0) > 0 and counts.get("test.finished") == 1
        expected = "inline send keeps its page" if args.reverse_refresh else "one page holds multiple actual bubbles"
        assert expected in str(metrics)
        continue
    passed = result.returncode == 0 and not counts.get("test.fail", 0) and counts.get("test.finished") == 1
    failed |= not passed
    if not passed:
        print(result.stderr[-4000:])
    for page in out.glob("thread-*.json"):
        snapshot = json.loads(page.read_text())
        messages = snapshot["messages"]
        fields = messages["fields"]
        assert len(fields) == messages["pagination"]["visible"]
        assert all(f["role"] in ("incoming", "outgoing") and f["value"] and " · " in f["label"] for f in fields)
    print("refresh:", (out / "refresh.txt").read_text().strip(), flush=True)
    if args.screenshots:
        from PIL import Image
        for page in out.glob("thread-*.json"):
            picture = out / f"clean-{page.stem}.png"
            if not picture.exists():
                picture = out / f"snap-{page.stem}.png"
            target = root / "docs/tasks/sms-thread-2" / f"{page.stem}.png"
            with Image.open(picture) as im:
                im.convert("L").save(target, optimize=True)
            assert target.stat().st_size <= 300_000

raise SystemExit(1 if failed else 0)
