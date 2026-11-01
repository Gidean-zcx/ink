"""One-time, preflighted merge. All proposals are validated before any batch write."""
import csv, io, json, hashlib
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/dragonbox-controls-deepening'
AUDIT=W/'applied-reviewed-updates.json'
assert not AUDIT.exists(), 'Already applied; do not rerun'
def sha(b):return hashlib.sha256(b).hexdigest()
tables={}
for pattern,key in [('*-coverage.csv','flow_id'),('*-evidence.jsonl','evidence_id')]:
 for path in sorted((L/'batches').glob(pattern)):
  raw=path.read_bytes();s=raw.decode()
  rows=list(csv.DictReader(io.StringIO(s,newline=''))) if key=='flow_id' else [json.loads(x) for x in s.splitlines() if x.strip()]
  tables[path]=dict(raw=raw,key=key,rows=rows)
proposals=[]
specs=[('dragonbox-controls-deepening/old-coverage-updates.json','flow_id',True),('dragonbox-controls-deepening/old-evidence-additional-captures.json','evidence_id',True),('hiby-controls-deepening/old-coverage-updates.json','flow_id',True),('hiby-controls-deepening/old-evidence-additional-captures.json','evidence_id',True),('anki-controls-deepening/existing-coverage-updates.json','flow_id',False),('anki-controls-deepening/additional-captures-updates.json','evidence_id',False)]
for source,key,whole in specs:
 for item in json.loads((R/'work'/source).read_text()):
  found=[]
  for path,table in tables.items():
   if table['key']!=key:continue
   for row in table['rows']:
    if row[key]!=item[key]:continue
    if whole:
     assert row==item['before'],(path,item[key],'before mismatch')
     row.clear();row.update(item['after'])
    else:
     assert row.get(item['field'])==item['before'],(path,item[key],item['field'],'before mismatch')
     row[item['field']]=item['after']
    found.append(str(path.relative_to(R)))
  assert found,('Missing',item[key])
  proposals.append(dict(source='work/'+source,whole_record=whole,key=key,updated_files=found,**item))
changed=sorted({p for x in proposals for p in x['updated_files']});prepared={};files=[]
for rel in changed:
 path=R/rel;table=tables[path]
 assert path.read_bytes()==table['raw'],('Concurrent write',rel)
 if table['key']=='flow_id':
  out=io.StringIO(newline='');writer=csv.DictWriter(out,fieldnames=list(table['rows'][0]),lineterminator='\n');writer.writeheader();writer.writerows(table['rows']);raw=out.getvalue().encode()
 else:raw=''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in table['rows']).encode()
 prepared[path]=raw;files.append(dict(path=rel,before_sha256=sha(table['raw']),after_sha256=sha(raw)))
backup=W/'reviewed-before';backup.mkdir(exist_ok=True)
for path in prepared:
 target=backup/path.name;assert not target.exists(),('Existing backup',target)
 target.write_bytes(tables[path]['raw'])
for path,raw in prepared.items():path.write_bytes(raw)
fs={x['flow_id'] for x in proposals if 'flow_id' in x};es={x['evidence_id'] for x in proposals if 'evidence_id' in x}
field_count=sum(len(x['changed_fields']) if x['whole_record'] else 1 for x in proposals if 'flow_id' in x)
report=dict(checked_on='2026-10-27',old_tasks=len(fs),old_fields=field_count,old_evidence=len(es),files=files,changes=proposals)
AUDIT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ['files','changes']},ensure_ascii=False))
