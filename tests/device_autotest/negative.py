"""Manual reverse controls; builds only temporary objects from the real compile database."""
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
work = Path(tempfile.mkdtemp(prefix="autotest-negative-", dir=build))
db = {Path(e["file"]).resolve(): e for e in json.loads((build / "compile_commands.json").read_text())}

def compile_object(original, mutant, output):
    entry = db[original]
    command = shlex.split(entry["command"])
    command[command.index("-o") + 1] = str(output)
    command[command.index(str(original))] = str(mutant)
    subprocess.run(command, cwd=entry["directory"], check=True, capture_output=True)

def link_and_fail(target, old_object, new_object, name):
    command = shlex.split((build / f"CMakeFiles/{target}.dir/link.txt").read_text())
    command = [str(new_object) if old_object in part else part for part in command]
    binary = work / name
    command[command.index("-o") + 1] = str(binary)
    subprocess.run(command, cwd=build, check=True, capture_output=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    (work / f"{name}.log").write_text(result.stdout + result.stderr)
    assert result.returncode != 0 and "Assertion" in result.stderr, result.stderr
    print(f"{name}: expected failure exit={result.returncode}")

try:
    parser = root / "system/autotest/script.c"
    source = parser.read_text()
    mutant = work / "script.c"
    mutant.write_text(source[:source.index("bool ink_script_cap_allowed(")] + "bool ink_script_cap_allowed(const ink_cap_t *c) { (void)c; return true; }\n")
    obj = work / "parser.o"
    compile_object(parser, mutant, obj)
    link_and_fail("test-autotest-parser", "libink_script.a", obj, "without-cap-guard")
    device = root / "system/autotest/device.c"
    mutant = work / "device.c"
    mutant.write_text(device.read_text().replace("if (!result) input_sink(&h, input_ctx);", "if (false) input_sink(&h, input_ctx);"))
    driver = root / "tests/device_autotest/device.c"
    copy = work / "driver.c"
    copy.write_text(driver.read_text().replace('"../../system/autotest/device.c"', '"' + str(mutant) + '"'))
    obj = work / "device.o"
    compile_object(driver, copy, obj)
    link_and_fail("test-autotest-device", "tests/device_autotest/device.c.o", obj, "without-input-forwarding")
finally:
    if not os.environ.get("INK_KEEP_EVIDENCE"):
        shutil.rmtree(work)
