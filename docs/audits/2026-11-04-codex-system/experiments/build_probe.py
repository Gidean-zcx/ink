#!/usr/bin/env python3
"""Compile an audit-only ink_main using the existing sim-systest link command. No source mutations."""
from pathlib import Path
import re
import shlex
import subprocess
import sys

root=Path(__file__).resolve().parents[4]
build=(root/(sys.argv[1] if len(sys.argv)>1 else "build-audit")).resolve()
source=Path(__file__).with_name(sys.argv[2] if len(sys.argv)>2 else "service_probe.c")
make=(build/"CMakeFiles/sim-systest.dir/flags.make").read_text()
flags=[]
for name in ("C_DEFINES", "C_INCLUDES", "C_FLAGS"):
    flags += shlex.split(re.search(r"^"+name+r" = (.*)$",make,re.M).group(1))
link=shlex.split((build/"CMakeFiles/sim-systest.dir/link.txt").read_text())
obj=build/(source.stem+".o")
subprocess.run([link[0],*flags,"-c",str(source),"-o",str(obj)],check=True)
link=[str(obj) if x=="CMakeFiles/sim-systest.dir/tests/integration/systest.c.o" else x for x in link]
link.insert(link.index("libink_system.a"), "libink_apps.a")
link[link.index("-o")+1]=str(build/source.stem)
subprocess.run(link,cwd=build,check=True)
print(build/source.stem)
