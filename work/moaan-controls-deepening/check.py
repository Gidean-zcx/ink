"""Read-only, pre-merge checks for this bounded batch. No network or full-library validator."""
from pathlib import Path
from hashlib import sha256
from urllib.parse import urlparse
import csv
import io
import json
import re

ROOT=Path('/home/workspace/reader')
WORK=ROOT/'work/moaan-controls-deepening'
LIB=ROOT/'docs/research/interaction-library'
def read_json(path): return json.loads(path.read_text())
def digest(path): return sha256(path.read_bytes()).hexdigest()
def read_csv(path):
    text=path.read_text()
    assert '\\r\\n' not in text and '\\n' not in text, path
    return list(csv.DictReader(io.StringIO(text)))

baseline=read_json(WORK/'baseline.json')
assert digest(WORK/'case-before.md')==baseline['case_sha256']
assert not baseline['reverse_dependencies']
manifest=read_json(WORK/'formal-manifest.json')
for r in manifest: assert digest(ROOT/r['path'])==r['sha256'],r['path']

master_e={e['evidence_id']:e for e in map(json.loads,(LIB/'evidence.jsonl').read_text().splitlines())}
master_f={r['flow_id']:r for r in csv.DictReader(io.StringIO((LIB/'coverage.csv').read_text()))}
new_e=list(map(json.loads,(LIB/'batches/moaan-controls-deepening-evidence.jsonl').read_text().splitlines()))
assert len(new_e)==1
new=new_e[0]
assert new['evidence_id']=='E-MOCD-01' and new['evidence_id'] not in master_e
assert new['supersedes'] is None and new['hands_on'] is False and new['visual_observed'] is True
assert new['url'] not in {e.get('url') for e in master_e.values()}
assert urlparse(new['url']).scheme in ('http','https')
assert not new['archived_artifact'].startswith(('work/','/'))
for path_key,hash_key in [('raw_work_path','raw_sha256'),('extracted_work_path','extracted_sha256')]:
    assert digest(ROOT/new[path_key])==new[hash_key]
assert digest(LIB/new['archived_artifact'])==new['sha256']
for img in new['observed_artifacts']:
    assert digest(LIB/img['path'])==img['sha256']==digest(ROOT/img['raw_work_path'])

assert read_csv(LIB/'batches/moaan-controls-deepening-coverage.csv')==[]
inventory=read_csv(LIB/'batches/moaan-controls-deepening-inventory.csv')
assert len(inventory)==82
assert len({r['inventory_id'] for r in inventory})==82
ids={r['inventory_id'] for r in inventory}
for r in inventory:
    assert r['parent_id']=='' or r['parent_id'] in ids
    assert urlparse(r['source_url']).scheme in ('http','https')
    assert all(e in master_e or e==new['evidence_id'] for e in r['evidence_ids'].split(';'))
    assert all(f in master_f for f in r['flow_ids'].split(';') if f)

updates=read_json(WORK/'old-coverage-updates.json')
assert len(updates)==8 and sum(len(r['changed_fields']) for r in updates)==27
for u in updates:
    assert master_f[u['flow_id']]==u['before'],u['flow_id']
    assert u['after']['depth']==u['before']['depth']
    assert u['after']['hands_on']=='false'
    assert u['after']['visual_status'] in ('not_observed','static_reference_only')
    diff={k:{'before':v,'after':u['after'][k]} for k,v in u['before'].items() if v!=u['after'][k]}
    assert diff==u['changed_fields']
    assert all(e in master_e or e==new['evidence_id'] for e in u['after']['evidence_ids'].split(';'))

ef=read_json(WORK/'old-evidence-field-updates.json')
assert len(ef)==2 and sum(len(r['changed_fields']) for r in ef)==3
for u in ef:
    assert master_e[u['evidence_id']]==u['before']
    assert u['changed_fields']=={k:{'before':v,'after':u['after'][k]} for k,v in u['before'].items() if v!=u['after'][k]}
caps=read_json(WORK/'old-evidence-additional-captures.json')
assert len(caps)==12
for u in caps:
    old=master_e[u['evidence_id']]
    assert old['url']==u['expected_url']
    assert old.get('additional_captures',[])==u['before_additional_captures']
    for c in u['append_additional_captures']:
        assert c['hands_on'] is False
        assert digest(ROOT/c['raw_work_path'])==c['raw_sha256']
        assert digest(LIB/c['archived_artifact'])==c['sha256']
        if c.get('extracted_work_path'): assert digest(ROOT/c['extracted_work_path'])==c['extracted_sha256']
        if c.get('audit_artifact'): assert digest(LIB/c['audit_artifact'])==c['audit_sha256']

case=(LIB/'cases/M-054-moaan.md').read_text()
assert '待root' not in case and '待根代理' not in case and '主表未改' not in case
assert set(re.findall(r'F-M054-\d{2}',case))==set(master_f)&{f'F-M054-{n:02d}' for n in range(1,9)}
for target in re.findall(r'\]\((/home/workspace/reader/[^)]+)\)',case):
    assert Path(target).exists(),target
print('PASS: 1 new E, 0 new F; 82 inventory rows; 8 old F / 27 fields; 2 old E / 3 fields; 12 captures; hashes, references, physical CSV newlines and pre-merge baselines verified.')
