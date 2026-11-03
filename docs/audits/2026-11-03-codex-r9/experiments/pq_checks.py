#!/usr/bin/env python3
"""Capture differential-kernel results; expected defect exit 1 is kept as evidence."""
import json, os, subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
os.chdir(ROOT)
rows=[]
for san in (False,True):
    exe=ROOT/'build-audit'/('r9-pq-skip-asan' if san else 'r9-pq-skip')
    compile=['cc','-O1','-g','-Wall','-Wextra','-Werror','-std=c11','-Idrivers/display/pqbn',str(HERE/'pq_skip.c'),'drivers/display/pqbn/pqbn_v4.c','-lm','-o',str(exe)]
    if san: compile+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    for label,cmd in [('compile',compile),('run',[str(exe)])]:
        p=subprocess.run(cmd,capture_output=True,text=True,timeout=55,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
        row=dict(label=label,sanitizer=san,command=cmd,exit_code=p.returncode,output=(p.stdout+p.stderr).splitlines())
        rows.append(row); (HERE/'pq-results.json').write_text(json.dumps(rows,indent=2)+'\n')
        print(json.dumps(row),flush=True)
        if label=='compile' and p.returncode: raise SystemExit(p.returncode)
