"""Exercise the real firmware component CMake with only the IDF registration boundary stubbed."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
work = Path(tempfile.mkdtemp(prefix="autotest-cmake-", dir=build))
try:
    source = work / "source"
    source.mkdir()
    (source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.22)
project(autotest_switch C)
file(GLOB_RECURSE system_sources "${REPO}/system/*.c")
add_library(fake_system STATIC ${system_sources})
set(COMPONENT_LIB fake_main)
function(idf_component_register)
  add_library(fake_main STATIC "${REPO}/ports/esp32s31/main/main.c")
endfunction()
function(idf_component_get_property out component property)
  set(${out} fake_system PARENT_SCOPE)
endfunction()
include("${REPO}/ports/esp32s31/main/CMakeLists.txt")
get_target_property(system_sources fake_system SOURCES)
get_target_property(main_sources fake_main SOURCES)
get_target_property(links fake_main INTERFACE_LINK_LIBRARIES)
file(WRITE "${CMAKE_BINARY_DIR}/sources.txt" "${system_sources};${main_sources}")
file(WRITE "${CMAKE_BINARY_DIR}/links.txt" "${links}")
''')
    for mode in ("default", "OFF", "ON", "missing"):
        area = work / mode
        args = ["cmake", "-S", str(source), "-B", str(area), f"-DREPO={root}"]
        if mode != "default":
            args.append(f"-DINK_AUTOTEST={'ON' if mode == 'missing' else mode}")
        if mode == "ON":
            args.append(f"-DINK_AUTOTEST_SCRIPT={root / 'tests/device_autotest/device.script'}")
        result = subprocess.run(args, capture_output=True, text=True)
        if mode == "missing":
            assert result.returncode and "INK_AUTOTEST requires INK_AUTOTEST_SCRIPT" in result.stderr
            continue
        assert result.returncode == 0, result.stderr
        sources = (area / "sources.txt").read_text().split(";")
        selected = [Path(p).name for p in sources if "/system/autotest/" in p]
        assert sorted(selected) == (["device.c", "script.c"] if mode == "ON" else []), selected
        links = (area / "links.txt").read_text()
        assert ("--wrap=ink_main" in links) == (mode == "ON")
        if mode == "ON":
            header = next(area.rglob("autotest_script.h")).read_text()
            assert "0x40," in header
    print("firmware switch: default/OFF exclude all autotest code; ON embeds script and wrappers; missing script fails")
finally:
    if not os.environ.get("INK_KEEP_EVIDENCE"):
        shutil.rmtree(work)
