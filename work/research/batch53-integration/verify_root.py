import csv, hashlib, json, re
from pathlib import Path
from collections import Counter
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch53-integration'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open()))
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
fs=rows(L/'coverage.csv');es=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];rp=rows(L/'reference-points.csv')
fm={f['flow_id']:f for f in fs};em={e['evidence_id']:e for e in es}
assert len(fs)==978 and len(es)==1730 and len(rp)==152
assert Counter(f['depth'] for f in fs)=={'D2':880,'D1':78,'D4':20}
assert len({e['url'] for e in es})==1558
assert sum(e.get('visual_observed') and e.get('visual_kind')!='browser_sequence' for e in es)==230
assert len({r['reference_id'] for r in rp})==152
for r in rp:
    assert len(r)==22 and all(r.values()),r['reference_id']
    ff=[fm[k] for k in r['flow_ids'].split(';')];ee=[em[k] for k in r['evidence_ids'].split(';')]
    assert all(f['product_id']==r['product_id'] for f in ff) and (L/r['artifact_path']).exists()
    expected={'task_depths':{f['flow_id']:f['depth'] for f in ff},'task_hands_on':{f['flow_id']:f['hands_on']=='true' for f in ff},'source_types':{e['evidence_id']:e['source_type'] for e in ee},'access_modes':{e['evidence_id']:e['access_mode'] for e in ee},'evidence_hands_on':{e['evidence_id']:e['hands_on'] for e in ee},'operation_methods':{e['evidence_id']:e.get('operation_method','not_recorded') for e in ee}}
    for k,v in expected.items():assert json.loads(r[k])==v
    paths=list(dict.fromkeys(e['operation_audit_path'] for e in ee if e.get('hands_on') is True and e.get('operation_audit_path')))
    assert r['operation_audit_paths']==(';'.join(paths) or 'none_for_cited_evidence')
fc=json.loads((W/'old-flow-updates.json').read_text());assert len(fc)==3
for x in fc:
    fid=x['before']['flow_id'];assert fm[fid]==x['after']
    assert x['before']['depth']==x['after']['depth']=='D2' and x['after']['hands_on']=='false'
    assert sum(any(r['flow_id']==fid for r in rows(p)) for p in (L/'batches').glob('*-coverage.csv'))==1
new=rows(L/'batches/quizlet-audio-and-study-coverage.csv');assert len(new)==6
assert [f['flow_id'] for f in new]==[f'F-M021-{i:02d}' for i in range(18,24)]
for pid,count in [('M-021',23),('M-030',16)]:
    subset=[f for f in fs if f['product_id']==pid];assert len(subset)==count and all(f['depth']=='D2' and f['hands_on']=='false' for f in subset)
    assert set(re.findall(r'F-M\d{3}-\d{2}',(L/subset[0]['case_path']).read_text()))=={f['flow_id'] for f in subset}
ec=json.loads((W/'old-evidence-updates.json').read_text());assert len(ec)==2
for x in ec:
    e=em[x['before']['evidence_id']];assert e==x['after']
    assert len(e['additional_captures'])==len(x['before'].get('additional_captures',[]))+1
    assert e['additional_captures'][:-1]==x['before'].get('additional_captures',[])
    c=e['additional_captures'][-1];assert not c['hands_on'] and not c['visual_observed'] and sha(R/c['raw_work_path'])==c['raw_sha256']
    if c.get('archived_artifact'):assert sha(L/c['archived_artifact'])==c['sha256']
    for p,h in c.get('artifact_hashes',{}).items():assert sha(L/p)==h
a=json.loads((L/'artifacts/quizlet-audio-and-study/source-audit.json').read_text());assert len(a['sources'])==8 and not a['account_used'] and not a['images_viewed']
phrases={
'01':["doesn't correct errors in your source material",'Your Library','share public podcasts'],
'02':['5 hours long','500 MB','draft on your device','Continue uploading','Create a new Study Guide',"Practice Tests can't be generated from audio"],
'03':['minimizing it will keep your chat active','conversation will reset','14 years old or older','3 files','25 MB'],
'04':['one session without creating an account','latest results','No one outside the session','Match: A leaderboard with scores'],
'05':['at least six terms','three or more charms',"Incorrect answers don't earn additional moves",'until you run out of moves'],
'06':['authorize access to your Quizlet account','eligible open tabs'],
'07':['written and verified by experts','Explain this step','Generate a practice problem',"solutions you've viewed recently"],
'08':['Recent sets','Created','All sets containing the term or phrase','only search your sets by keyword on the website']}
for src in a['sources']:
    e=em[src['evidence_id']];raw=R/src['raw_work_path'];txt=raw.read_text()
    assert e['url']==src['url'] and sha(raw)==src['raw_sha256']==e['raw_sha256'] and src['body_read_by_root']
    section=txt[txt.index(src['title']+' – Quizlet Help Center ('):]
    end=section.find('-----------------------------------------')
    if end>=0:section=section[:end]
    assert src['url'] in section
    for q in phrases[e['evidence_id'][-2:]]:assert q in section,(e['evidence_id'],q)
    assert not e['hands_on'] and not e['visual_observed'] and sha(L/e['archived_artifact'])==e['sha256']
    for p,h in e['artifact_hashes'].items():assert sha(L/p)==h
