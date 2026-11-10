"""Optional, compile/link only: compare recognizer probes in the same S31 ELF, never touch hardware."""
import argparse
from pathlib import Path
import json
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--zxing", type=Path, required=True)
p.add_argument("--zxing-lib", type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
work = build / "qr-size"
work.mkdir(exist_ok=True)
link = shlex.split((build / "CMakeFiles/ink-s31.elf.dir/link.txt").read_text())
cxx = Path(link[0])
ar = cxx.with_name("riscv32-esp-elf-ar")
size = cxx.with_name("riscv32-esp-elf-size")
archive_name = "esp-idf/ink_system/libink_system.a"
archive = build / archive_name
members = subprocess.check_output([str(ar), "t", str(archive)], text=True).splitlines()
assert members.count("scan.c.obj") == 1
probes = {
    "baseline": 'extern "C" int ink_camera_scan_init(void) { return 0; }',
    "quirc": '''#include "quirc.h"
#include <cstdlib>
extern "C" int ink_camera_scan_init(void) {
 auto q = quirc_new(); if (!q) return -1;
 auto c = (quirc_code*)malloc(sizeof(quirc_code)); auto d = (quirc_data*)malloc(sizeof(quirc_data));
 int r = quirc_resize(q,640,480);
 if (!r && c && d) { quirc_begin(q,nullptr,nullptr); quirc_end(q);
  if (quirc_count(q)) { quirc_extract(q,0,c); r=quirc_decode(c,d); } }
 free(c); free(d); quirc_destroy(q); return r;
}''',
    "zxing": '''#include "ReadBarcode.h"
#include <vector>
extern "C" int ink_camera_scan_init(void) {
 std::vector<unsigned char> p(640*480);
 auto result=ZXing::ReadBarcode({p.data(),640,480,ZXing::ImageFormat::Lum},
   ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::QRCode));
 return result.isValid();
}''',
}
values = {}
for name, text in probes.items():
    folder = work / name
    folder.mkdir(exist_ok=True)
    src, obj = folder / "probe.cpp", folder / "scan.c.obj"
    src.write_text(text)
    subprocess.run([str(cxx), "@"+str(build / "toolchain/cxxflags"), "-std=c++17", "-Os", "-ffunction-sections", "-fdata-sections", "-I"+str(ROOT / "third_party/quirc"), "-I"+str(a.zxing.resolve() / "core/src"), "-c", str(src), "-o", str(obj)], check=True)
    modified = folder / "libink_system.a"
    shutil.copyfile(archive, modified)
    subprocess.run([str(ar), "r", str(modified), str(obj)], check=True)
    output = folder / "probe.elf"
    command = [str(modified) if x == archive_name else x for x in link]
    command[command.index("-o") + 1] = str(output)
    command = ["-Wl,--Map="+str(folder / "probe.map") if x.startswith("-Wl,--Map=") else x for x in command]
    if name == "zxing":
        command.insert(command.index("-lstdc++"), str(a.zxing_lib.resolve()))
    linked = subprocess.run(command, cwd=build, capture_output=True, text=True)
    (folder / "link.log").write_text(linked.stdout + linked.stderr)
    if linked.returncode:
        raise SystemExit(linked.stderr[-3000:])
    raw = subprocess.check_output([str(size), "-A", str(output)], text=True)
    (folder / "size.txt").write_text(raw)
    # Linker's Flash-backed sections, including load images for initialized RAM.
    sections = {line.split()[0]: int(line.split()[1]) for line in raw.splitlines() if line.startswith(".")}
    values[name] = sum(v for k,v in sections.items() if k in (".flash.text", ".flash.rodata", ".iram0.text", ".dram0.data", ".flash.init_array", ".flash.tdata", ".iram0.data", ".rtc.text", ".rtc.force_fast", ".rtc.force_slow"))
values["quirc_delta"] = values["quirc"] - values["baseline"]
values["zxing_delta"] = values["zxing"] - values["baseline"]
(work / "summary.json").write_text(json.dumps(values, indent=2)+"\n")
print(json.dumps(values))
