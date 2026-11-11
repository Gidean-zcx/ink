"""Manual history-based contrast: rebuild only each page fixture against the supplied baseline."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--baseline", default="da3296b75fda740f680dc5639d835d4e06d9a33b")
parser.add_argument("--evidence", type=Path)
a = parser.parse_args()
build = a.build.resolve()
area = build / "ui-fix-a-reverse"
area.mkdir(exist_ok=True)
database = json.loads((build / "compile_commands.json").read_text())
pages = {"backup": "backup_pages.c", "confirm": "confirm_card.c", "transfer": "transfer_pages.c", "factory": "factory_pages.c"}
for case, page in pages.items():
    source = ROOT / "system/ui/shell" / page
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    baseline = area / page
    baseline.write_bytes(subprocess.check_output(["git", "show", f"{a.baseline}:system/ui/shell/{page}"], cwd=ROOT))
    target = "sim-ui-fix-a-" + case
    driver = ROOT / "tests/ui_fix_a" / (case + ".c")
    entry = next(e for e in database if Path(e["file"]) == driver)
    compile_args = shlex.split(entry["command"])
    obj = area / (case + ".o")
    compile_args[compile_args.index("-o") + 1] = str(obj)
    compile_args += [f'-DINK_UI_FIX_SOURCE="{baseline}"', "-iquote", str(source.parent)]
    result = subprocess.run(compile_args, cwd=entry["directory"], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    link = shlex.split((build / f"CMakeFiles/{target}.dir/link.txt").read_text())
    link = [str(obj) if s.endswith(f"tests/ui_fix_a/{case}.c.o") else s for s in link]
    link = ["CMakeFiles/sim-backup-pages.dir/simulator/runtime/sim_main.c.o"
            if s.endswith("simulator/runtime/sim_main.c.o") and not (build / s).exists() else s for s in link]
    binary = area / target
    link[link.index("-o") + 1] = str(binary)
    result = subprocess.run(link, cwd=build, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    command = [sys.executable, str(ROOT / "tests/ui_fix_a/run.py"), "--build", str(build),
        "--case", case, "--binary", str(binary), "--expect-failure"]
    if a.evidence:
        command += ["--evidence", str(a.evidence)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
    print(result.stdout, end="")
    assert result.returncode == 0, result.stdout + result.stderr
    assert hashlib.sha256(source.read_bytes()).hexdigest() == before, "contrast must not edit product sources"
