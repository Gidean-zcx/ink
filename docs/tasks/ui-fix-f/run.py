"""Run F presentation checks; --reverse manually links baseline code without changing the checkout."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
BASELINE = "a379ef4323b576dbda6543cf0874037f05ca1056"
CASES = {
    "remote": ("sim-remote-app", "tests/remote_app/remote_app.c", "system/apps/remote/remote.c", "tests/remote_app/remote.script"),
    "weather": ("sim-weather", "tests/weather/weather.c", "system/apps/weather/weather.c", "tests/weather/weather.script"),
    "calendar": ("sim-calendar", "tests/calendar/calendar.c", "system/apps/calendar/calendar.c", "tests/calendar/calendar.script"),
    "calculator": ("sim-calculator", "tests/ui_fix_f_calculator.c", "system/apps/calculator/calculator.c", "docs/tasks/ui-fix-f/scene.script"),
}
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
p.add_argument("--case", choices=CASES, action="append")
p.add_argument("--font", choices=("100", "120"), action="append")
p.add_argument("--reverse", action="store_true")
p.add_argument("--screenshots", type=Path)
a = p.parse_args()
build = a.build.resolve()
area = build / "ui-fix-f"
area.mkdir(exist_ok=True)

def flags(target):
    source = (build / f"CMakeFiles/{target}.dir/flags.make").read_text()
    return [v for name in ("C_DEFINES", "C_INCLUDES", "C_FLAGS")
            for v in shlex.split(re.search(r"^" + name + r" = (.*)$", source, re.M)[1])]

def compile_source(command, log):
    result = subprocess.run(command, cwd=build, text=True, capture_output=True)
    log.write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.stderr)

results = []
for mode in (("after", "before") if a.reverse else ("after",)):
    for case in a.case or CASES:
        target, driver, product, script = CASES[case]
        link = shlex.split((build / f"CMakeFiles/{target}.dir/link.txt").read_text())
        source = (ROOT / driver).read_text()
        overrides = []
        if mode == "before":
            old = subprocess.run(["git", "show", BASELINE + ":" + product], cwd=ROOT, capture_output=True, check=True).stdout
            baseline = area / (case + "-baseline.c")
            baseline.write_bytes(old)
            if case == "remote":
                source = source.replace('#include "../../system/apps/remote/remote.c"', f'#include "{baseline}"')
            elif case == "calculator":
                overrides = [f'-DINK_TEST_CALCULATOR="{baseline}"']
            else:
                obj = area / (case + "-baseline.o")
                compile_source([link[0], *flags("ink_system"), "-iquote", str((ROOT / product).parent), "-c", str(baseline), "-o", str(obj)], area / (case + "-baseline-compile.log"))
                link.insert(next(i for i, word in enumerate(link) if word.endswith(".a")), str(obj))
        driver_copy = area / (case + "-" + mode + ".c")
        driver_copy.write_text(source)
        obj, binary = area / (case + "-" + mode + ".o"), area / (case + "-" + mode)
        compile_source([link[0], *flags(target), *overrides, "-iquote", str((ROOT / driver).parent), "-c", str(driver_copy), "-o", str(obj)], area / (case + "-" + mode + "-compile.log"))
        original_driver = "tests/calculator/calculator.c" if case == "calculator" else driver
        link = [str(obj) if v.endswith(original_driver + ".o") else v for v in link]
        link[link.index("-o") + 1] = str(binary)
        compile_source(link, area / (case + "-" + mode + "-link.log"))
        for font in a.font or ("100", "120"):
            out = area / f"{case}-{mode}-{font}"
            out.mkdir(exist_ok=True)
            flash = out / "user.img"
            flash.unlink(missing_ok=True)
            command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"), "--script", str(ROOT / script),
                       "--budget", str(ROOT / "tests/ui_scene.budget.json"), "--assets", str(build / "assets"),
                       "--content", str(build / "test-content"), "--out", str(out), "--flash-image", str(flash),
                       "--sd-dir", str(out / "sd"), "--quiet", "--no-random-outages"]
            result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180,
                                    env={**os.environ, "INK_UI_FIX_FONT": font, "INK_WEATHER_PHASE": "main", "INK_CALENDAR_SCENE": "Form", "UBSAN_OPTIONS": "halt_on_error=1"})
            (out / "run.log").write_text(result.stdout + result.stderr)
            data = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
            counts = data.get("metrics", {})
            failures = counts.get("test.fail", 0)
            accepted = counts.get("test.finished") == 1 and ((result.returncode == 0 and not failures)
                        if mode == "after" else failures > 0 and any("UX" in str(w) for w in data.get("warnings", [])))
            row = {"case": case, "mode": mode, "font": font, "exit": result.returncode, "pass": counts.get("test.pass", 0), "fail": failures, "accepted": accepted}
            results.append(row)
            print(json.dumps(row), flush=True)
            if not accepted:
                raise SystemExit(result.stderr[-3000:] + "\n" + "\n".join(str(w) for w in data.get("warnings", [])))
            if a.screenshots:
                a.screenshots.mkdir(parents=True, exist_ok=True)
                for frame in out.glob("snap-*.png"):
                    dest = a.screenshots / f"{case}-{font}-{mode}-{frame.stem[5:]}.png"
                    Image.open(frame).convert("L").save(dest, optimize=True)
                    assert dest.stat().st_size <= 300 * 1024, dest.name
(area / "summary.json").write_text(json.dumps(results, ensure_ascii=False, indent=2) + "\n")
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(area)
