"""Run communication UX acceptance; --baseline is a manual history-dependent negative control."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--baseline", action="store_true")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
build = args.build.resolve()
output = build / "test-out" / ("comm-ux2-baseline" if args.baseline else "comm-ux2")
shutil.rmtree(output, ignore_errors=True)
output.mkdir(parents=True)
sim = build / "sim-comm-ux2"
if args.baseline:
    baseline = "a6598395aaceed6f74464d81a6df596eefb4c9d6"
    if subprocess.run(["git", "cat-file", "-e", baseline], cwd=root, capture_output=True).returncode:
        raise SystemExit("SKIP: manual baseline object unavailable")
    entries = json.loads((build / "compile_commands.json").read_text())
    link = shlex.split((build / "CMakeFiles/sim-comm-ux2.dir/link.txt").read_text())
    objects = []
    for file in ("system/apps/contacts/contacts.c", "system/apps/sms/sms.c", "system/apps/phone/phone.c", "system/services/comm/comm_service.c", "system/ui/shell/navigation.c"):
        source = output / file
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_bytes(subprocess.check_output(["git", "show", f"{baseline}:{file}"], cwd=root))
        entry = next(e for e in entries if Path(e["file"]) == root / file)
        command = shlex.split(entry["command"])
        obj = source.with_suffix(".o")
        command[1:1] = ["-iquote", str((root / file).parent)]
        command[command.index("-o") + 1] = str(obj)
        command[-1] = str(source)
        subprocess.run(command, cwd=build, check=True, capture_output=True)
        objects.append(str(obj))
    link[link.index("-o") + 1] = str(output / "sim-baseline")
    link[1:1] = objects
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    sim = output / "sim-baseline"
command = [str(sim), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
           "--script", str(root / "tests/sms/sms.script"), "--budget", str(root / "tests/ui_scene.budget.json"),
           "--assets", str(build / "assets"), "--content", str(build / "test-content"),
           "--flash-image", str(output / "user.img"), "--sd-dir", str(output / "sd"),
           "--out", str(output), "--quiet", "--no-random-outages"]
result = subprocess.run(command, env=dict(os.environ, INK_COMM_UX_MODE="baseline" if args.baseline else "current"), capture_output=True, text=True)
(output / "process.log").write_text(result.stdout + result.stderr)
if not (output / "metrics.json").exists():
    raise SystemExit(f"process exit {result.returncode}, no metrics: {result.stdout[-1000:]} {result.stderr[-3000:]}")
metrics = json.loads((output / "metrics.json").read_text())
counters = metrics.get("metrics", {})
print(f'comm-ux2: exit={result.returncode} pass={counters.get("test.pass", 0)} fail={counters.get("test.fail", 0)} finished={counters.get("test.finished", 0)}')
if result.returncode or not counters.get("test.finished") or counters.get("test.fail", 0):
    for warning in metrics.get("warnings", []):
        print(warning)
    print(result.stderr[-3000:])
    raise SystemExit(1)
for snapshot in output.glob("*.json"):
    if snapshot.name not in ("metrics.json", "app-list.json", "before-app-list.json"):
        json.loads(snapshot.read_text())
if not os.environ.get("INK_KEEP_EVIDENCE"):
    shutil.rmtree(output)
