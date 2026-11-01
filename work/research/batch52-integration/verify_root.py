import csv, hashlib, json, re
from pathlib import Path
from collections import Counter
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch52-integration'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open()))
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
fs=rows(L/'coverage.csv');es=[json.loads(x) for x in (L/'evidence.jsonl').read_text().splitlines()];rp=rows(L/'reference-points.csv')
fm={x['flow_id']:x for x in fs};em={x['evidence_id']:x for x in es}
assert len(fs)==972 and len(es)==1722 and len(rp)==142
assert Counter(x['depth'] for x in fs)=={'D2':874,'D1':78,'D4':20}
assert len({e['url'] for e in es})==1550
assert sum(e.get('visual_observed') and e.get('visual_kind')!='browser_sequence' for e in es)==230
assert len({r['reference_id'] for r in rp})==142
for r in rp:
    assert len(r)==22 and all(r.values()),r['reference_id']
    ff=[fm[k] for k in r['flow_ids'].split(';')];ee=[em[k] for k in r['evidence_ids'].split(';')]
    assert all(f['product_id']==r['product_id'] for f in ff) and (L/r['artifact_path']).exists()
    expected={'task_depths':{f['flow_id']:f['depth'] for f in ff},'task_hands_on':{f['flow_id']:f['hands_on']=='true' for f in ff},'source_types':{e['evidence_id']:e['source_type'] for e in ee},'access_modes':{e['evidence_id']:e['access_mode'] for e in ee},'evidence_hands_on':{e['evidence_id']:e['hands_on'] for e in ee},'operation_methods':{e['evidence_id']:e.get('operation_method','not_recorded') for e in ee}}
    for k,v in expected.items():assert json.loads(r[k])==v
    paths=list(dict.fromkeys(e['operation_audit_path'] for e in ee if e.get('hands_on') is True and e.get('operation_audit_path')))
    assert r['operation_audit_paths']==(';'.join(paths) or 'none_for_cited_evidence')
fc=json.loads((W/'old-flow-updates.json').read_text());assert len(fc)==5
for x in fc:
    fid=x['before']['flow_id'];assert fm[fid]==x['after']
    assert x['before']['depth']==x['after']['depth']=='D2' and x['after']['hands_on']=='false'
    assert sum(any(r['flow_id']==fid for r in rows(p)) for p in (L/'batches').glob('*-coverage.csv'))==1
qq=[f for f in fs if f['product_id']=='M-021'];assert len(qq)==17
assert set(re.findall(r'F-M\d{3}-\d{2}',(L/'cases/M-021-quizlet.md').read_text()))=={f['flow_id'] for f in qq}
ec=json.loads((W/'old-evidence-updates.json').read_text());assert len(ec)==2
for x in ec:
    e=em[x['before']['evidence_id']];assert e==x['after']
    assert len(e['additional_captures'])==len(x['before'].get('additional_captures',[]))+1
    assert e['additional_captures'][:-1]==x['before'].get('additional_captures',[])
    c=e['additional_captures'][-1];assert not c['hands_on'] and not c['visual_observed'] and sha(R/c['raw_work_path'])==c['raw_sha256']
a=json.loads((L/'artifacts/quizlet-material-workflows/source-audit.json').read_text());assert len(a['sources'])==8 and not a['images_viewed'] and not a['account_used']
phrases={
'01':['Settings → Connectors → Quizlet','opened in Quizlet','Edit and personalize'],
'02':['Allow camera','Boxes will outline','at least two cards','publish or preview','swiping left'],
'03':['rich text editing or custom audio and images','parent confirm their consent'],
'04':["doesn't delete the original",'combination is not saved','only combine sets on the website'],
'05':['@Quizlet','directly in ChatGPT','Edit and personalize'],
'06':['three or more cards','default language','all other activities','only change your error checking options on the website'],
'07':['automatically saved','below the card you selected','save and publish'],
'08':['Different from','Gravity is no longer available','Write mode is now part']}
for src in a['sources']:
    e=em[src['evidence_id']];raw=R/src['raw_work_path'];txt=raw.read_text()
    assert e['url']==src['url'] and sha(raw)==src['raw_sha256']==e['raw_sha256'] and src['body_read_by_root']
    start=txt.index(src['title']+' – Quizlet Help Center (')
    section=txt[start:];sep=section.find('-----------------------------------------')
    if sep>=0:section=section[:sep]
    assert src['url'] in section
    for q in phrases[e['evidence_id'][-2:]]:assert q in section,(e['evidence_id'],q)
    assert not e['hands_on'] and not e['visual_observed'] and sha(L/e['archived_artifact'])==e['sha256']
inv=rows(L/'batches/quizlet-controls-deepening-inventory.csv');by={r['order']:r for r in inv};ic=json.loads((W/'inventory-updates.json').read_text())
assert len(inv)==68 and len(ic)==8
for x in ic:assert by[x['after']['order']]==x['after'] and x['before']['coverage']=='仅目录；正文未在本例展开'
assert by['63']['functional_domain']=='Using error checking options (UK)' and by['64']['functional_domain']=='Setting up a study path'
assert 'E-QZMW-08' in by['50']['evidence_ids'] and '不独立计任务' in by['50']['coverage']
for r in inv:
    assert all(k in fm for k in r['flow_ids'].split(';') if k) and all(k in em for k in r['evidence_ids'].split(';') if k)
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
briefs=sorted((R/'docs/briefs').glob('[0-9]*.md'));assert len(briefs)==52 and all(p.name in (R/'docs/briefs/README.md').read_text() for p in briefs)
before=json.loads((W/'before-manifest.json').read_text())
now={str(p.relative_to(R)):sha(p) for p in [R/'README.md',R/'AGENTS.md',*sorted((R/'docs').rglob('*'))] if p.is_file()}
new=sorted(set(now)-set(before));modified=sorted(k for k in set(now)&set(before) if now[k]!=before[k]);deleted=sorted(set(before)-set(now))
assert not deleted and 'AGENTS.md' not in modified and 'docs/requirements.md' not in modified
result=dict(status='passed',flows=972,evidence=1722,distinct_urls=1550,reference_points=142,new_flows=0,old_flows_updated=5,
    new_help_evidence=8,old_directory_captures=2,inventory_rows_updated=8,requirements_unchanged=True,
    new_formal_files=new,modified_formal_files=modified,deleted_formal_files=deleted,
    scope='Source identity, selected source phrases, stable IDs, derived provenance, changed rows and formal-file consistency; no actual product operation')
dump(W/'root-final-verification.json',result);dump(W/'final-manifest.json',now)
print(json.dumps({k:result[k] for k in ['status','flows','evidence','reference_points','inventory_rows_updated','requirements_unchanged']},ensure_ascii=False))
print('Formal changes:',len(new),'new',len(modified),'modified',len(deleted),'deleted')
