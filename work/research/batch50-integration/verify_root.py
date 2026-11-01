import csv,hashlib,json,re
from pathlib import Path
from collections import Counter
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch50-integration'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open()))
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
fs=rows(L/'coverage.csv');es=[json.loads(l) for l in (L/'evidence.jsonl').read_text().splitlines()];rs=rows(L/'reference-points.csv')
fm={r['flow_id']:r for r in fs};em={r['evidence_id']:r for r in es}
assert len(fs)==967 and len(es)==1713 and len(rs)==122
assert Counter(x['depth'] for x in fs)=={'D2':869,'D1':78,'D4':20}
assert len({x['url'] for x in es})==1542
assert len({x['reference_id'] for x in rs})==122
audit_refs=0
for r in rs:
    assert len(r)==22 and all(r.values()),r['reference_id']
    f=[fm[k] for k in r['flow_ids'].split(';')];e=[em[k] for k in r['evidence_ids'].split(';')]
    assert all(x['product_id']==r['product_id'] for x in f)
    assert (L/r['artifact_path']).exists()
    expected={
        'task_depths':{x['flow_id']:x['depth'] for x in f},
        'task_hands_on':{x['flow_id']:x['hands_on']=='true' for x in f},
        'source_types':{x['evidence_id']:x['source_type'] for x in e},
        'access_modes':{x['evidence_id']:x['access_mode'] for x in e},
        'evidence_hands_on':{x['evidence_id']:x['hands_on'] for x in e},
        'operation_methods':{x['evidence_id']:x.get('operation_method','not_recorded') for x in e}}
    for k,v in expected.items():assert json.loads(r[k])==v,(r['reference_id'],k)
    paths=list(dict.fromkeys(x['operation_audit_path'] for x in e if x.get('hands_on') is True and x.get('operation_audit_path')))
    assert r['operation_audit_paths']==(';'.join(paths) or 'none_for_cited_evidence')
    for p in paths:assert (L/p).exists();audit_refs+=1
for pid,n in [('M-042',13),('M-060',17)]:
    subset=[x for x in fs if x['product_id']==pid];assert len(subset)==n
    assert all(x['depth']=='D2' and x['hands_on']=='false' for x in subset)
    case=(L/subset[0]['case_path']).read_text()
    assert set(re.findall(r'F-M\d{3}-\d{2}',case))=={x['flow_id'] for x in subset}
for x in json.loads((W/'old-flow-updates.json').read_text()):
    assert fm[x['before']['flow_id']]==x['after'] and x['before']['depth']==x['after']['depth']
    assert x['after']['evidence_ids'].startswith(x['before']['evidence_ids'])
for fid in ['F-M042-02','F-M042-10','F-M042-12','F-M042-13','F-M060-11','F-M060-12','F-M060-17']:
    owners=[p for p in (L/'batches').glob('*-coverage.csv') if any(r['flow_id']==fid for r in rows(p))]
    assert len(owners)==1,(fid,owners)
for eid in ['E-GENR-01','E-GENR-02','E-QGNT-01']:
    e=em[eid];assert not e['hands_on'] and not e['visual_observed']
    assert sha(R/e['raw_work_path'])==e['raw_sha256']
    assert sha(L/e['archived_artifact'])==e['sha256']
    for rel,h in e['artifact_hashes'].items():assert sha(L/rel)==h
texts=[(R/em[eid]['raw_work_path']).read_text() for eid in ['E-GENR-01','E-GENR-02']]
for t in texts:
    assert 'defaults to the last one it accessed' in t and 'background' in t and 'Undo' in t
assert 'View images and drawings' in texts[0] and 'Delete my tasks' in texts[1]
ga=json.loads((L/'artifacts/gemini-notes-reminders/source-audit.json').read_text());assert len(ga['captures'])==2 and len(ga['failed_sources'])==2
q=json.loads((L/'artifacts/quaderno-gen2-ink/source-audit.json').read_text())
assert q['read_pages']==[1,27,*range(51,58),124] and len(q['remaining_pages_not_recorded_as_read'])==114 and q['identity_only_pages']==[120]
assert sha(R/q['text_work_path'])==q['text_sha256']
qt=(R/q['text_work_path']).read_text()
for phrase in ['ペン先を離すと線が表示','ページを移動するとリセット','貼り付けの操作は、1回のみ','白黒の濃淡','ご購入時の設定は有効']:assert phrase in qt,phrase
inv=rows(L/'batches/gemini-controls-deepening-inventory.csv');assert len(inv)==133 and len({x['entry_id'] for x in inv})==133
assert sum(x['entry_type']=='connector_directory_entry' for x in inv)==85
for x in json.loads((W/'gemini-inventory-updates.json').read_text()):assert next(r for r in inv if r['entry_id']==x['entry_id'])==x['after']
qi=rows(L/'batches/quaderno-gen2-ink-inventory.csv');assert len(qi)==197
for x in qi:
    assert x['evidence_ids']=='E-QGNT-01'
    assert all(k in fm for k in x['flow_ids'].split(';') if k)
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
briefs=sorted((R/'docs/briefs').glob('[0-9]*.md'));assert len(briefs)==50
assert all(p.name in (R/'docs/briefs/README.md').read_text() for p in briefs)
assert '任务研究深度' in (L/'case-index.md').read_text()
before=json.loads((W/'before-manifest.json').read_text())
now={str(p.relative_to(R)):sha(p) for p in [R/'README.md',R/'AGENTS.md',*sorted((R/'docs').rglob('*'))] if p.is_file()}
new=sorted(set(now)-set(before));modified=sorted(k for k in set(now)&set(before) if now[k]!=before[k]);deleted=sorted(set(before)-set(now))
assert not deleted and 'docs/requirements.md' not in modified and 'AGENTS.md' not in modified
result=dict(status='passed',flows=len(fs),evidence=len(es),reference_points=len(rs),reference_columns=22,reference_audit_links=audit_refs,new_evidence=3,new_flows=3,old_flows_updated=4,new_reference_points=8,gen2_read_pages=q['read_pages'],gen2_identity_only_pages=q['identity_only_pages'],requirements_unchanged=True,new_formal_files=new,modified_formal_files=modified,deleted_formal_files=deleted,scope='Source identity and selected text claims plus references/schema; no UI/device execution or learning result verified')
dump(W/'root-final-verification.json',result);dump(W/'final-manifest.json',now)
print(json.dumps({k:result[k] for k in ['status','flows','evidence','reference_points','reference_columns','reference_audit_links','requirements_unchanged']},ensure_ascii=False))
print('Formal changes:',len(new),'new',len(modified),'modified',len(deleted),'deleted')
