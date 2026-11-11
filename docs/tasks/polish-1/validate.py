"""Manual screenshots and baseline controls; regular ctest never needs Git history."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DEST = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--baseline", action="store_true")
parser.add_argument("--source-control", action="store_true", help="restore touch attribution to check the confirmation regression")
parser.add_argument("--layout-probe", action="store_true", help="check a real short field after leaving password input")
args = parser.parse_args()
if args.source_control and (args.baseline or args.layout_probe):
    parser.error("choose one reverse control")
build = args.build.resolve()
stage = "source-control" if args.source_control else "before" if args.baseline else "after"
if args.layout_probe:
    stage = "layout-" + stage
area = build / "test-out" / f"polish-1-{stage}"
area.mkdir(parents=True, exist_ok=True)
baseline = "2587faa2fca29425bf913127c3e6914daa576e8e"
entries = json.loads((build / "compile_commands.json").read_text())
checks = {
    "ime-keyboard1": ("tests/ime_keyboard1.c", "system/ui/shell/keyboard.c", "../system/ui/shell/keyboard.c",
                      {"WifiPassword-normal": "Password-100", "WifiPassword-large": "Password-120"},
                      ["password keyboard removes", "password hides candidates", "password symbols retain"]),
    "sms": ("tests/sms/sms.c", "system/apps/sms/sms.c", "../../system/apps/sms/sms.c",
            {"SmsThread-normal": "SmsThread-100", "SmsThread-large": "SmsThread-120"},
            ["local date label", "adjacent gap", "SMS keyboard enter key visibly says Send", "keyboard enter sends exactly once"]),
}
summaries = []
for target, (test, product, include, shots, expected) in checks.items():
    if args.source_control and target != "sms":
        continue
    if args.layout_probe and target != "ime-keyboard1":
        continue
    case = area / target
    case.mkdir(parents=True, exist_ok=True)
    binary = build / f"sim-{target}"
    if args.baseline or args.source_control or args.layout_probe:
        source = (ROOT / test).read_text()
        if args.layout_probe:
            before = "    expect_mode(true);\n    ink_text_blur(0);\n}\n\nstatic bool contains"
            probe = '''    expect_mode(true);
    ink_text_blur(0);
    ink_text_cfg_t secret = {.type = INK_TEXT_PASSWORD, .max = 32};
    ink_text_focus(&secret, NULL, NULL); ink_text_blur(0);
    ink_text_cfg_t ordinary = {.type = INK_TEXT_PLAIN, .max = 32};
    lv_obj_t *field = ink_textfield_create(lv_screen_active(), &ordinary, NULL, NULL, false);
    ink_textfield_style(field, INK_FIELD_SINGLE, "合成短输入");
    lv_obj_set_pos(field, ink_ui_px(INK_MARGIN), ink_ui_height() - ink_ui_px(INK_MARGIN + ink_textfield_height(INK_FIELD_SINGLE)));
    CHECK(ink_textfield_short_focus(field) == 0, "focus ordinary short field after password");
    lv_area_t bounds, keyboard; lv_obj_update_layout(field); lv_obj_get_coords(field, &bounds); ink_shell_keyboard_area(&keyboard);
    CHECK(bounds.y2 < keyboard.y1, "ordinary short field must fit after password: bottom=%d keyboard=%d", (int)bounds.y2, (int)keyboard.y1);
    lv_obj_delete(field);
}

static bool contains'''
            assert source.count(before) == 1
            source = '#include "ink/sys/textfield.h"\n' + source.replace(before, probe)
        if args.baseline:
            original = subprocess.check_output(["git", "show", f"{baseline}:{product}"], cwd=ROOT, text=True)
            source = source.replace(f'#include "{include}"', original)
            if target == "ime-keyboard1":
                # Only the current SMS object needs this symbol; keyboard tests never send SMS.
                source += "\nink_source_t ink_shell_keyboard_enter_source(void) { return INK_SRC_AI; }\n"
        entry = next(e for e in entries if Path(e["file"]) == ROOT / test)
        command = shlex.split(entry["command"])
        obj = case / "baseline.o"
        command[command.index("-o") + 1] = str(obj)
        command[command.index(str(ROOT / test))] = "-"
        command += ["-iquote", str((ROOT / test).parent), "-x", "c"]
        # -x must precede stdin for both clang and GCC.
        command.remove("-")
        command.append("-")
        subprocess.run(command, input=source, cwd=entry["directory"], text=True, check=True, capture_output=True)
        link = shlex.split((build / f"CMakeFiles/sim-{target}.dir/link.txt").read_text())
        link = [str(obj) if part.endswith(test + ".o") else part for part in link]
        if args.source_control:
            keyboard = ROOT / "system/ui/shell/keyboard.c"
            original = keyboard.read_text()
            before = "return enter_call ? enter_call->src : INK_SRC_AI;"
            assert original.count(before) == 1
            mutant = case / "keyboard.c"
            mutant.write_text(original.replace(before, "return INK_SRC_TOUCH;"))
            entry = next(e for e in entries if Path(e["file"]) == keyboard)
            command = shlex.split(entry["command"])
            obj = case / "keyboard.o"
            command[command.index("-o") + 1] = str(obj)
            command[command.index(str(keyboard))] = str(mutant)
            subprocess.run(command, cwd=entry["directory"], check=True, capture_output=True)
            link.insert(1, str(obj))
        binary = case / "sim-baseline"
        link[link.index("-o") + 1] = str(binary)
        subprocess.run(link, cwd=build, check=True, capture_output=True)
    out = case / "run"
    shutil.rmtree(out, ignore_errors=True)
    state = case / "state"
    shutil.rmtree(state, ignore_errors=True)
    (state / "sd").mkdir(parents=True)
    script = ROOT / ("tests/ime_keyboard1.script" if target == "ime-keyboard1" else "tests/sms/sms.script")
    command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
               "--script", str(script), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
               "--assets", str(build / "assets"), "--content", str(build / "test-content"),
               "--flash-image", str(state / "user.img"), "--sd-dir", str(state / "sd"),
               "--out", str(out), "--quiet", "--no-random-outages"]
    result = subprocess.run(command, env=dict(os.environ, INK_SMS_PHASE="polish"),
                            cwd=ROOT, capture_output=True, text=True, timeout=150)
    (case / "process.log").write_text(result.stdout + result.stderr)
    data = json.loads((out / "metrics.json").read_text())
    metrics = data["metrics"]
    warnings = "\n".join(data.get("warnings", []))
    passed = metrics.get("test.finished") == 1
    if args.layout_probe and not args.baseline:
        passed = passed and result.returncode == 1 and metrics.get("test.fail", 0) == 1 and "ordinary short field must fit after password" in warnings
    elif args.source_control:
        passed = passed and result.returncode == 1 and metrics.get("test.fail", 0) > 0 and "AI enter keeps external-send confirmation" in warnings
    elif args.baseline:
        passed = passed and result.returncode == 1 and metrics.get("test.fail", 0) > 0 and all(s in warnings for s in expected)
        if args.layout_probe:
            passed = passed and "ordinary short field must fit after password" not in warnings
    else:
        passed = passed and result.returncode == 0 and metrics.get("test.fail", 0) == 0 and not warnings
    summary = {"stage": stage, "target": target, "exit": result.returncode,
               "checks": metrics.get("test.pass", 0), "failures": metrics.get("test.fail", 0), "accepted": passed}
    if args.layout_probe:
        summary["layout_failed"] = "ordinary short field must fit after password" in warnings
    summaries.append(summary)
    print(json.dumps(summary), flush=True)
    if args.layout_probe:
        for warning in data.get("warnings", []):
            if "ordinary short field must fit after password" in warning:
                print(warning, flush=True)
    if not passed:
        print(warnings, result.stderr[-3000:])
        raise SystemExit(1)
    if args.source_control or args.layout_probe:
        continue
    (DEST / "screenshots").mkdir(exist_ok=True)
    for source, name in shots.items():
        picture = out / f"snap-{source}.png"
        if picture.stat().st_size > 300_000:
            raise SystemExit(f"screenshot exceeds 300 KB: {picture.name}")
        shutil.copyfile(picture, DEST / "screenshots" / f"{stage}-{name}.png")
(area / "summary.json").write_text(json.dumps(summaries, indent=2) + "\n")
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(area)
