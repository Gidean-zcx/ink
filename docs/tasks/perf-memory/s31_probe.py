"""Compile portable C units for S31; object sizes are not an IDF linked image."""
import argparse
import json
import re
import shlex
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--gcc", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    args.out.mkdir(parents=True, exist_ok=True)
    targets = ["ink_system", "ink_boot", "ink_apps", "ink_drivers", "ink_board_metalio_plus",
               "ink_image_codec", "ink_littlefs", "ink_cjson", "ink_lz4", "ink_sha256", "ink_tlsf"]
    flags = ["-march=rv32imafc_zicsr_zifencei_zaamo_zalrsc", "-mabi=ilp32f", "-mtune=esp-base",
             "-Os", "-std=gnu11", "-ffunction-sections", "-fdata-sections", "-fno-common",
             "-fno-jump-tables", "-fno-tree-switch-conversion", "-fstrict-volatile-bitfields",
             "-fstack-usage", "-fcallgraph-info=su", "-DNDEBUG",
             '-DINK_PSRAM_BSS=__attribute__((section(".ext_ram.bss")))']
    symbols, failures, units = [], [], []
    with (args.out / "compile.log").open("w") as log:
        for target in targets:
            directory = args.build / "CMakeFiles" / (target + ".dir")
            makefile = (directory / "build.make").read_text()
            sources = sorted(set(re.findall(r"^CMakeFiles/[^ ]+\.dir/(.+?\.c)\.o:", makefile, re.M)))
            options = (directory / "flags.make").read_text()
            includes = shlex.split(re.search(r"^C_INCLUDES = (.*)$", options, re.M)[1])
            defines = shlex.split(re.search(r"^C_DEFINES = (.*)$", options, re.M)[1])
            for source in sources:
                obj = args.out / (source.replace("/", "_") + ".o")
                result = subprocess.run([str(args.gcc), *flags, *defines, *includes,
                                         "-c", str(root / source), "-o", str(obj)],
                                        stdout=log, stderr=log)
                if result.returncode:
                    failures.append(source)
                    continue
                units.append(source)
                table = subprocess.check_output([str(args.gcc.with_name("riscv32-esp-elf-objdump")),
                                                 "-t", str(obj)], text=True)
                for line in table.splitlines():
                    columns = line.split()
                    if "O" not in columns or len(columns) < 6:
                        continue
                    section, size, name = columns[-3:]
                    if section.startswith((".bss", ".sbss", ".data", ".sdata", ".ext_ram.bss")):
                        symbols.append({"source": source, "name": name, "section": section,
                                        "bytes": int(size, 16)})
    (args.out / "objects.json").write_text(json.dumps({"units": units, "failures": failures,
        "symbols": symbols}, ensure_ascii=False, indent=2) + "\n")
    print(f"S31 objects: {len(units)} compiled, {len(failures)} failed")
    for source in failures:
        print("FAILED", source)
    raise SystemExit(bool(failures))


if __name__ == "__main__":
    main()
