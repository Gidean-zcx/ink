from pathlib import Path
import csv,io,json,hashlib
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/sony-dpt-controls-deepening'
audit=W/'applied-reviewed-updates.json';assert not audit.exists(),'one-time merge already applied'
def read(p):return json.loads((R/p).read_text())
def sha(b):return hashlib.sha256(b).hexdigest()
flow_changes=[]
for item in read('work/sony-dpt-controls-deepening/existing-coverage-updates.json'):
 for k,v in item['after'].items():
  if item['before'][k]!=v:flow_changes.append({'id':item['flow_id'],'field':k,'before':item['before'][k],'after':v})
for item in read('work/quaderno-controls-deepening/old-coverage-updates.json'):
 for k,v in item['changed_fields'].items():flow_changes.append({'id':item['flow_id'],'field':k,**v})
for item in read('work/huawei-controls-deepening/existing-coverage-updates.json'):
 flow_changes.append({'id':item['flow_id'],**{k:item[k] for k in ['field','before','after']}})
evidence_changes=[]
for item in read('work/quaderno-controls-deepening/old-evidence-date-corrections.json'):
 for k,v in item['changed_fields'].items():evidence_changes.append({'id':item['evidence_id'],'field':k,**v})
for source in ['work/quaderno-controls-deepening/old-evidence-additional-captures.json','work/huawei-controls-deepening/additional-captures-updates.json']:
 for item in read(source):evidence_changes.append({'id':item['evidence_id'],'field':'additional_captures','before':item['before'],'after':item['after']})
csv_files={p:list(csv.DictReader(p.open())) for p in (L/'batches').glob('*-coverage.csv')}
json_files={p:[json.loads(x) for x in p.read_text().splitlines() if x] for p in (L/'batches').glob('*-evidence.jsonl')}
fi={x['flow_id']:(p,x) for p,rows in csv_files.items() for x in rows};ei={x['evidence_id']:(p,x) for p,rows in json_files.items() for x in rows}
for item in read('work/sony-dpt-controls-deepening/additional-captures-updates.json'):
 prev=ei[item['evidence_id']][1].get('additional_captures')
 evidence_changes.append({'id':item['evidence_id'],'field':'additional_captures','before':prev,'after':(prev or [])+[item['capture']]})
changed=set()
# All assertions precede any filesystem mutation.
for changes,idx in [(flow_changes,fi),(evidence_changes,ei)]:
 for c in changes:
  p,row=idx[c['id']];actual=row.get(c['field'])
  if c['field']=='additional_captures' and c['before']==[] and actual is None:actual=[]
  assert actual==c['before'],(c['id'],c['field'],actual,c['before'])
  row[c['field']]=c['after'];changed.add(p)
prepared={}
for p in changed:
 if p.suffix=='.csv':
  rows=csv_files[p];s=io.StringIO(newline='');w=csv.DictWriter(s,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows);data=s.getvalue().encode()
 else:data=''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in json_files[p]).encode()
 prepared[p]=data
backup=W/'reviewed-before';backup.mkdir(exist_ok=False);file_records=[]
for p,data in prepared.items():
 old=p.read_bytes();bp=backup/p.name;bp.write_bytes(old)
 file_records.append({'path':str(p.relative_to(R)),'backup':str(bp.relative_to(R)),'before_sha256':sha(old),'after_sha256':sha(data)})
for p,data in prepared.items():p.write_bytes(data)
report={'flow_changes':flow_changes,'evidence_changes':evidence_changes,'files':file_records,
 'old_tasks':len({x['id'] for x in flow_changes}),'old_task_fields':len(flow_changes),
 'old_evidence_captures':len([x for x in evidence_changes if x['field']=='additional_captures']),
 'old_evidence_metadata_fields':len([x for x in evidence_changes if x['field']!='additional_captures'])}
audit.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k.startswith('old_')},ensure_ascii=False))
