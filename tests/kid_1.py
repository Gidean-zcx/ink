"""Exercise local member forms and reboots, retaining evidence only on request."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--build", required=True, type=Path)
parser.add_argument("--mutant", choices=("unlock", "set", "parent", "privacy", "locked"))
args = parser.parse_args()
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="kid-1-", dir=build))
binary = build / "sim-kid-1"
if args.mutant:
    source = (ROOT / "system/ui/shell/member_pages.c").read_text()
    old, new = {
        "unlock": ("ink_member_unlock(P.id, P.pin, completed, NULL)", "ink_member_unlock(P.id, \"\", completed, NULL)"),
        "set": ("ink_member_set_pin(P.id, P.old, P.pin, completed, NULL)", "ink_member_set_pin(P.id, P.old, \"\", completed, NULL)"),
        "parent": ("ink_member_check_parent(P.pin, completed, NULL)", "ink_member_unlock(P.id, P.pin, completed, NULL)"),
        "privacy": ("P.overlay->name, P.name, P.status", "P.overlay->name, P.pin, P.status"),
        "locked": ("ink_ui_pill(P.card.panel, text, x, y, w, INK_CONTROL, primary, clicked, (void *)cap)",
                   "ink_shell_bind(ink_ui_pill(P.card.panel, text, x, y, w, INK_CONTROL, primary, NULL, NULL), cap, NULL)"),
    }[args.mutant]
    assert source.count(old) == 1
    (work / "member-mutant.c").write_text(source.replace(old, new))
    test = (ROOT / "tests/kid_1.c").read_text().replace(
        '#include "../system/ui/shell/member_pages.c"', '#include "member-mutant.c"').replace(
        '#include "configured_boot.c"', f'#include "{ROOT}/tests/configured_boot.c"').replace(
        '#include "integration/whole_product.h"', f'#include "{ROOT}/tests/integration/whole_product.h"')
    driver = work / "test.c"
    driver.write_text(test)
    import shlex
    flags = dict(line.split(" = ", 1) for line in (build / "CMakeFiles/sim-kid-1.dir/flags.make").read_text().splitlines() if " = " in line)
    obj = work / "test.o"
    subprocess.run(["cc", *shlex.split(flags["C_DEFINES"]), *shlex.split(flags["C_INCLUDES"]),
                    "-I", str(ROOT / "system/ui/shell"), *shlex.split(flags["C_FLAGS"]), "-c", str(driver), "-o", str(obj)], check=True)
    link = shlex.split((build / "CMakeFiles/sim-kid-1.dir/link.txt").read_text())
    link[link.index("CMakeFiles/sim-kid-1.dir/tests/kid_1.c.o")] = str(obj)
    binary = work / "sim-mutant"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)

ok = True
for phase, scene in (("preview", "MemberSwitch"), ("write", "PinSet"), ("verify", ""), ("wait", "PinWait"), ("recover", "PinForgot")):
    out = work / phase
    result = subprocess.run([str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
        "--script", str(ROOT / "tests/kid_1.script"), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
        "--assets", str(build / "assets"), "--content", str(build / "test-content"), "--flash-image", str(work / "user.img"),
        "--sd-dir", str(work / "sd"), "--out", str(out), "--quiet", "--no-random-outages"],
        env={**os.environ, "INK_KID_PHASE": phase, "INK_KID_SCENE": scene}, capture_output=True, text=True, timeout=150)
    (work / f"{phase}.log").write_text(result.stdout + result.stderr)
    metrics = json.loads((out / "metrics.json").read_text()) if (out / "metrics.json").exists() else {}
    counts = metrics.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
    print(phase, "PASS" if passed else "FAIL", "exit", result.returncode, "checks", counts.get("test.pass", 0), "failures", counts.get("test.fail", 0), flush=True)
    ok &= passed
    if not passed:
        print(metrics.get("warnings", []), result.stderr[-1200:])
        break
    for path in out.rglob("*"):
        if path.suffix in (".txt", ".json", ".ndjson", ".log"):
            data = path.read_text(errors="replace")
            assert not any(pin in data for pin in ("24681357", "13572468", "90817263")), (phase, path.name, "PIN leak")
    assert not any(pin in result.stdout + result.stderr for pin in ("24681357", "13572468", "90817263")), "PIN in process log"
    if scene:
        assert counts.get("test.snapshot.settled") == counts.get(f"test.snapshot.{scene}") == 1
        if not args.mutant:
            target = build / "kid-1-compare" / scene
            target.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(out / "snap-scene.png", target / "snap-scene.png")
            shutil.copyfile(out / "metrics.json", target / "metrics.json")
if ok and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT), flush=True)
raise SystemExit(0 if ok else 1)
