#!/usr/bin/env python3
"""Compile only, using the existing target's exact compile/link flags. Root owns all edits."""
import argparse,pathlib,shlex,subprocess
p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('--template',default='sim-update');p.add_argument('--build',default='build-audit');p.add_argument('--name');p.add_argument('--extra',action='append',default=[]);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[4];build=root/a.build;source=root/a.source
dest=build/'audit-probes'/(a.name or source.stem);dest.mkdir(parents=True,exist_ok=True)
flags={}
for line in (build/'CMakeFiles'/(a.template+'.dir')/'flags.make').read_text().splitlines():
    if ' = ' in line:
        k,v=line.split(' = ',1);flags[k]=shlex.split(v)
link=shlex.split((build/'CMakeFiles'/(a.template+'.dir')/'link.txt').read_text())
objects=[]
for i,f in enumerate([str(source),*[str(root/x) for x in a.extra]]):
    obj=dest/f'part-{i}.o'
    subprocess.run([link[0],*flags['C_DEFINES'],*flags['C_INCLUDES'],*flags['C_FLAGS'],'-Wno-unused-function','-c',f,'-o',str(obj)],cwd=build,check=True)
    objects.append(str(obj))
for i,x in enumerate(link):
    if '/tests/integration/' in x and x.endswith('.o'):link[i]=objects[0];break
link[link.index('-o')+1]=str(dest/'probe')
at=next(i for i,x in enumerate(link) if x.endswith('.a'));link[at:at]=objects[1:]
subprocess.run(link,cwd=build,check=True)
print(dest/'probe')
