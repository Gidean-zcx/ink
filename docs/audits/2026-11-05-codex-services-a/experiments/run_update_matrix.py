#!/usr/bin/env python3
"""Synthetic packages, real product boots and HAL. No existing fixtures are modified."""
import hashlib, hmac, importlib.util, json, os, pathlib, struct, subprocess, sys, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[4]
HERE=pathlib.Path(__file__).resolve().parent
BUILD=ROOT/os.environ.get('AUDIT_BUILD','build-audit')
spec=importlib.util.spec_from_file_location('inkfw',ROOT/'tools/inkfw/make_inkfw.py')
fw=importlib.util.module_from_spec(spec);spec.loader.exec_module(fw)
work=pathlib.Path(tempfile.mkdtemp(prefix='update-matrix-',dir=BUILD/'audit-probes'))
results=[]
def pkg(code=200,**kw):return fw.build(f'{code//100}.0.0',code,app_size=8192,**kw)[0]
def resign(b):
    hs=struct.unpack_from('<I',b,12)[0]
    b[hs-64:hs]=bytes(64)
    b[hs-64:hs-32]=hmac.new(fw.KEY,hashlib.sha256(b[:hs]).digest(),hashlib.sha256).digest()
    return b
def chain(name):
    p=work/name;(p/'sd/content/update').mkdir(parents=True);return p
def run(p,phase,label,**env):
    out=p/label
    cmd=[str(BUILD/'audit-probes/update_probe/probe'),'--profile',str(ROOT/'simulator/profiles/metalio-plus.json'),'--quiet','--no-random-outages','--assets',str(BUILD/'assets'),'--content',str(BUILD/'test-content'),'--sd-dir',str(p/'sd'),'--flash-image',str(p/'user.img'),'--out',str(out),'--script',str(HERE/'network.script')]
    e=os.environ.copy();e.update(AUDIT_PHASE=phase);e.update({k:str(v) for k,v in env.items()})
    r=subprocess.run(cmd,cwd=ROOT,env=e,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,timeout=120)
    print('\nCASE',p.name,label,'exit',r.returncode,flush=True)
    print(r.stdout,flush=True)
    if r.returncode:print(r.stderr[-4000:],flush=True)
    m=json.loads((out/'metrics.json').read_text())
    mm=m['metrics']
    summary={k:v for k,v in mm.items() if k.startswith(('update.','ota.','power.reboot','test.'))}
    results.append(dict(chain=p.name,phase=label,exit=r.returncode,metrics=summary,stdout=r.stdout,warnings=m.get('warnings',[])))
    assert r.returncode==0 and mm.get('test.fail',0)==0,(label,m.get('warnings',[]))
    if phase=='install' or env.get('AUDIT_SELF_FAIL'):assert mm.get('power.reboot')==1,label
    else:assert mm.get('test.finished')==1,label
    return mm
def install(p,code=200,**kw):
    (p/'sd/content/update/next.inkfw').write_bytes(pkg(code,**kw))
    return run(p,'install',f'install-{code}')

# Each state transition still uses the actual storage service, except this one refused enqueue.
p=chain('state-control');install(p,font=4096,resources=8192);m=run(p,'inspect','valid')
assert m.get('ota.resource_bytes')==12288
p=chain('state-refused');(p/'sd/content/update/next.inkfw').write_bytes(pkg(font=4096,resources=8192))
run(p,'install','install-200',AUDIT_FAIL_STATE=1);m=run(p,'inspect','valid-without-state')
assert m.get('ota.marked_valid')==1 and m.get('ota.resource_bytes',0)==0

# A real resource-write failure, then an ordinary newer package containing only an app.
p=chain('resources-replaced');install(p,font=4096,resources=8192)
m=run(p,'inspect','half-resources',INK_SIM_OTA_RESOURCE_FAILS=8192)
assert m.get('update.resources_failed')==1
(p/'sd/content/update/new.inkfw').write_bytes(pkg(300))
run(p,'install','install-300',INK_SIM_OTA_RESOURCE_FAILS=0,AUDIT_INSTALL_FILE='new.inkfw')
m=run(p,'inspect','new-version-without-resources')
assert m.get('ota.marked_valid')==1 and m.get('ota.resource_bytes',0)==0

# Two failed upgrades: the first failed version is offered again after the second rollback.
p=chain('blocked-history')
for code in (200,300):
    install(p,code)
    run(p,'inspect',f'fails-{code}',AUDIT_SELF_FAIL=1)
    (p/'sd/content/prior.inkfw').write_bytes(pkg(200))
    run(p,'inspect',f'rollback-{code}',AUDIT_PRIOR=1)

# Signed-but-ill-formed tables. These test publisher/format validation, not signature bypass.
p=chain('package-table');d=p/'sd/content/cases';d.mkdir()
normal=pkg(font=4096)
cases={'normal':normal}
b=bytearray(normal);b[88+56+8:88+56+56]=b[88+8:88+56];cases['overlap']=resign(b)
b=bytearray(normal);struct.pack_into('<I',b,88+56,0);cases['duplicate']=resign(b)
b=bytearray(normal);struct.pack_into('<I',b,88+56,99);cases['unknown']=resign(b)
b=bytearray(normal);struct.pack_into('<QQ',b,88+8,0,0);b[88+24:88+56]=hashlib.sha256(b'').digest();cases['zero']=resign(b)
b=bytearray(normal);struct.pack_into('<Q',b,88+8,len(b)+4096);cases['past_eof']=resign(b)
cases['truncated']=normal[:-2048]
b=bytearray(normal);struct.pack_into('<QQ',b,88+8,2**64-8,32);cases['overflow']=resign(b)
b=bytearray(normal);b[88+8]^=0x20;cases['unsigned_damage']=b
for name,b in cases.items():(d/(name+'.inkfw')).write_bytes(b)
run(p,'packages','check-tables')

# Update/check publishes a new package while update/fetch is in progress.
p=chain('download-offer-changed')
old=fw.build('2.0.0',200,app_size=131072)[0]
new=fw.build('3.0.0',300,app_size=196608)[0]
(p/'old.inkfw').write_bytes(old);(p/'new.inkfw').write_bytes(new)
run(p,'download','changing-offer',INK_SIM_UPDATE_PKG=p/'old.inkfw',AUDIT_NEXT_PACKAGE=p/'new.inkfw')
(HERE/'update-results.json').write_text(json.dumps(dict(work=str(work),runs=results),ensure_ascii=False,indent=2)+'\n')
print('MATRIX complete',len(results),'boots; artifacts',work)
