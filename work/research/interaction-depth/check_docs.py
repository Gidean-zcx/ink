import csv
import json
import re
import hashlib
from collections import Counter
from pathlib import Path
from urllib.parse import urlparse, unquote

root=Path('/home/workspace/reader')
lib=root/'docs/research/interaction-library'
markdown=[root/'README.md', root/'AGENTS.md', *sorted((root/'docs').rglob('*.md'))]
errors=[]
links=0
for path in markdown:
    body=path.read_text()
    if not body.endswith('\n'): errors.append(f'No final newline: {path}')
    for n,line in enumerate(body.splitlines(),1):
        if line.rstrip()!=line: errors.append(f'Trailing whitespace: {path}:{n}')
    for raw in re.findall(r'\[[^\]]*\]\(([^\n)]+)\)',body):
        target=raw.strip('<>')
        if target.startswith(('https://','http://','mailto:','#','app://','codex://')): continue
        target=unquote(target.split('#',1)[0])
        target=re.sub(r':\d+$','',target)
        p=Path(target) if target.startswith('/') else path.parent/target
        links+=1
        if not p.exists(): errors.append(f'Broken local link: {path}: {target}')

products=list(csv.DictReader((lib/'catalog.csv').open()))
flows=list(csv.DictReader((lib/'coverage.csv').open()))
evidence=[json.loads(line) for line in (lib/'evidence.jsonl').read_text().splitlines()]
for records,key in [(products,'product_id'),(flows,'flow_id'),(evidence,'evidence_id')]:
    ids=[r[key] for r in records]
    if len(ids)!=len(set(ids)): errors.append(f'Duplicate ID: {key}')
pids={x['product_id'] for x in products}
eids={x['evidence_id'] for x in evidence}
evidence_by_id={x['evidence_id']:x for x in evidence}
def audited_browser(item):
    rel=item.get('operation_audit_path')
    audit=lib/rel if rel else None
    return bool(item.get('hands_on') is True
                and item.get('operation_method')=='automated_public_browser'
                and item.get('visual_kind')=='browser_sequence'
                and item.get('visual_observed') is True
                and audit and audit.exists()
                and item['evidence_id'] in audit.read_text()
                and hashlib.sha256(audit.read_bytes()).hexdigest()==item.get('operation_audit_sha256')
                and any(p.endswith('observed-states.json') for p in item.get('artifact_hashes',{}))
                and any(p.endswith('trace-actions.json') for p in item.get('artifact_hashes',{})))
for p in products:
    if not (root/p['legacy_document']).exists(): errors.append(f'Missing old document {p["product_id"]}')
    if p['detailed_case'] and not (lib/p['detailed_case']).exists(): errors.append(f'Missing case {p["product_id"]}')
    if not p['next_gap']: errors.append(f'Missing gap {p["product_id"]}')
for flow in flows:
    case=lib/flow['case_path']
    if flow['product_id'] not in pids or flow['flow_id'] not in case.read_text(): errors.append(f'Missing flow {flow["flow_id"]}')
    if not set(flow['evidence_ids'].split(';')) <= eids: errors.append(f'Unknown evidence {flow["flow_id"]}')
    browser_flow=flow['hands_on']=='true' and flow['depth']=='D4' and flow['visual_status']=='observed_browser_sequence'
    supporting=[evidence_by_id[x] for x in flow['evidence_ids'].split(';') if x in evidence_by_id]
    if browser_flow:
        if not any(audited_browser(e) and flow['flow_id'] in (lib/e['operation_audit_path']).read_text() for e in supporting):
            errors.append(f'Missing audited browser operation: {flow["flow_id"]}')
    elif flow['hands_on']!='false' or flow['depth'] not in ('D1','D2'):
        errors.append(f'Unsupported operation/depth claim: {flow["flow_id"]}')
    if not browser_flow and flow['visual_status'] not in ('not_observed','static_reference_only','static_official_images_only','static_manual_pages_only'):
        errors.append(f'Unreviewed visual status: {flow["flow_id"]}: {flow["visual_status"]}')
for item in evidence:
    parsed=urlparse(item['url'])
    if parsed.scheme not in ('http','https') or not parsed.netloc or any(c.isspace() for c in item['url']): errors.append(f'Invalid URL {item["evidence_id"]}')
    if item['product_id'] not in pids: errors.append(f'Unknown product {item["evidence_id"]}')
    if item['hands_on'] and not audited_browser(item): errors.append(f'Unsupported hands-on claim: {item["evidence_id"]}')
    if item['supersedes'] and item['supersedes'] not in eids: errors.append(f'Unknown supersedes: {item["evidence_id"]}')
    if item.get('derived_from') and item['derived_from'] not in eids: errors.append(f'Unknown parent: {item["evidence_id"]}')
    if item['archived_artifact']:
        asset=lib/item['archived_artifact']
        if not asset.exists(): errors.append(f'Missing artifact: {asset}')
        elif hashlib.sha256(asset.read_bytes()).hexdigest()!=item['sha256']: errors.append(f'Hash mismatch: {asset}')
    if item['visual_observed'] and not item['archived_artifact']:
        audit_path=item.get('visual_audit_path')
        if not audit_path:
            errors.append(f'Observed static evidence needs durable excerpt or explicit audit: {item["evidence_id"]}')
        else:
            audit=lib/audit_path
            if not audit.exists() or item['evidence_id'] not in audit.read_text():
                errors.append(f'Missing visual observation audit: {item["evidence_id"]}')
            elif hashlib.sha256(audit.read_bytes()).hexdigest()!=item.get('visual_audit_sha256'):
                errors.append(f'Visual audit hash mismatch: {item["evidence_id"]}')
    for rel,digest in item.get('artifact_hashes',{}).items():
        asset=lib/rel
        if not asset.exists() or hashlib.sha256(asset.read_bytes()).hexdigest()!=digest:
            errors.append(f'Multi-artifact hash mismatch: {asset}')
for case in (lib/'cases').glob('*.md'):
    text=case.read_text()
    if not set(re.findall(r'E-[A-Z]+-\d{2}',text)) <= eids: errors.append(f'Unknown E reference: {case}')
    listed=set(re.findall(r'F-M\d{3}-\d{2}',text))
    covered={f['flow_id'] for f in flows if (lib/f['case_path'])==case}
    if listed!=covered: errors.append(f'Flow coverage mismatch: {case}')
for path in (root/'docs').rglob('*.json'): json.loads(path.read_text())
for kind,key,records in [('evidence.jsonl','evidence_id',evidence),('coverage.csv','flow_id',flows)]:
    merged={}
    for path in (lib/'batches').glob('*-'+kind):
        batch=[json.loads(l) for l in path.read_text().splitlines()] if kind.endswith('jsonl') else list(csv.DictReader(path.open()))
        for row in batch:
            if row[key] in merged and merged[row[key]]!=row:errors.append(f'Batch conflict: {row[key]}')
            merged[row[key]]=row
    if merged!={r[key]:r for r in records}:errors.append(f'Master index differs from batches: {kind}')
report=dict(markdown_files=len(markdown),local_links_checked=links,products=len(products),
    partial_cases=sum(bool(p['detailed_case']) for p in products),flows=len(flows),depths=Counter(f['depth'] for f in flows),
    evidence_records=len(evidence),distinct_source_urls=len({e['url'] for e in evidence}),
    static_visual_evidence=sum(e['visual_observed'] and e.get('visual_kind')!='browser_sequence' for e in evidence),
    operated_browser_evidence=sum(audited_browser(e) for e in evidence),errors=errors)
print(json.dumps(report,ensure_ascii=False,indent=2))
raise SystemExit(bool(errors))
