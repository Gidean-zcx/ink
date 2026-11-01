"""Read-only validation for this batch, before parent merges old/new records."""
import csv
import hashlib
import json
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
W=ROOT/'work/boox-reading-controls-deepening'
LIB=ROOT/'docs/research/interaction-library'
ART=LIB/'artifacts/boox-reading-controls-deepening'
P=LIB/'batches/boox-reading-controls-deepening'
def js(p):return json.loads(p.read_text())
def jl(p):return [json.loads(x) for x in p.read_text().splitlines() if x]
def csvrows(p):
    with p.open(newline='') as f:return list(csv.DictReader(f))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

baseline=js(W/'baseline.json')
assert sha(W/'case-before.md')==baseline['case_sha256']
assert sha(W/'inventory-before.csv')==baseline['old_inventory_sha256']
assert baseline['reverse_refs']==[]
masterE=jl(LIB/'evidence.jsonl');masterF=csvrows(LIB/'coverage.csv')
oldE=js(W/'evidence-before.json');oldF=js(W/'coverage-before.json')
assert [x for x in masterE if x['product_id']=='M-004']==oldE,'M004 E changed; pre-merge check only'
assert [x for x in masterF if x['product_id']=='M-004']==oldF,'M004 F changed; pre-merge check only'
assert len(oldE)==38 and len(oldF)==14
byE={x['evidence_id']:x for x in masterE};byF={x['flow_id']:x for x in masterF}
E=jl(Path(str(P)+'-evidence.jsonl'));F=csvrows(Path(str(P)+'-coverage.csv'));I=csvrows(Path(str(P)+'-inventory.csv'))
assert len(E)==13 and len(F)==5 and len(I)==127
newE={x['evidence_id']:x for x in E};newF={x['flow_id']:x for x in F}
assert set(newE)=={f'E-BXRD-{n:02}' for n in range(1,14)}
assert set(newF)=={f'F-M004-{n:02}' for n in range(15,20)}
assert not set(newE).intersection(byE) and not set(newF).intersection(byF)
for p in (LIB/'batches').glob('*-evidence.jsonl'):
    if p==Path(str(P)+'-evidence.jsonl'):continue
    assert not set(newE).intersection(x['evidence_id'] for x in jl(p)),p
oldurls={x['url'] for x in masterE}
assert len({x['url'] for x in E})==13
for row in E:
    assert row['url'] not in oldurls,row['url']
    assert row['product_id']=='M-004' and row['supersedes'] is None and row['hands_on'] is False
    assert row['derived_from'] in (byE|newE)
    assert row['archived_artifact'].startswith('artifacts/boox-reading-controls-deepening/')
    assert sha(LIB/row['archived_artifact'])==row['sha256']
    assert sha(ROOT/row['raw_work_path'])==row['raw_sha256']
    assert row['visual_observed']==(int(row['evidence_id'].split('-')[-1])>=10)
for row in F:
    assert row['depth']=='D2' and row['hands_on']=='false'
    assert row['visual_status'] in {'not_observed','static_reference_only'}
    assert row['case_path']=='cases/M-004-boox-neoreader.md'
    assert set(row['evidence_ids'].split(';')).issubset(byE|newE)

original=ROOT/'work/boox-controls-deepening/epaper-pc-articles.json'
assert sha(original)=='02e30182beee444963ce691bbec9523380dcf91000c32685e27bea33f08a2cd8'
source={x['id']:x for x in js(original)['articles']}
index=js(W/'article-index.json');assert len(index)==10
for a in index:
    assert js(W/'raw'/f'{a["article_id"]}.json')==source[a['article_id']]
    assert (ROOT/a['text_path']).is_file()
originalI=js(W/'inventory-before-parsed.json')
parents=[x for x in I if x['entry_kind']=='article'];children=[x for x in I if x['entry_kind']=='body_heading_or_control_group']
assert len(parents)==59 and len(children)==68
assert len({x['section'] for x in parents})==14
assert sum(x['read_basis']=='existing_local_article_body' for x in parents)==10
for before,after in zip(originalI,parents):
    for key in ('order','section','article_id','title','url','source_updated_on','edited_at'):
        assert before[key]==after[key],(key,before['article_id'])
for row in I:
    assert set(filter(None,row['evidence_ids'].split(';'))).issubset(byE|newE)
    assert set(filter(None,row['flow_ids'].split(';'))).issubset(byF|newF)
for row in children:
    assert row['parent_item_id'] in {x['item_id'] for x in parents}
    assert row['article_id'] in {str(x['article_id']) for x in index}
for suffix,count in (('-coverage.csv',5),('-inventory.csv',127)):
    raw=Path(str(P)+suffix).read_bytes()
    assert raw.count(b'\n')==count+1 and b'\r' not in raw and b'\\r\\n' not in raw

updates=js(W/'old-coverage-updates.json');assert len(updates)==2
changes=0
for u in updates:
    assert byF[u['flow_id']]==u['before']
    changed={k for k in u['before'] if u['before'][k]!=u['after'][k]}
    assert changed==set(u['changed_fields'])
    for key in changed:assert u['changed_fields'][key]=={'before':u['before'][key],'after':u['after'][key]}
    changes+=len(changed)
assert changes==8
captures=js(W/'old-evidence-additional-captures.json');assert len(captures)==2
for u in captures:
    current=byE[u['evidence_id']]
    assert current['url']==u['expected_url']
    assert current.get('additional_captures',[])==u['before_additional_captures']
    assert len(u['append_additional_captures'])==1
    c=u['append_additional_captures'][0]
    assert c['hands_on'] is False and c['continuous_sequence'] is False
    assert sha(ROOT/c['raw_work_path'])==c['raw_sha256']
    assert sha(LIB/c['archived_artifact'])==c['sha256']
assert js(W/'old-evidence-field-updates.json')==[]
images=js(W/'image-fetch.json');views=js(ART/'observed-images.json')
assert len(images)==len(views)==4
for image,view in zip(images,views):
    assert image['status']==200 and image['method']=='anonymous_ordinary_GET_once'
    assert view['actual_view']=='view_image' and view['hands_on'] is False and view['continuous_sequence'] is False
    assert sha(ROOT/view['raw_work_path'])==view['raw_sha256']
    assert sha(LIB/view['archived_artifact'])==view['sha256']==view['raw_sha256']
case=LIB/'cases/M-004-boox-neoreader.md';text=case.read_text()
assert len(re.findall(r'^\| F-M004-\d{2} \|',text,re.M))==19
assert '本轮不新增 F' not in text
assert not any(term in text for term in ['待root合并','主表未改'])
for target in re.findall(r'\]\(([^)]+)\)',text):
    if target.startswith(('http://','https://','#')):continue
    assert (case.parent/target.split('#')[0]).exists(),target
for name,count in [('formal-manifest.json',10),('raw-manifest.json',25)]:
    manifest=js(W/name);assert len(manifest)==count
    for row in manifest:assert sha(ROOT/row['path'])==row['sha256'],row['path']
result=js(W/'summary.json')|{'result':'PASS','mode':'read_only_pre_merge','state_changed_by_check':False}
print(json.dumps(result,ensure_ascii=False,indent=2))
