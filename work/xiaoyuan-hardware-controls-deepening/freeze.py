from pathlib import Path
import hashlib,json
R=Path('/home/workspace/reader');W=R/'work/xiaoyuan-hardware-controls-deepening';L=R/'docs/research/interaction-library';P='xiaoyuan-hardware-controls-deepening'
files=[L/'cases/M-056-xiaoyuan-hardware.md',*sorted((L/'batches').glob(P+'-*')),*sorted((L/'artifacts'/P).glob('*')),*sorted(p for p in W.iterdir() if p.is_file() and p.name not in {'manifest.json','check-output.json'})]
files += [R/'work/learning-hardware'/f for f in ['official-main.js','official-main-decoded.txt','parent-privacy-body.html','parent-privacy-body.txt']]
rows=[{'path':str(p.relative_to(R)),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in files]
(W/'manifest.json').write_text(json.dumps({'checked_on':'2026-10-27','scope':'M056 owned formal/work files and unchanged referenced raw; no master/old batch freeze','files':rows},ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'files':len(rows),'manifest_sha256':hashlib.sha256((W/'manifest.json').read_bytes()).hexdigest()},indent=2))
