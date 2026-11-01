from pathlib import Path
p=Path('/home/workspace/reader/work/research/batch52-integration/integrate_once.py')
s=p.read_text()
# Initial run wrote E/F/capture records before stopping at the inventory assertion.
# Resume only the unapplied inventory/RP portion; never append those captures twice.
prefix=s[:s.index("assert not (L/'batches/quizlet-material-workflows-evidence.jsonl').exists()")]
data=s[s.index('data=['):s.index('known=[')]
tail=s[s.index("p=L/'batches/quizlet-controls-deepening-inventory.csv'"):]
exec(compile(prefix+data+tail,str(p)+'#resume_indexes','exec'))
