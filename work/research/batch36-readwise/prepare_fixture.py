from pathlib import Path
import json,subprocess,hashlib
W=Path(__file__).resolve().parent
# Research-only synthetic content. No personal reading records or external source navigation.
base=[
 dict(id='101',text='Synthetic highlight A: a learner explains an idea before receiving feedback.',note='Synthetic note: ask for an example.',book_id=1,source_title='Synthetic Learning Notes',source_author='Research fixture',url='https://example.invalid/a'),
 dict(id='102',text='Synthetic highlight B: a later attempt can distinguish recognition from recall.',note='',book_id=1,source_title='Synthetic Learning Notes',source_author='Research fixture',url='https://example.invalid/a'),
 dict(id='201',text='Synthetic highlight C: compare evidence from independent news sources.',note='Synthetic note: a related idea, not a proven conclusion.',book_id=2,source_title='Synthetic News Notes',source_author='Research fixture',url='https://example.invalid/b'),
 dict(id='202',text='Synthetic highlight D: record a correction separately from the original report.',note='',book_id=2,source_title='Synthetic News Notes',source_author='Research fixture',url='https://example.invalid/b'),
 dict(id='301',text='Synthetic singleton highlight E: one book has only one retained highlight.',note='',book_id=3,source_title='Synthetic Single Source',source_author='Research fixture',url='https://example.invalid/c'),
 dict(id='401',text='识字练习先尝试再提示',note='Synthetic short Chinese content.',book_id=4,source_title='Synthetic Short Source',source_author='Research fixture',url=''),
 dict(id='101',text='Synthetic duplicate highlight with a changed note is placed after the first.',note='This is intentionally a second version.',book_id=1,source_title='Synthetic Learning Notes',source_author='Research fixture',url=''),
 dict(id='501',text='Finished Document: synthetic activity marker excluded by the renderer.',note='',book_id=5,source_title='Synthetic Activity',source_author='Research fixture',url='')]
connections=[dict(a_id='101',b_id='201',label='Synthetic relation',why='Synthetic connection supplied by the researcher; no AI judgment.'),dict(a_id='201',b_id='101',label='Duplicate reversed pair',why='Must deduplicate by unordered IDs.'),dict(a_id='101',b_id='401',label='Filtered endpoint',why='Short text endpoint is filtered out.')]
for name,data in [('highlights.json',base),('connections.json',connections)]:
 (W/name).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
cmd=['python3',str(W/'build_graph.py'),'--highlights',str(W/'highlights.json'),'--connections',str(W/'connections.json'),'--output',str(W/'synthetic-graph.html')]
r=subprocess.run(cmd,capture_output=True,text=True)
log=dict(command=cmd,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr,synthetic_only=True,source_sha256=hashlib.sha256((W/'build_graph.py').read_bytes()).hexdigest())
(W/'generation-log.json').write_text(json.dumps(log,ensure_ascii=False,indent=2)+'\n');print(json.dumps(log,ensure_ascii=False));raise SystemExit(r.returncode)
