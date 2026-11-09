"""Exercise lifetime contracts; optional mutants remove one fix at a time."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--mutant", choices=("page", "member", "identity", "close", "state", "settings", "event"))
args = parser.parse_args()
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="fw-gaps-", dir=build))
binary = build / "sim-fw-gaps"
if args.mutant:
    file, before, after = {
        "page": ("system/ui/app_registry.c", "if (p->page && p->owner.epoch", "if (p->owner.epoch"),
        "member": ("system/ui/app_registry.c", "p->owner.epoch == ink_member_epoch()", "true"),
        "identity": ("system/ui/app_registry.c", "ink_db_open(p->def, p->owner.member)", "ink_db_open(p->def, NULL)"),
        "close": ("system/ui/app_registry.c", "else ink_db_close(p->db);", "else (void)p->db;"),
        "state": ("system/policy/state.c", "return watch_remove(&ST.watch, handle);", "(void)handle; return 0;"),
        "settings": ("system/policy/settings.c", "return watch_remove(&SE.watch, handle);", "(void)handle; return 0;"),
        "event": ("system/policy/cap.c", "live = SUBS[e->sub].handle == e->handle && SUBS[e->sub].fn;", "live = true;"),
    }[args.mutant]
    source = (ROOT / file).read_text()
    assert source.count(before) == 1
    target = work / Path(file).name
    target.write_text(source.replace(before, after))
    flags = {}
    for line in (build / "CMakeFiles/ink_system.dir/flags.make").read_text().splitlines():
        if " = " in line:
            key, value = line.split(" = ", 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / "CMakeFiles/sim-fw-gaps.dir/link.txt").read_text())
    obj = work / "mutant.o"
    subprocess.run([link[0], *flags["C_DEFINES"], *flags["C_INCLUDES"], *flags["C_FLAGS"],
                    "-I" + str((ROOT / file).parent), "-c", str(target), "-o", str(obj)], cwd=build, check=True)
    link.insert(1, str(obj))
    binary = work / "sim-fw-gaps"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)

command = [str(binary), "--profile", str(ROOT / "simulator/profiles/metalio-plus.json"),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"),
           "--script", str(ROOT / "tests/jot/app.script"), "--budget", str(ROOT / "tests/ui_scene.budget.json"),
           "--out", str(work / "out"), "--quiet", "--no-random-outages"]
result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=150,
                        env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"})
(work / "run.log").write_text(result.stdout + result.stderr)
data = json.loads((work / "out/metrics.json").read_text()) if (work / "out/metrics.json").exists() else {}
counts = data.get("metrics", {})
passed = result.returncode == 0 and counts.get("test.finished") == 1 and not counts.get("test.fail", 0)
print("PASS" if passed else "FAIL", "exit", result.returncode, "checks", counts.get("test.pass", 0),
      "failures", counts.get("test.fail", 0), flush=True)
if not passed:
    print(data.get("warnings", []))
    print(result.stderr[-4000:])
if passed and not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(work)
else:
    print("Evidence:", work.relative_to(ROOT))
raise SystemExit(0 if passed else 1)
