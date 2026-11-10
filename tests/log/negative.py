"""Manual reverse controls: each changed logger must compile, then fail the same functional test."""
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
db = json.loads((build / "compile_commands.json").read_text())
entry = next(e for e in db if Path(e["file"]) == ROOT / "tests/log/log.c")
args = shlex.split(entry["command"])
flags = []
skip = False
for arg in args[1:]:
    if skip:
        skip = False
        continue
    if arg in ("-o", "-MF", "-MT", "-MQ"):
        skip = True
        continue
    if arg in ("-c", "-MD", "-MMD") or arg == entry["file"]:
        continue
    flags.append(arg)
source = (ROOT / "system/services/log/log.c").read_text()
cases = {
    "wrap": ("L.page = (L.page + 1) % (INK_LOG_BYTES / INK_LOG_SECTOR)", "L.page++"),
    "torn-crc": ("ink_log_valid(r) && r->seq > L.seq", "r->magic == INK_LOG_MAGIC && r->seq > L.seq"),
    "crash-tail": ("L.pending = valid && crash ? L.tail->used : 0", "L.pending = valid && !crash ? L.tail->used : 0"),
    "problem-flush": ("bool ok = ink_log_flush();", "bool ok = true;"),
    "privacy": ('description=[private]", n, page);', 'description=[private]", n, page); ink_log(INK_LOG_INFO, "problem", "%s", c->args.text);'),
}
with tempfile.TemporaryDirectory(prefix="ink-log-negative-") as d:
    for name, (old, new) in cases.items():
        assert old in source, name
        folder = Path(d) / name
        folder.mkdir()
        mutant = folder / "log.c"
        mutant.write_text(source.replace(old, new, 1))
        binary = folder / "test-log"
        subprocess.run([args[0], *flags, f'-DINK_LOG_SOURCE="{mutant}"', str(ROOT / "tests/log/log.c"),
                        str(ROOT / "ports/sim/hal/hal_log_sim.c"), "-o", str(binary)], check=True, capture_output=True)
        device = folder / "device"
        device.mkdir()
        run = subprocess.run([str(binary), str(device)], capture_output=True, text=True)
        if run.returncode == 0:
            raise SystemExit(f"FAIL: {name} mutation accepted")
        print(f"PASS: {name}: compiled, functional test rejected (exit {run.returncode})")
        print(run.stderr.strip().splitlines()[-1][:240])
