"""Manual baseline control for the unrelated races failure; not registered in ctest."""
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]
build = Path(sys.argv[1] if len(sys.argv) > 1 else root / "build").resolve()
base = "83ec2e4e4483d530866603c2e93474ed30ce96a3"
if subprocess.run(["git", "cat-file", "-e", base], cwd=root, capture_output=True).returncode:
    print("SKIP: manual baseline control needs the task's baseline commit")
    sys.exit(0)
db = {Path(e["file"]).resolve(): e for e in json.loads((build / "compile_commands.json").read_text())}
(root / "work").mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="races-control-", dir=root / "work") as directory:
    work = Path(directory)
    shadow = work / "include/ink"
    shadow.mkdir(parents=True)
    def original(path):
        return subprocess.check_output(["git", "show", base + ":" + path], cwd=root)
    (shadow / "hal_transfer.h").write_bytes(original("platform/include/ink/hal_transfer.h"))
    objects = []
    for filename in ["system/services/file/transfer.c", "ports/sim/hal/hal_transfer_sim.c"]:
        source = work / Path(filename).name
        source.write_bytes(original(filename))
        entry = db[(root / filename).resolve()]
        args = shlex.split(entry["command"])
        obj = work / (source.stem + ".o")
        args[args.index("-o") + 1] = str(obj)
        args[args.index(str((root / filename).resolve()))] = str(source)
        args[1:1] = ["-I" + str(work / "include"), "-I" + str((root / filename).parent)]
        subprocess.run(args, cwd=build, check=True, capture_output=True)
        objects.append(str(obj))
    link = shlex.split((build / "CMakeFiles/sim-races.dir/link.txt").read_text())
    link[link.index("-o") + 1] = str(work / "sim-races-baseline")
    i = next(i for i, value in enumerate(link) if value.endswith(".a"))
    link[i:i] = objects
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    for label, binary in [("current1", build / "sim-races"), ("baseline", work / "sim-races-baseline")]:
        out = work / label
        command = [str(binary), "--profile", str(root / "simulator/profiles/metalio-plus.json"),
                   "--budget", str(root / "tests/integration/races.budget.json"),
                   "--script", str(root / "tests/integration/races.script"),
                   "--assets", str(build / "assets"), "--content", str(build / "test-content"),
                   "--out", str(out), "--quiet", "--no-random-outages"]
        for stage in ["fresh", "retained"]:
            result = subprocess.run(command, cwd=root, capture_output=True, text=True)
            data = json.loads((out / "metrics.json").read_text())
            warnings = [w.replace(str(root) + "/", "") for w in data.get("warnings", [])]
            print(label, stage, "exit=", result.returncode, "warnings=", warnings, flush=True)
