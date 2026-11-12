"""手动反向：仅把会话清理换回原 unload，复用构建的真实编译和链接参数。"""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build", type=Path)
parser.add_argument("font", type=Path)
args = parser.parse_args()
build = args.build.resolve()
database = json.loads((build / "compile_commands.json").read_text())
original = ROOT / "third_party/mogo/main/font/ttf_font.c"
entry = next(item for item in database if Path(item["file"]) == original)
with tempfile.TemporaryDirectory(prefix="mogo-close-", dir=build) as folder:
    work = Path(folder)
    source = original.read_text()
    start = source.index("\nvoid ink_mogo_font_shutdown(void) {")
    source = source[:start] + "\nvoid ink_mogo_font_shutdown(void) { ttf_font_unload(); }\n"
    mutant = work / "ttf_font.c"
    mutant.write_text(source)
    command = shlex.split(entry["command"])
    command[command.index(str(original))] = str(mutant)
    command[command.index("-o") + 1] = str(work / "ttf_font.o")
    command += ["-iquote", str(original.parent)]
    subprocess.run(command, cwd=entry["directory"], check=True)
    link = shlex.split((build / "third_party/mogo/CMakeFiles/mogo-font-file.dir/link.txt").read_text())
    index = next(i for i, arg in enumerate(link) if arg.endswith("/main/font/ttf_font.c.o"))
    link[index] = str(work / "ttf_font.o")
    link[link.index("-o") + 1] = str(work / "reverse-font")
    subprocess.run(link, cwd=build / "third_party/mogo", check=True)
    result = subprocess.run([str(work / "reverse-font"), str(args.font.resolve())],
                            capture_output=True, text=True)
    assert result.returncode != 0 and "live == 0" in result.stderr, result.stderr[:300]
    print(f"reverse close: original unload fails live == 0, exit {result.returncode}; source unchanged")