k=json.loads((L/'artifacts/khan-intro-route-audit/source-audit.json').read_text())
assert not k['visual_observed'] and not k['video_played'] and not k['video_downloaded'] and not k['hands_on']
assert k['image_attempts']['results']==[403,403,403] and k['image_attempts']['route_stopped']
for x in [k['page'],k['image_attempts']]:assert sha(R/x['raw_work_path'])==x['sha256']
assert (W/'khan-image-attempts.txt').read_text().count('(403) Forbidden')==3
assert 'opened_module_text' in next(x['before']['access_mode'] for x in ec if x['before']['evidence_id']=='E-KHD-04')
inv=rows(L/'batches/quizlet-controls-deepening-inventory.csv');by={r['order']:r for r in inv};ic=json.loads((W/'inventory-updates.json').read_text())
assert len(inv)==68 and len(ic)==11 and Counter(x['kind'] for x in ic)=={'new_body':8,'stale_status_correction':3}
for x in ic:
    assert by[x['after']['order']]==x['after'] and x['before']['coverage']=='仅目录；正文未在本例展开'
studying=[r for r in inv if r['parent_order']=='38'];assert len(studying)==27
for r in studying:
    assert '仅目录' not in r['coverage'] and r['article_url'] and any('index' not in em[e]['source_type'] for e in r['evidence_ids'].split(';'))
assert sum(r['entry_kind']=='child_topic' and '仅目录' in r['coverage'] for r in inv)==21
for r in inv:assert all(k in fm for k in r['flow_ids'].split(';') if k) and all(k in em for k in r['evidence_ids'].split(';') if k)
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
briefs=sorted((R/'docs/briefs').glob('[0-9]*.md'));assert len(briefs)==53 and all(p.name in (R/'docs/briefs/README.md').read_text() for p in briefs)
cleanup=json.loads((W/'cache-cleanup.json').read_text());profile=R/'work/research/batch38-week/desktop-profile'
assert not (R/cleanup['target']).exists() and not cleanup['active_browser_processes'] and cleanup['retained_profile_unchanged'] and not cleanup['formal_evidence_deleted']
assert all(sha(profile/p)==h for p,h in cleanup['retained_profile_files'].items())
check=json.loads((W/'final-check.json').read_text());assert check['errors']==[] and check['flows']==978 and check['evidence_records']==1730
before=json.loads((W/'before-manifest.json').read_text());now={str(p.relative_to(R)):sha(p) for p in [R/'README.md',R/'AGENTS.md',*sorted((R/'docs').rglob('*'))] if p.is_file()}
newfiles=sorted(set(now)-set(before));modified=sorted(k for k in set(now)&set(before) if now[k]!=before[k]);deleted=sorted(set(before)-set(now))
assert not deleted and 'AGENTS.md' not in modified and 'docs/requirements.md' not in modified
result=dict(status='passed',flows=978,evidence=1730,distinct_urls=1558,reference_points=152,new_flows=6,old_flows_updated=3,new_help_evidence=8,old_captures=2,
    inventory_rows_updated=11,new_body_rows=8,old_game_rows_corrected=3,studying_articles_with_body_records=27,other_index_only_article_rows=21,
    requirements_unchanged=True,khan_images_unobserved=True,cache_only_cleanup_retained_profile_unchanged=True,
    new_formal_files=newfiles,modified_formal_files=modified,deleted_formal_files=deleted,
    scope='Source identity and selected text claims, F/E/RP consistency, index corrections, failed image route and formal-file identity; no actual product operation')
dump(W/'root-final-verification.json',result);dump(W/'final-manifest.json',now)
print(json.dumps({k:result[k] for k in ['status','flows','evidence','reference_points','studying_articles_with_body_records','requirements_unchanged']},ensure_ascii=False))
print('Formal changes:',len(newfiles),'new',len(modified),'modified',len(deleted),'deleted')
