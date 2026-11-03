#!/usr/bin/env python3
"""Build only committed source snapshots in build-audit and compare pre/post-restructure metrics."""
import hashlib, io, json, subprocess, tarfile
from pathlib import Path

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
DEPS=Path('/Users/bytedance/Documents/Workspace/ink')
OUT=ROOT/'build-audit/history'
OUT.mkdir(exist_ok=True)
records=[]
for rev in ('4309087','dc085cb'):
    src=OUT/rev/'src'; build=OUT/rev/'build'
    src.mkdir(parents=True,exist_ok=True); build.mkdir(exist_ok=True)
    top=subprocess.check_output(['git','ls-tree','--name-only',rev],cwd=ROOT,text=True).splitlines()
    wanted={'CMakeLists.txt','apps','system','platform','drivers','boards','ports','sim','profiles','simulator','tests','tools','third_party'}
    paths=[x for x in top if x in wanted]
    data=subprocess.check_output(['git','archive',rev,*paths],cwd=ROOT)
    with tarfile.open(fileobj=io.BytesIO(data)) as ar:
        for m in ar.getmembers():
            if m.name.startswith('/') or '..' in Path(m.name).parts or m.issym() or m.islnk():
                raise RuntimeError('archive path rejected')
        ar.extractall(src)
    commands=[
        ['cmake','-S',str(src),'-B',str(build),'-DINK_WERROR=ON','-DINK_GUI=OFF',
         '-DINK_CJK_FONT='+str(DEPS/'build-deps/fonts/NotoSansCJK-Regular.ttc'),'-DINK_CJK_FACE=2',
         '-DPython3_EXECUTABLE='+str(DEPS/'build-deps/pyenv/bin/python'),
         '-DFETCHCONTENT_SOURCE_DIR_LVGL='+str(DEPS/'build/_deps/lvgl-src')],
        ['cmake','--build',str(build),'-j','8'],
        [str(DEPS/'build-deps/pyenv/bin/ctest'),'--test-dir',str(build),'-j','8','--timeout','300','--output-on-failure']]
    for label,cmd in zip(('configure','build','ctest'),commands):
        print(rev,label,'begin',flush=True)
        log=OUT/rev/(label+'.log')
        with log.open('w') as f:
            p=subprocess.run(cmd,cwd=ROOT,stdout=f,stderr=subprocess.STDOUT,timeout=300)
        records.append(dict(revision=rev,step=label,command=cmd,exit_code=p.returncode,tail=log.read_text().splitlines()[-4:]))
        print(rev,label,'exit',p.returncode,flush=True)
        (HERE/'history-evidence.json').write_text(json.dumps({'commands':records,'complete':False},ensure_ascii=False,indent=2)+'\n')
        if p.returncode:
            raise SystemExit(p.returncode)

old=OUT/'4309087/build/test-out'; new=OUT/'dc085cb/build/test-out'
a={str(p.relative_to(old)):p for p in old.rglob('metrics.json')}
b={str(p.relative_to(new)):p for p in new.rglob('metrics.json')}
diffs=[]
for path in sorted(a.keys() & b.keys()):
    x=json.loads(a[path].read_text()); y=json.loads(b[path].read_text())
    if x!=y:
        keys=[k for k in set(x)|set(y) if x.get(k)!=y.get(k)]
        diffs.append(dict(path=path,different_top_level_keys=keys))
png_a={str(p.relative_to(old)):hashlib.sha256(p.read_bytes()).hexdigest() for p in old.rglob('*.png')}
png_b={str(p.relative_to(new)):hashlib.sha256(p.read_bytes()).hexdigest() for p in new.rglob('*.png')}
out={'commands':records,'complete':True,'metrics_pairs':len(a.keys() & b.keys()),'metrics_only_before':sorted(a.keys()-b.keys()),'metrics_only_after':sorted(b.keys()-a.keys()),'metrics_differences':diffs,
     'png_pairs':len(png_a.keys()&png_b.keys()),'png_only_before':sorted(png_a.keys()-png_b.keys()),'png_only_after':sorted(png_b.keys()-png_a.keys()),'png_differences':[p for p in png_a.keys()&png_b.keys() if png_a[p]!=png_b[p]]}
(HERE/'history-evidence.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v for k,v in out.items() if k!='commands'},ensure_ascii=False,indent=2),flush=True)
