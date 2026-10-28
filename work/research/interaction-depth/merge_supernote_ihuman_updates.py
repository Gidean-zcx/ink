import csv,json
from pathlib import Path
root=Path('/home/workspace/reader'); lib=root/'docs/research/interaction-library'
updates={}
for filename,key in [('work/supernote-controls-deepening/old-coverage-updates.json','changes'),('work/ihuman-controls-deepening/existing-coverage-field-changes.json','field_updates')]:
    for item in json.loads((root/filename).read_text()):
        assert item['flow_id'] not in updates
        updates[item['flow_id']]=item[key]
seen=set(); changed=[]
for path in (lib/'batches').glob('*-coverage.csv'):
    rows=list(csv.DictReader(path.open())); dirty=False
    for row in rows:
        fid=row['flow_id']
        if fid not in updates: continue
        assert fid not in seen
        for key,value in updates[fid].items():
            assert row[key]==value['before'],(fid,key,row[key],value['before'])
        for key,value in updates[fid].items(): row[key]=value['after']
        dirty=True;seen.add(fid)
    if dirty:
        with path.open('w',newline='') as f:
            w=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');w.writeheader();w.writerows(rows)
        changed.append(str(path.relative_to(root)))
assert seen==set(updates)
print(json.dumps({'updated':len(seen),'batches':changed}))
