from pathlib import Path
import csv,json,hashlib,re
from urllib.parse import unquote
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch33-closeout'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open(encoding='utf-8-sig')))
F={x['flow_id']:x for x in rows(L/'coverage.csv')}
E={x['evidence_id']:x for x in map(json.loads,(L/'evidence.jsonl').read_text().splitlines())}
N=[json.loads(x) for x in (L/'batches/family-audio-and-voice-controls-evidence.jsonl').read_text().splitlines()]
NF=rows(L/'batches/family-audio-and-voice-controls-coverage.csv')
assert len(N)==29 and len(NF)==20
for e in N:assert E[e['evidence_id']]==e
for f in NF:assert F[f['flow_id']]==f and f['depth']=='D2' and f['hands_on']=='false'
changes=json.loads((W/'coverage-updates-applied.json').read_text())
assert len(changes)==4
assert sum(sum(c['before'][k]!=c['after'][k] for k in c['before']) for c in changes)==12
for c in changes:assert F[c['flow_id']]==c['after']
captures=json.loads((W/'evidence-updates-applied.json').read_text())
assert len(captures)==4
for c in captures:assert E[c['evidence_id']]==c['after']
hashed={}
for d in N+[c['after']['additional_captures'][-1] for c in captures]:
    for pathfield,digestfield,base in [('raw_work_path','raw_sha256',R),('extracted_work_path','extracted_sha256',R),('archived_artifact','sha256',L)]:
        if d.get(pathfield):
            p=base/d[pathfield];assert sha(p)==d[digestfield],str(p);hashed[str(p.relative_to(R))]=d[digestfield]
inventories=[]
for d in json.loads((W/'inventory-updates-applied.json').read_text()):
    p=R/d['path'];before=rows(W/'before'/d['path']);after=rows(p)
    b={x['inventory_id']:x for x in before};a={x['inventory_id']:x for x in after}
    changed=[k for k in b if b[k]!=a[k]];added=sorted(set(a)-set(b))
    assert len(changed)==d['old_rows_updated'],(p,len(changed),d)
    assert len(added)==d['new_rows']
    inventories.append(dict(path=d['path'],rows=len(after),changed=len(changed),new=len(added)))
assert sum(x['changed'] for x in inventories)==30
assert sum(x['new'] for x in inventories)==1
audible=rows(L/'batches/audible-controls-deepening-inventory.csv')
assert not any(x['coverage_status']=='index_only' for x in audible)
assert sum(x['coverage_status']=='body_read_scope_only' for x in audible)==2
p=L/'inventories/README.md'
paths={(p.parent/unquote(t)).resolve() for t in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)',p.read_text())}
count=0;used_e=set();used_f=set()
for p in paths:
    rr=rows(p);count+=len(rr)
    for row in rr:
        text=' '.join(str(v) for v in row.values())
        es=set(re.findall(r'E-[A-Z]+-\d{2}',text));fs=set(re.findall(r'F-M\d{3}-\d{2}',text))
        assert es<=set(E),(p,es-set(E));assert fs<=set(F),(p,fs-set(F));used_e|=es;used_f|=fs
assert len(paths)==77 and count==7325,(len(paths),count)
requirements=sha(R/'docs/requirements.md')
assert requirements=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
result=dict(status='PASS',new_evidence=29,new_flows=20,old_flows=4,old_fields=12,old_evidence_captures=4,unique_files_hash_checked=len(hashed),inventory_changes=inventories,linked_inventory_csvs=len(paths),linked_inventory_rows=count,distinct_inventory_e=len(used_e),distinct_inventory_f=len(used_f),requirements_sha256=requirements)
(W/'post-merge-verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
(W/'verified-source-hashes.json').write_text(json.dumps(hashed,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(result,ensure_ascii=False))
