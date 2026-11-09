"""Compile isolated regressions against current tests, without editing product sources."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
flags = {}
for line in (build / "CMakeFiles/sim-sms.dir/flags.make").read_text().splitlines():
    if " = " in line:
        key, value = line.split(" = ", 1)
        flags[key] = shlex.split(value)
link = shlex.split((build / "CMakeFiles/sim-sms.dir/link.txt").read_text())
compile_flags = [link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"]]
app = (root / "system/apps/sms/sms.c").read_text()
comm = (root / "system/services/comm/comm_service.c").read_text()
args = (root / "system/util/ink_args.c").read_text()
test = (root / "tests/sms/sms.c").read_text()
old = subprocess.check_output(["git", "show", "086c5059:system/services/comm/comm_service.c"], cwd=root, text=True)
old_send = old[old.index("static void cap_send("):old.index("static void cap_list(")]
send_start, send_end = comm.index("static void cap_send("), comm.index("static void cap_list(")
variants = [
    ("01-list", app, comm[:send_end] + comm[send_end:].replace("ink_cap_call_member(c)", '"owner"', 1), None, ["legacy"]),
    ("01-send", app, comm[:send_start] + old_send + comm[send_end:], None, ["legacy-send"]),
    ("02-rx", app, comm.replace("char text[355];", "char text[200];", 1), None, ["long-rx"]),
    ("02-metadata", app, None, args.replace("> INK_ARGS_TEXT", "> 384", 1), ["long-rx"]),
    ("03-pages", app.replace("size_t end = start, len = strlen(text);", "return strlen(text); size_t end = start, len = strlen(text);", 1), None, None, ["multiline"]),
    ("04-cold", app.replace("static void navigate(ink_call_t *c) {\n    member_ready();", "static void navigate(ink_call_t *c) {", 1), None, None, ["cold-thread"]),
    ("05-notice", app.replace("INK_CANVAS_H - INK_MARGIN - INK_PRIMARY - INK_CONTROL - 2 * INK_GAP_L - INK_LINE_NOTE", "INK_CANVAS_H - INK_PRIMARY - INK_CONTROL - INK_GAP_L", 1), None, None, ["receipt-ok", "receipt-fail", "receipt-unknown"]),
    ("06-draft", app.replace(".draft_id = !strcmp(S.to, S.draft_ref.to) ? S.draft_ref.id : 0", ".draft_id = 0", 1), None, None, ["draft-write", "draft-read"]),
]

def compile_source(source, target):
    subprocess.run([*compile_flags, "-iquote", str(root / "tests/sms"), "-x", "c", "-c", "-", "-o", str(target)],
                   input=source, text=True, check=True, cwd=build)

for name, app_source, comm_source, args_source, phases in variants:
    target = output / name
    target.mkdir(exist_ok=True)
    obj, binary = target / "test.o", target / "sim-sms-negative"
    compile_source(test.replace('#include "../../system/apps/sms/sms.c"', app_source), obj)
    command = [str(obj) if part.endswith("tests/sms/sms.c.o") else part for part in link]
    command[command.index("-o") + 1] = str(binary)
    for source, stem in ((comm_source, "comm"), (args_source, "args")):
        if source is not None:
            extra = target / (stem + ".o")
            compile_source(source, extra)
            command.insert(1, str(extra))
    subprocess.run(command, cwd=build, check=True, capture_output=True, text=True)
    image = target / "user.img"
    if image.exists(): image.unlink()
    for phase in phases:
        out = target / phase
        phase_image = image if phase.startswith("draft-") else target / (phase + ".img")
        if not phase.startswith("draft-") and phase_image.exists(): phase_image.unlink()
        command = [str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
                   "--script", str(root / "tests/sms/sms.script"), "--budget", str(root / "tests/ui_scene.budget.json"),
                   "--assets", str(build / "assets"), "--content", str(build / "test-content"),
                   "--flash-image", str(phase_image),
                   "--sd-dir", str(target / "sd"), "--out", str(out), "--quiet", "--no-random-outages"]
        result = subprocess.run(command, env=dict(os.environ, INK_SMS_PHASE=phase), capture_output=True, text=True)
        (target / (phase + ".log")).write_text(result.stdout + result.stderr)
        metrics = json.loads((out / "metrics.json").read_text())
        counters = metrics["metrics"]
        failures = counters.get("test.fail", 0)
        print(name, phase, "exit", result.returncode, "finished", counters.get("test.finished"), "failures", failures, flush=True)
        if phase == "draft-write":
            assert result.returncode == 0 and not failures and counters.get("test.finished") == 1
        else:
            assert result.returncode == 1 and failures > 0 and counters.get("test.finished") == 1
        for warning in metrics.get("warnings", []): print(warning, flush=True)
