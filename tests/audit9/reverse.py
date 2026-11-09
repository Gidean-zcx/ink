"""Manual historical controls; not registered in CTest (CI may be shallow)."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--baseline", required=True)
args = parser.parse_args()
build = args.build.resolve()
area = build / "audit9/reverse"
area.mkdir(parents=True, exist_ok=True)
commands = []


def run(command, **kwargs):
    commands.append([str(item) for item in command])
    return subprocess.run(command, cwd=build, capture_output=True, text=True, timeout=180, **kwargs)


def original(path):
    result = subprocess.run(["git", "show", args.baseline + ":" + path], cwd=ROOT,
                            capture_output=True, text=True, check=True)
    return result.stdout


compilation = json.loads((build / "compile_commands.json").read_text())
summary = []
for case, file, expected in (("bt-ai", "bluetooth_page.c", "snapshot matches the Bluetooth switch"),
                             ("ime-layout", "keyboard.c", "more is fully inside the screen")):
    path = "system/ui/shell/" + file
    source = area / file
    source.write_text(original(path))
    row = next(item for item in compilation if Path(item["file"]) == ROOT / path)
    command = shlex.split(row["command"])
    obj = area / (case + ".o")
    command[command.index("-o") + 1] = str(obj)
    command[command.index(str(ROOT / path))] = str(source)
    command += ["-iquote", str((ROOT / path).parent)]
    result = run(command)
    assert result.returncode == 0, result.stderr
    link = shlex.split((build / f"CMakeFiles/sim-audit9-{case}.dir/link.txt").read_text())
    binary = area / case
    link[link.index("-o") + 1] = str(binary)
    link.insert(next(i for i, part in enumerate(link) if part.endswith("libink_system.a")), str(obj))
    result = run(link)
    assert result.returncode == 0, result.stderr
    result = run([sys.executable, str(ROOT / "tests/audit9/run.py"), "--build", str(build),
                  "--case", case, "--binary", str(binary)],
                 env={**os.environ, "INK_KEEP_EVIDENCE": "1"})
    (area / (case + ".log")).write_text(result.stdout + result.stderr)
    metrics = json.loads((build / f"test-out/audit9-{case}/metrics.json").read_text())
    assert result.returncode != 0 and metrics["metrics"].get("test.finished") == 1
    assert metrics["metrics"].get("test.fail", 0) > 0 and expected in str(metrics["warnings"])
    summary.append({"case": case, "exit": result.returncode, "failures": metrics["metrics"]["test.fail"]})

source = original("tools/imedict/build_base.py")
source = source.replace("ROOT = Path(__file__).resolve().parent", f"ROOT = Path({str(ROOT / 'tools/imedict')!r})")
script = area / "build_base.py"
script.write_text(source)
dictionary = area / "unfiltered.ikd"
result = run([sys.executable, str(script), "--cache", str(ROOT / "build-deps/imedict"), "--out", str(dictionary)],
             env={**os.environ, "PYTHONPATH": str(ROOT / "tools/imedict")})
assert result.returncode == 0, result.stderr
result = run([sys.executable, str(ROOT / "tests/audit9/font_coverage.py"),
              "--build", str(build), "--dictionary", str(dictionary)])
(area / "font-coverage.log").write_text(result.stdout + result.stderr)
assert result.returncode != 0 and "missing glyphs" in result.stderr
summary.append({"case": "font-coverage", "exit": result.returncode, **json.loads(result.stdout)})
(area / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
(area / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
print(json.dumps(summary, ensure_ascii=False, indent=2))
