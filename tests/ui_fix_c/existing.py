"""Relink the current panel into unchanged regression drivers while the full build is blocked."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
work = build / "ui-fix-c/existing"
work.mkdir(parents=True, exist_ok=True)
entry = next(e for e in json.loads((build / "compile_commands.json").read_text())
             if Path(e["file"]) == ROOT / "system/ui/shell/ai_panel.c")
command = shlex.split(entry["command"]) if "command" in entry else entry["arguments"][:]
obj = work / "current-panel.o"
command[command.index("-o") + 1] = str(obj)
subprocess.run(command, cwd=entry["directory"], check=True)
binaries = {}
for target in ("sim-ai-chat", "sim-ui-regressions", "sim-ai-history-photo"):
    command = shlex.split((build / f"CMakeFiles/{target}.dir/link.txt").read_text())
    command.insert(1, str(obj))
    binary = work / target
    command[command.index("-o") + 1] = str(binary)
    subprocess.run(command, cwd=build, check=True, capture_output=True, text=True)
    binaries[target] = binary

results = []
for name in ("ai-chat", "ui-answer-regressions-metalio-plus", "ui-answer-regressions-desktop-lcd"):
    line = next(line for line in (build / "CTestTestfile.cmake").read_text().splitlines()
                if line.startswith(f"add_test({name} "))
    command = shlex.split(line[len("add_test("):-1])[1:]
    command = [str(binaries[Path(item).name]) if Path(item).name in binaries else item for item in command]
    folder = work / name
    command[command.index("--out") + 1] = str(folder)
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
    (work / f"{name}.log").write_text(result.stdout + result.stderr)
    data = json.loads((folder / "metrics.json").read_text())
    counts = data.get("metrics", {})
    passed = result.returncode == 0 and counts.get("test.pass", 0) > 0 and not counts.get("test.fail", 0)
    results.append({"case": name, "exit": result.returncode, "pass": passed,
                    "checks": counts.get("test.pass", 0), "failed": counts.get("test.fail", 0)})
    print(name, "PASS" if passed else "FAIL", results[-1], flush=True)

for name in ("assets", "test-content"):
    link = work / name
    if not link.exists():
        link.symlink_to(build / name, target_is_directory=True)
result = subprocess.run([os.sys.executable, "-B", str(ROOT / "tests/ai_chat_snapshots.py"),
                         str(work / "ai-chat"), str(work / "ai-chat-compare")],
                        cwd=ROOT, capture_output=True, text=True, timeout=30)
(work / "snapshots.log").write_text(result.stdout + result.stderr)
results.append({"case": "ai-chat-snapshots", "exit": result.returncode, "pass": result.returncode == 0})
print("ai-chat-snapshots", "PASS" if result.returncode == 0 else "FAIL", result.stdout, flush=True)
result = subprocess.run([os.sys.executable, "-B", str(ROOT / "tests/ai_history_photo/run.py"),
                         "--build", str(work), "--case", "replay"],
                        cwd=ROOT, capture_output=True, text=True, timeout=180)
(work / "replay.log").write_text(result.stdout + result.stderr)
results.append({"case": "ai-history-photo/replay", "exit": result.returncode, "pass": result.returncode == 0})
print("ai-history-photo/replay", "PASS" if result.returncode == 0 else "FAIL", result.stdout, flush=True)
(work / "summary.json").write_text(json.dumps(results, indent=2) + "\n")
print("Evidence:", work.relative_to(ROOT))
raise SystemExit(0 if all(row["pass"] for row in results) else 1)
