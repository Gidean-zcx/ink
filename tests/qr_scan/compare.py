"""Optional evaluation (not ctest): point --zxing at an unpacked zxing-cpp v2.3.0 source tree."""
import argparse
from pathlib import Path
import subprocess
import sys
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--zxing", type=Path, required=True)
p.add_argument("--build", type=Path, required=True)
p.add_argument("--toolchain", type=Path)
a = p.parse_args()
work = a.build.resolve()
work.mkdir(exist_ok=True)
source = work / "source"
source.mkdir(exist_ok=True)
(source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.16)
project(qr_compare C CXX)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ZXING_WRITERS OFF CACHE STRING "" FORCE)
set(ZXING_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("{a.zxing.resolve()}" zxing)
file(GLOB qc "{ROOT}/third_party/quirc/*.c")
add_library(quirc STATIC ${{qc}})
target_include_directories(quirc PUBLIC "{ROOT}/third_party/quirc")
add_executable(compare "{ROOT}/tests/qr_scan/compare.cpp")
target_link_libraries(compare PRIVATE quirc ZXing::ZXing)
''')
prefix = ["taskpolicy", "-c", "utility"] if sys.platform == "darwin" else []
command = [*prefix, "cmake", "-S", str(source), "-B", str(work), "-DCMAKE_BUILD_TYPE=MinSizeRel"]
if a.toolchain:
    command += [f"-DCMAKE_TOOLCHAIN_FILE={a.toolchain.resolve()}"]
subprocess.run(command, check=True)
subprocess.run([*prefix, "cmake", "--build", str(work), "-j2", "--target", *( ["quirc", "ZXing"] if a.toolchain else ["compare"] )], check=True)
if not a.toolchain:
    images = work / "images"
    images.mkdir(exist_ok=True)
    files = []
    for p in sorted((ROOT / "tests/qr_scan/images").glob("*.png")):
        dst = images / (p.stem + ".gray")
        dst.write_bytes(Image.open(p).convert("L").tobytes())
        files.append(str(dst))
    subprocess.run([*prefix, str(work / "compare"), *files], check=True)
