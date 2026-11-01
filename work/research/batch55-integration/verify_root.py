import csv,gzip,hashlib,json,re
from collections import Counter
from pathlib import Path
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch55-integration'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open()))
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
fs=rows(L/'coverage.csv');es=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];rp=rows(L/'reference-points.csv')
fm={f['flow_id']:f for f in fs};em={e['evidence_id']:e for e in es}
assert len(fs)==996 and len(es)==1797 and len(rp)==179
assert Counter(f['depth'] for f in fs)=={'D2':897,'D1':79,'D4':20}
assert len({e['url'] for e in es})==1625
assert sum(e.get('visual_observed') and e.get('visual_kind')!='browser_sequence' for e in es)==230
assert sum(e.get('hands_on') is True for e in es)==21
assert len({r['reference_id'] for r in rp})==179
for r in rp:
    assert len(r)==22 and all(r.values()),r['reference_id']
    ff=[fm[k] for k in r['flow_ids'].split(';')];ee=[em[k] for k in r['evidence_ids'].split(';')]
    assert all(f['product_id']==r['product_id'] for f in ff) and (L/r['artifact_path']).exists()
    expected={'task_depths':{f['flow_id']:f['depth'] for f in ff},'task_hands_on':{f['flow_id']:f['hands_on']=='true' for f in ff},'source_types':{e['evidence_id']:e['source_type'] for e in ee},'access_modes':{e['evidence_id']:e['access_mode'] for e in ee},'evidence_hands_on':{e['evidence_id']:e['hands_on'] for e in ee},'operation_methods':{e['evidence_id']:e.get('operation_method','not_recorded') for e in ee}}
    for k,v in expected.items():assert json.loads(r[k])==v
    paths=list(dict.fromkeys(e['operation_audit_path'] for e in ee if e.get('hands_on') is True and e.get('operation_audit_path')))
    assert r['operation_audit_paths']==(';'.join(paths) or 'none_for_cited_evidence')
fc=json.loads((W/'old-flow-updates.json').read_text());assert len(fc)==10
for x in fc:
    f=x['after'];assert fm[f['flow_id']]==f and f['depth']==x['before']['depth']=='D2'
    assert sum(any(r['flow_id']==f['flow_id'] for r in rows(p)) for p in (L/'batches').glob('*-coverage.csv'))==1
new=rows(L/'batches/shortcuts-entry-and-organization-coverage.csv')
assert [f['flow_id'] for f in new]==[f'F-M043-{i:02d}' for i in range(19,28)]
subset=[f for f in fs if f['product_id']=='M-043'];assert len(subset)==27 and all(f['depth']=='D2' and f['hands_on']=='false' for f in subset)
case=(L/'cases/M-043-shortcuts.md').read_text()
assert set(re.findall(r'F-M\d{3}-\d{2}',case))=={f['flow_id'] for f in subset}
assert '范围差异尚未统一' in case and 'E-SCEN-03' in case
a=json.loads((L/'artifacts/shortcuts-entry-and-organization/fetch-audit.json').read_text())
assert len(a['sources'])==44 and len(a['auxiliary_sources'])==2
assert not any(a[k] for k in ['images_viewed','hands_on','account_used','api_requests_executed'])
all_sources=a['sources']+a['auxiliary_sources']
for s in all_sources:
    raw=R/s['raw_work_path'];decoded=gzip.decompress(raw.read_bytes());txt=R/s['text_work_path']
    assert sha(raw)==s['raw_sha256'] and hashlib.sha256(decoded).hexdigest()==s['decompressed_sha256']
    assert sha(txt)==s['text_sha256'] and s['status']==200 and s['body_read_by_root']
    assert s['selected_versions'][0]['text']=='iOS 27' and '/10.0/ios/27' in s['selected_versions'][0]['url']
    if s['key']!='01':assert s['body_id'].upper()==s['upstream_id'] and s['body_id'] in decoded.decode()
    else:assert 'article-section' not in decoded.decode() and 'What’s a shortcut?' in txt.read_text()
    if 'evidence_id' in s:
        e=em[s['evidence_id']];assert e['url']==s['final_url'] and e['raw_sha256']==s['raw_sha256'] and e['decompressed_sha256']==s['decompressed_sha256']
        assert not e['hands_on'] and not e['visual_observed'] and sha(L/e['archived_artifact'])==e['sha256']
        for p,h in e['artifact_hashes'].items():assert sha(L/p)==h
        assert all(f in fm for f in s['flow_ids'])
