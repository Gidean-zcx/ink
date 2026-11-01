import csv,hashlib,json,re
from pathlib import Path
from collections import Counter
R=Path('/home/workspace/reader');L=R/'docs/research/interaction-library';W=R/'work/research/batch51-integration'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open()))
def dump(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n')
fs=rows(L/'coverage.csv');es=[json.loads(s) for s in (L/'evidence.jsonl').read_text().splitlines()];rp=rows(L/'reference-points.csv')
fm={x['flow_id']:x for x in fs};em={x['evidence_id']:x for x in es}
assert len(fs)==972 and len(es)==1714 and len(rp)==135
assert Counter(f['depth'] for f in fs)=={'D2':874,'D1':78,'D4':20}
assert len({e['url'] for e in es})==1542
assert sum(e.get('visual_observed') and e.get('visual_kind')!='browser_sequence' for e in es)==230
assert len({r['reference_id'] for r in rp})==135
for r in rp:
 assert len(r)==22 and all(r.values()),r['reference_id']
 ff=[fm[k] for k in r['flow_ids'].split(';')];ee=[em[k] for k in r['evidence_ids'].split(';')]
 assert all(f['product_id']==r['product_id'] for f in ff) and (L/r['artifact_path']).exists()
 expected={'task_depths':{f['flow_id']:f['depth'] for f in ff},'task_hands_on':{f['flow_id']:f['hands_on']=='true' for f in ff},'source_types':{e['evidence_id']:e['source_type'] for e in ee},'access_modes':{e['evidence_id']:e['access_mode'] for e in ee},'evidence_hands_on':{e['evidence_id']:e['hands_on'] for e in ee},'operation_methods':{e['evidence_id']:e.get('operation_method','not_recorded') for e in ee}}
 for k,v in expected.items():assert json.loads(r[k])==v
 paths=list(dict.fromkeys(e['operation_audit_path'] for e in ee if e.get('hands_on') is True and e.get('operation_audit_path')))
 assert r['operation_audit_paths']==(';'.join(paths) or 'none_for_cited_evidence')
 for p in paths:assert (L/p).exists()
fc=json.loads((W/'old-flow-updates.json').read_text());assert len(fc)==9
for x in fc:
 fid=x['before']['flow_id'];assert fm[fid]==x['after'] and x['before']['depth']==x['after']['depth']
 assert sum(any(r['flow_id']==fid for r in rows(p)) for p in (L/'batches').glob('*-coverage.csv'))==1
for pid,n in [('M-060',22),('M-065',18)]:
 subset=[f for f in fs if f['product_id']==pid];assert len(subset)==n and all(f['hands_on']=='false' for f in subset)
 assert set(re.findall(r'F-M\d{3}-\d{2}',(L/subset[0]['case_path']).read_text()))=={f['flow_id'] for f in subset}
assert fm['F-M065-07']['depth']==fm['F-M065-09']['depth']=='D1'
assert fm['F-M065-06']['visual_status']=='static_reference_only'
ec=json.loads((W/'old-evidence-updates.json').read_text());assert len(ec)==4
for x in ec:
 assert em[x['before']['evidence_id']]==x['after']
 assert x['after']['additional_captures'][:-1]==x['before'].get('additional_captures',[])
 c=x['after']['additional_captures'][-1];assert not c['hands_on'] and not c['visual_observed']
 assert sha(L/c['archived_artifact'])==c['sha256']
 for p,h in c['artifact_hashes'].items():assert sha(L/p)==h
q=json.loads((L/'artifacts/quaderno-file-lifecycle/source-audit.json').read_text())
assert len(q['gen2_pages_this_batch'])==17 and len(q['gen3c_pages_this_batch'])==18 and len(q['gen2_explicit_full_body_pages_in_batches50_51'])==27
texts={}
for c in q['captures']:
 assert sha(R/c['pdf'])==c['pdf_sha256'] and sha(R/c['text'])==c['text_sha256'] and 'root_read_full' in c['reading_status']
 texts[c['key']]=(R/c['text']).read_text()
claims={'gen2-transfers':['「Received」','（1）','閲覧日時はリセット','60 分に1 回','「QUADERNO PC App」以外には'],'gen2-delete-failures':['1GB以上','パソコンの時刻に再設定','リストに表示されなく','2.1.00.26250FP'],'gen3c-connection':['Bluetooth Low Energy(BLE)','macOS 12','スリープ中にWi-Fi 接続を維持','接続状態を確認'],'gen3c-file-management':['読み取り専用','一時的に保存','終了時に削除','1ページ目のみ','機器ID を選択']}
for k,ps in claims.items():
 for p in ps:assert p in texts[k],(k,p)
a=json.loads((L/'artifacts/ak-openapp-states/source-audit.json').read_text());assert len(a['images'])==4 and len(a['text_captures'])==2
for c in a['images']:assert c['status']==200 and c['viewed_by_root'] and sha(R/c['path'])==c['sha256']==sha(L/c['artifact_path'])
for c in a['text_captures']:assert sha(R/c['raw_work_path'])==c['raw_sha256']
assert '2025年5月1日' in (W/'ak-faq1014.txt').read_text() and 'SP1000M' in (W/'ak-faq974.txt').read_text()
e=em['E-AKOP-01'];assert e['derived_from']=='E-AK-06' and e['visual_observed'] and not e['hands_on'] and sha(L/e['archived_artifact'])==e['sha256']
for p,h in e['artifact_hashes'].items():assert sha(L/p)==h
ic=json.loads((W/'inventory-updates.json').read_text());assert len(ic)==65
assert Counter(x['file'] for x in ic)=={'quaderno-gen2-ink-inventory.csv':38,'quaderno-controls-deepening-inventory.csv':25,'ak-directory-deepening-inventory.csv':2}
for name,n in [('quaderno-gen2-ink-inventory.csv',197),('quaderno-controls-deepening-inventory.csv',275),('ak-directory-deepening-inventory.csv',202)]:
 rr=rows(L/'batches'/name);by={r['inventory_id']:r for r in rr};assert len(rr)==len(by)==n
 for x in ic:
  if x['file']==name:assert by[x['after']['inventory_id']]==x['after']
 for r in rr:
  assert all(k in fm for k in r['flow_ids'].split(';') if k) and all(k in em for k in r['evidence_ids'].split(';') if k)
assert sha(R/'docs/requirements.md')=='e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
briefs=sorted((R/'docs/briefs').glob('[0-9]*.md'));assert len(briefs)==51 and all(p.name in (R/'docs/briefs/README.md').read_text() for p in briefs)
cleanup=json.loads((W/'cache-cleanup.json').read_text());profile=R/'work/research/batch43-time-sample/profile'
assert cleanup['retained_profile_unchanged'] and not cleanup['formal_evidence_deleted'] and not cleanup['active_browser_processes_for_profile']
assert all(not (R/p).exists() for p in cleanup['targets'])
assert all(sha(profile/k)==v for k,v in cleanup['retained_profile_files'].items())
before=json.loads((W/'before-manifest.json').read_text());now={str(p.relative_to(R)):sha(p) for p in [R/'README.md',R/'AGENTS.md',*sorted((R/'docs').rglob('*'))] if p.is_file()}
new=sorted(set(now)-set(before));modified=sorted(k for k in set(now)&set(before) if now[k]!=before[k]);deleted=sorted(set(before)-set(now));assert not deleted and 'AGENTS.md' not in modified and 'docs/requirements.md' not in modified
result=dict(status='passed',flows=972,evidence=1714,distinct_urls=1542,reference_points=135,new_flows=5,old_flows_updated=9,old_evidence_captures=4,new_static_evidence=1,images_verified=4,inventory_rows_updated=65,gen2_pages=17,gen3c_pages=18,cache_only_cleanup_other_profile_files_unchanged=True,requirements_unchanged=True,new_formal_files=new,modified_formal_files=modified,deleted_formal_files=deleted,scope='Source identity, selected text claims, static-photo provenance and record/link consistency; no actual product operation')
dump(W/'root-final-verification.json',result);dump(W/'final-manifest.json',now)
print(json.dumps({k:result[k] for k in ['status','flows','evidence','reference_points','images_verified','inventory_rows_updated','requirements_unchanged']},ensure_ascii=False))
print('Formal changes:',len(new),'new',len(modified),'modified',len(deleted),'deleted')
