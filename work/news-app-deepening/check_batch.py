from pathlib import Path
import csv
import hashlib
import json
import re
from urllib.parse import unquote

root = Path('/home/workspace/reader')
lib = root/'docs/research/interaction-library'
batches = lib/'batches'
errors = []
evidence, flows = {}, {}
for path in batches.glob('*-evidence.jsonl'):
    for line in path.read_text().splitlines():
        item = json.loads(line)
        key = item['evidence_id']
        if key in evidence and evidence[key] != item:
            errors.append('conflicting evidence '+key)
        evidence[key] = item
for path in batches.glob('*-coverage.csv'):
    for item in csv.DictReader(path.open()):
        key = item['flow_id']
        if key in flows and flows[key] != item:
            errors.append('conflicting flow '+key)
        flows[key] = item
new_evidence=[json.loads(line) for line in (batches/'news-app-deepening-evidence.jsonl').read_text().splitlines()]
new_flows=list(csv.DictReader((batches/'news-app-deepening-coverage.csv').open()))
digest=lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
for item in new_evidence:
    if item['hands_on']:
        errors.append('hands-on '+item['evidence_id'])
    for key in ('qualifies','derived_from','supersedes'):
        if item.get(key) and item[key] not in evidence:
            errors.append('unknown '+key+' '+item['evidence_id'])
    if item.get('raw_work_path'):
        path=root/item['raw_work_path']
        if not path.exists() or digest(path)!=item['raw_sha256']:
            errors.append('raw hash '+item['evidence_id'])
    if item['visual_observed'] and not item['archived_artifact']:
        errors.append('missing static artifact '+item['evidence_id'])
    for path, sha in item.get('artifact_hashes',{}).items():
        if not (lib/path).exists() or digest(lib/path)!=sha:
            errors.append('artifact hash '+path)
for item in new_flows:
    if set(item['evidence_ids'].split(';'))-evidence.keys():
        errors.append('flow references '+item['flow_id'])
    if item['depth'] not in ('D1','D2') or item['hands_on']!='false':
        errors.append('depth/operation '+item['flow_id'])
for name in ('M-045-espresso.md','M-046-ground-news.md'):
    path=lib/'cases'/name
    text=path.read_text()
    if not text.endswith('\n') or any(line.rstrip()!=line for line in text.splitlines()):
        errors.append('whitespace '+name)
    if set(re.findall(r'E-[A-Z]+-\d{2}',text))-evidence.keys():
        errors.append('case evidence '+name)
    written=set(re.findall(r'F-M\d{3}-\d{2}',text))
    indexed={item['flow_id'] for item in flows.values() if item['case_path']=='cases/'+name}
    if written!=indexed:
        errors.append('case flow coverage '+name)
    for raw in re.findall(r'\[[^\]]*\]\(([^\n)]+)\)',text):
        target=raw.strip('<>')
        if target.startswith(('http://','https://','#','mailto:')):
            continue
        target=re.sub(r':\d+$','',unquote(target.split('#',1)[0]))
        linked=Path(target) if target.startswith('/') else path.parent/target
        if not linked.exists():
            errors.append('local link '+str(linked))
print(json.dumps(dict(evidence_records=len(new_evidence),flow_records=len(new_flows),
    D2=sum(x['depth']=='D2' for x in new_flows),D1=sum(x['depth']=='D1' for x in new_flows),
    static_evidence=sum(x['visual_observed'] for x in new_evidence),errors=errors),ensure_ascii=False))
raise SystemExit(bool(errors))
