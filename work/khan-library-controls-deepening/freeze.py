from pathlib import Path
import hashlib,json
R=Path('/home/workspace/reader');W=R/'work/khan-library-controls-deepening';L=R/'docs/research/interaction-library';P='khan-library-controls-deepening'
files=[L/'cases/M-030-khan-kids.md',*sorted((L/'batches').glob(P+'-*')),*sorted((L/'artifacts'/P).rglob('*')),*sorted(p for p in W.rglob('*') if p.name not in {'manifest.json','check-output.json'}),R/'work/khan-controls-deepening/12-sections.json']
rows=[{'path':str(p.relative_to(R)),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in files if p.is_file()]
(W/'manifest.json').write_text(json.dumps({'checked_on':'2026-10-27','files':rows,'scope':'owned case/new batch/work and referenced unchanged section JSON; old/global ledgers excluded'},ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'files':len(rows),'manifest_sha256':hashlib.sha256((W/'manifest.json').read_bytes()).hexdigest()},indent=2))
