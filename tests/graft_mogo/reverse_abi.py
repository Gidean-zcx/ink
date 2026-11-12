"""Manually verify that the C++ contract test rejects removal of the C linkage guards."""
from pathlib import Path
import json
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
db = json.loads((build / "compile_commands.json").read_text())
entry = next(e for e in db if e["file"].endswith("/tests/graft_mogo/abi.cc"))
header = (root / "system/graft/graft.h").read_text()
mutant = header.replace('#ifdef __cplusplus\nextern "C" {\n#endif\n', "", 1)
mutant = mutant.replace('#ifdef __cplusplus\n}\n#endif\n', "", 1)
if mutant == header:
    raise SystemExit("C linkage guards missing from the current interface")
with tempfile.TemporaryDirectory(prefix="graft-abi-", dir=build) as directory:
    folder = Path(directory)
    (folder / "graft.h").write_text(mutant)
    command, skip = [], False
    for arg in shlex.split(entry["command"]):
        if skip:
            skip = False
        elif arg in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
        elif arg not in ("-MD", "-MMD"):
            command.append(arg)
    command.insert(1, "-I" + str(folder))
    command += ["-o", str(folder / "abi.o")]
    result = subprocess.run(command, cwd=entry["directory"], capture_output=True, text=True)
    if not result.returncode or "ink_graft_current" not in result.stderr or "linkage" not in result.stderr:
        raise SystemExit("C++ reverse control failed to detect the missing C ABI")
    print("reverse C ABI: expected compiler rejection without C linkage guards")
