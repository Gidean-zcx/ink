from pathlib import Path
import csv,json,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch35-integration'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p,k):
    rs=list(csv.DictReader(p.open())) if p.suffix=='.csv' else [json.loads(s) for s in p.read_text().splitlines()]
    return {r[k]:r for r in rs}
changes=json.loads((W/'changes.json').read_text());report={}
for name,key,ck in [('coverage.csv','flow_id','old_flows'),('evidence.jsonl','evidence_id','old_evidence')]:
    old=read(W/'before/docs/research/interaction-library'/name,key);new=read(L/name,key)
    changed={i for i in old if old[i]!=new[i]};expected={r[key] for r in changes[ck]};assert changed==expected,(name,changed)
    assert all(new[r[key]]==r['after'] and old[r[key]]==r['before'] for r in changes[ck])
    newids=set(new)-set(old);assert newids==set(changes['new_flows' if name.endswith('.csv') else 'new_evidence'])
    assert set(old)<=set(new)
    report[name]=dict(total=len(new),new=len(newids),changed=sorted(changed))
es=read(L/'evidence.jsonl','evidence_id');fs=read(L/'coverage.csv','flow_id')
for i in changes['new_flows']:assert fs[i]['depth']=='D2' and fs[i]['hands_on']=='false'
raw={};assets={}
for i in changes['new_evidence']:
    e=es[i];assert e['hands_on'] is False
    for pathkey,hkey,base in [('raw_work_path','raw_sha256',R),('text_work_path','text_sha256',R),('archived_artifact','sha256',L)]:
        if e.get(pathkey):
            p=base/e[pathkey];assert sha(p)==e[hkey],(i,p)
            (assets if base==L else raw)[str(p.relative_to(R))]=e[hkey]
    for rel,digest in e.get('artifact_hashes',{}).items():
        p=L/rel;assert sha(p)==digest;assets[str(p.relative_to(R))]=digest
cap=es['E-RWCD-17']['additional_captures'][-1]
assert sha(R/cap['raw_work_path'])==cap['raw_sha256'] and sha(L/cap['archived_artifact'])==cap['sha256']
for filename,count in [('readwise-ai-workflows-inventory.csv',14),('xiaoyuan-historical-learning-inventory.csv',12)]:
    inv=list(csv.DictReader((L/'batches'/filename).open()));assert len(inv)==count
    assert len({x['entry_id'] for x in inv})==count
    for x in inv:
        assert set(x['evidence_ids'].split(';'))<=set(es)
        assert not x['flow_ids'] or set(x['flow_ids'].split(';'))<=set(fs)
for filename,count in [('readwise-controls-deepening-inventory.csv',281),('xiaoyuan-hardware-controls-deepening-inventory.csv',33)]:
    assert len(list(csv.DictReader((L/'batches'/filename).open())))==count
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
report.update(requirements_unchanged=True,raw_hashes=raw,artifact_hashes=assets,all_new_flows_D2_no_product_operation=True)
(W/'root-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ['raw_hashes','artifact_hashes']},ensure_ascii=False))
