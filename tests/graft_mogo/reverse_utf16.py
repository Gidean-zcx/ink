"""Manual reverse control using the fixed upstream file, without editing this checkout."""
from pathlib import Path
import hashlib
import json
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
upstream = Path(sys.argv[2]).resolve()
manifest = json.loads((root / "third_party/mogo/upstream-files.json").read_text())
expected = next(f["sha256"] for f in manifest["files"] if f["path"] == "main/book/book_txt.c")
if hashlib.sha256(upstream.read_bytes()).hexdigest() != expected:
    raise SystemExit("reverse control requires the pinned upstream TXT file")
db = json.loads((build / "compile_commands.json").read_text())
entry = next(e for e in db if e["file"].endswith("/main/book/book_txt.c") and "mogo-utf16.dir" in e["command"])
with tempfile.TemporaryDirectory(prefix="mogo-reverse-", dir=build) as directory:
    folder = Path(directory)
    source = folder / "book_txt.c"
    source.write_bytes(upstream.read_bytes())
    obj = folder / "book_txt.o"
    command, skip = [], False
    for arg in shlex.split(entry["command"]):
        if skip:
            skip = False
        elif arg in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
        elif arg not in ("-MD", "-MMD"):
            command.append(str(source) if arg == entry["file"] else arg)
    subprocess.run([*command, "-o", str(obj)], cwd=entry["directory"], check=True, capture_output=True)
    native_build = build / "third_party/mogo"
    link = shlex.split((native_build / "CMakeFiles/mogo-utf16.dir/link.txt").read_text())
    link = [str(obj) if arg.endswith("/main/book/book_txt.c.o") else arg for arg in link]
    binary = folder / "unpatched-utf16"
    link[link.index("-o") + 1] = str(binary)
    subprocess.run(link, cwd=native_build, check=True, capture_output=True)
    result = subprocess.run([sys.executable, str(root / "tests/graft_mogo/run_utf16.py"), str(binary)],
                            capture_output=True, text=True)
    if result.returncode == 0 or "exit status 4" not in result.stderr:
        raise SystemExit("reverse control did not fail at UTF-16 chapter/byte-position validation")
    print("reverse UTF-16: expected failure at chapter/byte-position validation (native exit 4)")
