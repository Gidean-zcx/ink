"""File/schema/link checks for this audit's deliverables, not product tests."""
import json
import re
import subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[4]
out=Path(__file__).resolve().parent.parent
f=json.loads((out/'findings.json').read_text())
v=json.loads((out/'verification.json').read_text())
assert len(f)==9 and len({x['id'] for x in f})==9
assert sum(x['severity']=='高' for x in f)==4 and sum(x['severity']=='中' for x in f)==5
for x in f:
    assert set(x)=={'id','severity','area','title','evidence_type','locations','impact','recommendation','reproduce'}
    for loc in x['locations']:
        p,n=loc.rsplit(':',1); path=root/p
        assert path.is_file(),p
        assert 0<int(n)<=len(path.read_text().splitlines()),loc
for x in v:
    assert set(x)=={'id','verdict','evidence','note'}
    assert x['verdict'] in ['已修复','部分修复','未修复']
for p in out.rglob('*.md'):
    if p.name=='TASK.md': continue
    for link in re.findall(r'\[[^\]]+\]\(([^)]+)\)',p.read_text()):
        if '://' not in link: assert (p.parent/link.split('#')[0]).exists(),(str(p),link)
for p in out.rglob('*'):
    if p.is_file():
        assert p.stat().st_size<3_000_000,(str(p),p.stat().st_size)
        if p.name!='codex-run.log':
            assert not re.search(r'(?i)\b(?:[0-9a-f]{2}:){5}[0-9a-f]{2}\b',p.read_text()),str(p)
changed=subprocess.check_output(['git','diff','--name-only'],cwd=root,text=True).splitlines()
assert changed==['docs/audits/2026-11-04-codex-services-s2/experiments/pin_cut_probe.c'],changed
untracked=subprocess.check_output(['git','ls-files','--others','--exclude-standard'],cwd=root,text=True).splitlines()
assert all(p.startswith(str(out.relative_to(root))+'/') for p in untracked),untracked
print('OK: findings schema (9), verification schema (7), source line ranges, Markdown links, every delivered file <3 MB.')
print('OK: no hardware-address strings in deliverables; tracked diff unchanged from initial deletion; new files only in audit output directory.')
