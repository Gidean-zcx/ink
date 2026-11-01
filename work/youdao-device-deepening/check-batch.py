from pathlib import Path
import json,re,hashlib
root=Path('/home/workspace/reader')
base=root/'docs/research/interaction-library'
case=base/'cases/M-025-youdao-pen.md'
files=[case,base/'batches/youdao-device-deepening-evidence.jsonl',base/'batches/youdao-device-deepening-coverage.csv']
for p in files:
 b=p.read_bytes()
 assert b.endswith(b'\n') and b'\r' not in b, p
 for i,l in enumerate(b.decode().splitlines(),1):
  assert l==l.rstrip(),(p,i,'trailing whitespace')
ids=set()
for name in ['dictionary-evidence.jsonl','youdao-device-deepening-evidence.jsonl']:
 for line in (base/'batches'/name).read_text().splitlines():
  ids.add(json.loads(line)['evidence_id'])
refs=set(re.findall(r'E-(?:YDP|YDCD)-\d{2}',case.read_text()))
assert refs<=ids,refs-ids
for target in re.findall(r'\]\(([^)]+)\)',case.read_text()):
 assert target.startswith('https://') or (root/target).exists(),target
print('PASS: 3 formal files LF/trailing whitespace; all explicit case E references resolve; links structurally valid (not a fresh reachability check)')
print('formal files:')
for p in files:print(str(p.relative_to(root)),len(p.read_bytes()),'bytes',hashlib.sha256(p.read_bytes()).hexdigest())
