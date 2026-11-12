"""Compile a throwaway HAL without its pin comparison; the real fixture must reject it."""
import json
from pathlib import Path
import resource
import shlex
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
source = Path(__file__).resolve().parents[1] / "hal_ws_s31.c"
db = json.loads((build / "compile_commands.json").read_text())
entry = next(e for e in db if Path(e["file"]).resolve() == source)
args = shlex.split(entry["command"])
original_object = args[args.index("-o") + 1]
needle = "if (w->pinned && memcmp(w->expected, w->peer, sizeof w->peer))"
assert source.read_text().count(needle) == 1
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
with tempfile.TemporaryDirectory(prefix="ws-reverse-", dir=build) as directory:
    temporary = Path(directory)
    mutant, obj, executable = (temporary / n for n in ("mutant.c", "mutant.o", "mutant"))
    mutant.write_text(source.read_text().replace(needle, "if (false)"))
    args[args.index("-o") + 1] = str(obj)
    args[args.index(entry["file"])] = str(mutant)
    subprocess.run(args, cwd=build, check=True, capture_output=True)
    link = shlex.split((build / "CMakeFiles/test-s31-ws.dir/link.txt").read_text())
    link[link.index(original_object)] = str(obj)
    link[link.index("-o") + 1] = str(executable)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    result = subprocess.run([str(executable)], cwd=temporary, capture_output=True, timeout=10)
    assert result.returncode != 0 and b"closed == 2" in result.stderr, result.stderr[-400:]
    print(f"Removed SPKI comparison: fixture failed as required (exit {result.returncode})")
