#!/usr/bin/env python3
"""Build an audit entry point with the original project's flags and libraries."""
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[4]


def build_probe(build, source, name=None, source_text=None, extra_sources=()):
    build = (ROOT / build).resolve()
    source = Path(source).resolve()
    name = name or source.stem
    make = (build / "CMakeFiles/sim-systest.dir/flags.make").read_text()
    flags = []
    for key in ("C_DEFINES", "C_INCLUDES", "C_FLAGS"):
        flags += shlex.split(re.search(r"^" + key + r" = (.*)$", make, re.M).group(1))
    link = shlex.split((build / "CMakeFiles/sim-systest.dir/link.txt").read_text())
    obj = build / (name + ".o")
    if source_text is None:
        subprocess.run([link[0], *flags, "-c", str(source), "-o", str(obj)], check=True)
    else:
        subprocess.run([link[0], *flags, "-I", str(source.parent), "-x", "c", "-c", "-", "-o", str(obj)], input=source_text, text=True, check=True)
    link = [str(obj) if x == "CMakeFiles/sim-systest.dir/tests/integration/systest.c.o" else x for x in link]
    for k, (extra_path, extra_text) in enumerate(extra_sources):
        extra_obj = build / (name + f"-extra-{k}.o")
        subprocess.run([link[0], *flags, "-I", str(extra_path.parent), "-x", "c", "-c", "-", "-o", str(extra_obj)],
                       input=extra_text, text=True, check=True)
        link.insert(link.index("libink_system.a"), str(extra_obj))
    link.insert(link.index("libink_system.a"), "libink_apps.a")
    link[link.index("-o") + 1] = str(build / name)
    subprocess.run(link, cwd=build, check=True)
    return build / name


if __name__ == "__main__":
    print(build_probe(sys.argv[1], Path(__file__).with_name(sys.argv[2])))