# 定点复核影响本批判断的原文；不执行任何示例或把字符串检查当产品运行。
phrases={
'04':['rich preview','Cancel a shortcut'],
'05':['Siri asks to unlock','instead of the standard command'],
'09':['progress indicator','automatically opens','shortcut pauses'],
'12':['Show in Share Sheet','appropriate content to pass'],
'13':['only shortcuts that you enable','Show on Apple Watch'],
'22':['Unable to Load','run out of memory','Local Only','Expire At'],
'24':['Remove eliminates','keeps the shortcut','Keep Shortcuts','Delete Shortcuts'],
'29':['copy of the selected shortcut','other devices'],
'30':['default Reminders list','tap the Shortcuts icon'],
'31':['If no input is passed','retrieve content on their own'],
'33':['webpages can contain images','View Content Graph'],
'35':['shortcuts://create-shortcut','shortcuts://open-shortcut?name='],
'39':['first Split Text action','second Split Text action','Show Compose Sheet','will create contacts'],
'40':['number of seconds','00:00:00'],
'42':['may vary by locale','None'],
'43':['when you run the shortcut'],
'44':['RFC 2822','RFC 2282'],
'45':['Format String','Unicode Technical Standard #35'],
}
for s in all_sources:
    for q in phrases.get(s['key'],[]):assert q in (R/s['text_work_path']).read_text(),(s['key'],q)
for s in a['existing_comparison_sources']:
    assert sha(R/s['raw_work_path'])==s['raw_sha256'] and sha(R/s['extracted_work_path'])==s['extracted_sha256']
    assert 'outside of the shortcut editor, no preview' in (R/s['extracted_work_path']).read_text()
ec=json.loads((W/'old-evidence-updates.json').read_text());assert len(ec)==1
e=em['E-SCCD-01'];assert e==ec[0]['after'] and e['additional_captures'][:-2]==ec[0]['before'].get('additional_captures',[])
for c in e['additional_captures'][-2:]:assert sha(R/c['raw_work_path'])==c['raw_sha256'] and not c['hands_on'] and not c['visual_observed']
ic=json.loads((W/'inventory-updates.json').read_text());inv=rows(L/'batches/shortcuts-controls-deepening-inventory.csv');im={r['entry_id']:r for r in inv}
assert len(ic)==46 and len(inv)==114
assert Counter(r['entry_type'] for r in inv)=={'topic':95,'group':19}
assert Counter(r['review_status'] for r in inv)=={'body_read_documented_not_operated':93,'reference_body_read_no_flow':2,'directory_only':19}
for x in ic:assert im[x['before']['entry_id']]==x['after'] and x['before']['review_status']=='directory_only'
for r in inv:
    assert all(k in em for k in r['evidence_id'].split(';') if k) and all(k in fm for k in r['related_flow_ids'].split(';') if k)
rc=json.loads((W/'old-reference-updates.json').read_text());assert len(rc)==1
current=next(r for r in rp if r['reference_id']=='RP-M043-10')
for k in list(current)[:15]:assert current[k]==rc[0]['after'][k]
assert 'E-SCEN-03' in current['evidence_ids'] and '范围' in current['unknown_branches']
c=json.loads((W/'cache-cleanup.json').read_text())
assert len(c['removed_files'])==610 and c['deleted_bytes']==10125501 and not c['formal_evidence_deleted']
assert c['sources_unchanged'] and c['retained_files_unchanged']
for f in c['removed_files']:assert not (R/f['cache']).exists() and sha(R/f['source'])==f['source_sha256']
for p,h in c['retained_package_files'].items():assert sha(R/p)==h
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
briefs=sorted((R/'docs/briefs').glob('[0-9]*.md'));assert len(briefs)==55 and all(p.name in (R/'docs/briefs/README.md').read_text() for p in briefs)
check=json.loads((W/'final-check.json').read_text());assert check['errors']==[] and check['flows']==996 and check['evidence_records']==1797
before=json.loads((W/'before-manifest.json').read_text());now={str(p.relative_to(R)):sha(p) for p in [R/'README.md',R/'AGENTS.md',*sorted((R/'docs').rglob('*'))] if p.is_file()}
newfiles=sorted(set(now)-set(before));modified=sorted(k for k in set(now)&set(before) if now[k]!=before[k]);deleted=sorted(set(before)-set(now))
assert not deleted and 'AGENTS.md' not in modified and 'docs/requirements.md' not in modified
result=dict(status='passed',flows=996,evidence=1797,distinct_urls=1625,reference_points=179,new_flows=9,new_evidence=44,old_flows_updated=10,inventory_rows_updated=46,original_topics_with_body_record=95,old_reference_corrected='RP-M043-10',new_actual_operations=0,requirements_unchanged=True,new_formal_files=newfiles,modified_formal_files=modified,deleted_formal_files=deleted,scope='Source identity, selected claims and document/index consistency only; no product execution or learning-effect validation')
dump(W/'root-final-verification.json',result);dump(W/'final-manifest.json',now)
print(json.dumps({k:result[k] for k in ['status','flows','evidence','reference_points','original_topics_with_body_record','requirements_unchanged']},ensure_ascii=False))
print('Formal files:',len(newfiles),'new',len(modified),'modified',len(deleted),'deleted')
