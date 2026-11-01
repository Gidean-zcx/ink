import csv,json,hashlib
from pathlib import Path
root=Path('/home/workspace/reader');lib=root/'docs/research/interaction-library';w=root/'work/inoreader-controls-deepening';audit=w/'applied-existing-updates.json'
assert not audit.exists(),'Already applied; do not rerun'
updates=[]
for name in ['inoreader','quizlet']:updates+=list(csv.DictReader((root/f'work/{name}-controls-deepening/existing-coverage-updates.csv').open()))
for group in json.loads((root/'work/audible-controls-deepening/old-coverage-updates.json').read_text()):
 for field,x in group['changes'].items():updates.append(dict(flow_id=group['flow_id'],field=field,**x,reason='根抽核核心原文；D2未实操'))
files={p:list(csv.DictReader(p.open())) for p in (lib/'batches').glob('*-coverage.csv')};loc={}
for p,rows in files.items():
 for row in rows:loc.setdefault(row['flow_id'],[]).append((p,row))
changed=set()
for u in updates:
 assert len(loc[u['flow_id']])==1; p,r=loc[u['flow_id']][0];assert r[u['field']]==u['before'],(u['flow_id'],u['field']);changed.add(p)
before={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in changed}
for u in updates:loc[u['flow_id']][0][1][u['field']]=u['after']
for p in changed:
 with p.open('w',newline='') as f:
  writer=csv.DictWriter(f,fieldnames=list(files[p][0]),lineterminator='\n');writer.writeheader();writer.writerows(files[p])
# Retain the historical source note, add an explicit later curation correction.
p=lib/'batches/initial-evidence.jsonl';rs=[json.loads(x) for x in p.read_text().splitlines()];r=next(x for x in rs if x['evidence_id']=='E-INO-02');old=dict(r)
assert 'curation_note' not in r
r['curation_note']='2026-10-27：E-INCD-06帮助明确Run rule会执行邮件/导出等动作；本条原页preview措辞不能当无副作用保证，见F-M044-02修订。';r['curation_checked_on']='2026-10-27'
p.write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in rs))
audit.write_text(json.dumps(dict(before_hashes=before,updates=updates,evidence_before=old,evidence_after=r,after_hashes={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in changed}),ensure_ascii=False,indent=2)+'\n')
print(json.dumps(dict(old_flows=len({x['flow_id'] for x in updates}),field_changes=len(updates),coverage_batches=len(changed),source_curation=1),ensure_ascii=False))
