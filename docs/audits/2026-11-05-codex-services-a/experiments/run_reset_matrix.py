#!/usr/bin/env python3
"""Reset mechanism only: no authentication experiments. All state is synthetic."""
import json,os,pathlib,shutil,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[4]
HERE=pathlib.Path(__file__).resolve().parent
BUILD=ROOT/'build-audit'
work=pathlib.Path(tempfile.mkdtemp(prefix='reset-matrix-',dir=BUILD/'audit-probes'))
results=[]
def run(d,label,script=None,**env):
    (d/'sd/content').mkdir(parents=True,exist_ok=True)
    cmd=[str(BUILD/'audit-probes/reset_probe/probe'),'--profile',str(ROOT/'simulator/profiles/metalio-plus.json'),'--quiet','--no-random-outages','--assets',str(BUILD/'assets'),'--content',str(BUILD/'test-content'),'--sd-dir',str(d/'sd'),'--flash-image',str(d/'user.img'),'--out',str(d/label)]
    cmd+=['--script',str(script)] if script else ['--end','15s']
    e=os.environ.copy();e.update({k:str(v) for k,v in env.items()})
    p=subprocess.run(cmd,cwd=ROOT,env=e,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,timeout=60)
    m=json.loads((d/label/'metrics.json').read_text())
    print('CASE',d.name,label,'exit',p.returncode,flush=True);print(p.stdout,flush=True)
    assert p.returncode==0 and m['metrics'].get('test.fail',0)==0,(label,m.get('warnings'),p.stderr)
    if not script:assert m['metrics'].get('test.finished')==1
    results.append(dict(case=d.name,label=label,exit=p.returncode,stdout=p.stdout,metrics={k:v for k,v in m['metrics'].items() if k.startswith(('system.factory','test.','flash.power'))}))
    (HERE/'reset-results.json').write_text(json.dumps(dict(work=str(work),runs=results),ensure_ascii=False,indent=2)+'\n')
    return m['metrics']
seed=work/'seed'
run(seed,'prepare',AUDIT_RESET_SEED=1)
def clone(name):
    d=work/name;d.mkdir()
    for n in ('user.img','user.img.ota'):shutil.copy2(seed/n,d/n)
    shutil.copytree(seed/'sd',d/'sd')
    return d
run(clone('control'),'resume')
run(clone('card-open-fails'),'resume',AUDIT_FAIL_CARD_ROOT=1)
run(clone('flash-readdir-fails'),'resume',AUDIT_FAIL_USER_READDIR=1)
cuts=0
for n in range(1,33):
    d=clone(f'cut-{n}')
    script=d/'cut.script';script.write_text(f'@0 powercut write {n}\n@15s end\n')
    m=run(d,'cut',script=script)
    run(d,'after-cut')
    if m.get('system.factory_reset_done'):
        break
    cuts+=1
print('RESET MATRIX complete; reset Flash cuts exercised',cuts,'artifacts',work)
