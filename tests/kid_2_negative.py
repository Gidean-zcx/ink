"""Compile isolated mutants without editing the worktree, and require decisive failures."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--case", choices=("parent", "locked", "volume", "entry", "settings", "child-notice", "settings-notice", "no-pin-layout", "confirm-layout"), required=True)
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix=f"kid-2-negative-{a.case}-", dir=build))
case = {
    "parent": ("system/policy/members.c", ".id = \"member.time_set\"", "limit", "accepting without the parent's password is refused"),
    "locked": ("system/policy/members.c", "M.counting = M.timed && ink_power_state() != INK_PWR_LOCKED;", "limit", "locked time is not charged"),
    "volume": ("system/policy/settings.c", "", "control", "settings and actual audio both enforce child_max"),
    "entry": ("system/ui/app_registry.c", "if (!ink_app_visible(a->def)) { ink_shell_kid_limit(id); return -2; }", "blocked", "an AI link to a forbidden app is denied"),
    "settings": ("system/ui/shell/settings_family.c", "", "settings", "SetKid shows the real daily policy without a timeslot placeholder"),
    "child-notice": ("system/ui/shell/member_pages.c", "ink_member_any_pin() ? why : INK_PARENT_PIN_MISSING", "no-pin", "missing parent PIN is explained on the visible page"),
    "settings-notice": ("system/ui/shell/confirm_card.c", '    if (!strncmp(C.now.cap, "member.time_", 12) && !ink_member_any_pin()) return INK_PARENT_PIN_MISSING;\n', "no-pin", "missing parent PIN is explained on the visible page"),
    "no-pin-layout": ("system/ui/shell/member_pages.c", "P.mode = ink_member_any_pin() ? PARENT : FORGOT;", "no-pin", "missing PIN is explained without an unusable password keypad"),
    "confirm-layout": ("system/ui/shell/confirm_card.c", 'static int pager_y(void) { return notice_y() + ink_ui_text_height(notice_text(), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_W - 2 * INK_MARGIN) + INK_GAP_S; }', "no-pin", "the complete no-PIN explanation fits above the page buttons"),
}[a.case]
relative, needle, phase, expected = case
source = (ROOT / relative).read_text()
if a.case == "settings":
    changed = subprocess.run(["git", "show", f"1d5312ec:{relative}"], cwd=ROOT, check=True, capture_output=True, text=True).stdout
    assert changed != source
elif a.case in ("child-notice", "settings-notice", "no-pin-layout", "confirm-layout"):
    assert source.count(needle) == 1
    changed = source.replace(needle, {
        "child-notice": "why", "settings-notice": "", "no-pin-layout": "P.mode = PARENT;",
        "confirm-layout": "static int pager_y(void) { return notice_y() + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S; }",
    }[a.case])
elif a.case == "parent":
    line = next(line for line in source.splitlines() if needle in line)
    assert line.count(".parent_pin = true") == 1
    changed = source.replace(line, line.replace(".parent_pin = true", ".parent_pin = false"))
elif a.case == "volume":
    needle = '    if (ink_member_current()->role == INK_ROLE_CHILD && !strcmp(id, "audio.volume") && v > ink_setting_get("audio.child_max"))\n        v = ink_setting_get("audio.child_max");\n'
    assert source.count(needle) == 1
    changed = source.replace(needle, "")
else:
    assert source.count(needle) == 1
    changed = source.replace(needle, "M.counting = M.timed;" if a.case == "locked" else "/* Mutant: bypass child navigation policy. */")
mutant = work / "mutant.c"
mutant.write_text(changed)
flags = dict(line.split(" = ", 1) for line in (build / "CMakeFiles/ink_system.dir/flags.make").read_text().splitlines() if " = " in line)
link = shlex.split((build / "CMakeFiles/sim-kid-2.dir/link.txt").read_text())
obj = work / "mutant.o"
subprocess.run([link[0], *shlex.split(flags["C_DEFINES"]), *shlex.split(flags["C_INCLUDES"]),
                *shlex.split(flags["C_FLAGS"]), "-I", str(ROOT / Path(relative).parent), "-c", str(mutant), "-o", str(obj)], check=True)
binary = work / "sim-mutant"
link[link.index("-o") + 1] = str(binary)
link.insert(link.index("libink_apps.a"), str(obj))
subprocess.run(link, cwd=build, check=True)
out = work / "run"
result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
    "--script", str(ROOT / ("tests/kid_2_settings.script" if phase == "settings" else "tests/kid_2.script")), "--wall-start", "1794268800", "--assets", str(build / "assets"),
    "--content", str(build / "test-content"), "--out", str(out), "--quiet", "--no-random-outages", "--budget", str(ROOT / "tests/ui_scene.budget.json")],
    env={**os.environ, "INK_KID2_PHASE": phase}, capture_output=True, text=True, timeout=150)
(work / "run.log").write_text(result.stdout + result.stderr)
data = json.loads((out / "metrics.json").read_text())
failures = data.get("metrics", {}).get("test.fail", 0)
matched = any(expected in warning for warning in data.get("warnings", []))
ok = result.returncode != 0 and failures > 0 and matched
print(a.case, "PASS (mutant rejected)" if ok else "FAIL", "exit", result.returncode, "assertions", failures, "targeted_failure", matched)
if not ok or os.environ.get("INK_KEEP_EVIDENCE"):
    print("Evidence:", work.relative_to(ROOT))
else:
    shutil.rmtree(work)
raise SystemExit(0 if ok else 1)
