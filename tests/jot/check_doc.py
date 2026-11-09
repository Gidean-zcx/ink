"""Keep the guide's C excerpts, source links and example size checkable."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
doc = ROOT / "docs/system/app-framework.md"
text = doc.read_text()
source = (ROOT / "examples/jot/jot.c").read_text()
blocks = re.findall(r"```([^\n]*)\n(.*?)\n```", text, re.S)
assert blocks, "missing example excerpts"
for language, code in blocks:
    assert language == "c", f"unverified code language: {language}"
    assert code.strip() in source, "guide excerpt differs from the compiled example"
assert "<example:" not in text, "unexpanded excerpt"
links = re.findall(r"\]\(([^)]+)\)", text)
headers = 0
for link in links:
    path, _, anchor = link.partition("#")
    if "://" in path:
        continue
    target = (doc.parent / path).resolve()
    assert target.is_file(), f"missing linked file: {path}"
    if anchor.startswith("L"):
        line = int(anchor[1:])
        lines = target.read_text().splitlines()
        assert 0 < line <= len(lines) and lines[line - 1].strip(), f"invalid source line: {link}"
        headers += target.suffix == ".h"
assert headers >= 20, "missing header provenance"
example_files = sorted((ROOT / "examples/jot").glob("*.[ch]"))
line_count = sum(len(file.read_text().splitlines()) for file in example_files)
assert line_count <= 300, f"example exceeds 300 lines: {line_count}"
assert not re.search(r"\b(?:hal|osal|sim|vtk)_", source), "example reaches below the app framework"
cmake = (ROOT / "CMakeLists.txt").read_text()
assert cmake.count("examples/jot/jot.c") == 1
assert "ink_sim_executable(sim-jot tests/jot/app.c examples/jot/jot.c)" in cmake
assert "jot" not in (ROOT / "system/boot/main.c").read_text(), "example entered the factory boot"
print(f"PASS: {len(blocks)} exact C excerpts, {headers} header references, {line_count}/300 example lines")
