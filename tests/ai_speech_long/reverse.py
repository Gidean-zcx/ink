"""Manual reverse checks only: requires history; never registered in CTest."""
import argparse
import json
import os
import shlex
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--baseline", required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
build = args.build.resolve()
work = Path(tempfile.mkdtemp(prefix="ai-speech-reverse-", dir=build))
db = {Path(e["file"]).resolve(): e for e in json.loads((build / "compile_commands.json").read_text())}
for name, path in [("fixed-wait", "system/services/ai/ai_service.c"), ("double-pace", "simulator/environment/sim_hub.c")]:
    if name == "fixed-wait":
        result = subprocess.run(["git", "show", f"{args.baseline}:{path}"], cwd=root, capture_output=True, text=True, check=True)
        source = result.stdout
    else:
        source = (root / path).read_text()
        old = 'speed = num_or(ls, "tts_speed", 1)'
        assert source.count(old) == 1
        source = source.replace(old, 'speed = num_or(ls, "tts_speed", 2)')
    target, obj = work / f"{name}.c", work / f"{name}.o"
    target.write_text(source)
    entry = db[root / path]
    command = shlex.split(entry["command"]) if "command" in entry else entry["arguments"]
    command[command.index("-o") + 1] = str(obj)
    command[command.index(entry["file"])] = str(target)
    subprocess.run(command, cwd=entry["directory"], check=True)
    link = shlex.split((build / "CMakeFiles/sim-ai-speech-long.dir/link.txt").read_text())
    binary = work / name
    link.insert(1, str(obj))
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    result = subprocess.run([os.sys.executable, str(root / "tests/ai_speech_long/run.py"),
        "--build", str(build), "--binary", str(binary), "--case", "stop"],
        cwd=root, capture_output=True, text=True)
    (work / f"{name}.log").write_text(result.stdout + result.stderr)
    assert result.returncode == 1, f"{name}: reverse check did not fail\n{result.stdout}"
    assert "CHECK FAILED" in result.stdout, result.stdout
    print(name, "expected failure:", result.stdout.splitlines()[0], flush=True)
print("Evidence:", work.relative_to(root))
