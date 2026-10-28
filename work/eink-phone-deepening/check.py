from pathlib import Path
from urllib.parse import urlsplit
from collections import Counter
import csv
import json
import re

ROOT = Path('/home/workspace/reader')
BASE = ROOT / 'docs/research/interaction-library'
BATCH = BASE / 'batches/eink-phone-deepening-evidence.jsonl'
COVERAGE = BASE / 'batches/eink-phone-deepening-coverage.csv'
new_e = [json.loads(line) for line in BATCH.read_text().splitlines()]
master_e = [json.loads(line) for line in (BASE / 'evidence.jsonl').read_text().splitlines()]
with COVERAGE.open() as f:
    r = csv.DictReader(f)
    fields = r.fieldnames
    rows = list(r)
with (BASE / 'coverage.csv').open() as f:
    assert fields == next(csv.reader(f))

assert len(new_e) == 13 and len(rows) == 10
assert len({e['evidence_id'] for e in new_e}) == 13
assert len({r['flow_id'] for r in rows}) == 10
all_e = {e['evidence_id'] for e in new_e + master_e}
domains = {'device.report', 'www.bilibili.com', 'www.reddit.com', 'mall.hisense.com',
           'mobile.hisense.com', 'www.ithome.com', 'post.smzdm.com', 'www.e-paper.org.cn',
           'mobile.yesky.com', 'sblt.deali.cn', 'store.bigme.vip', 'fans.hisense.com'}
for e in new_e:
    assert set(e) == set(master_e[0]), e['evidence_id']
    assert e['checked_on'] == '2026-10-26'
    assert e['hands_on'] is False and e['visual_observed'] is False
    assert e['archived_artifact'] is None and e['sha256'] is None
    u = urlsplit(e['url'])
    assert u.scheme == 'https' and u.hostname in domains
    assert e['version_scope'] and e['access_mode'] and e['locator']
    assert e['product_id'] in ['M-011', 'M-055']
for r in rows:
    assert r['depth'] in ['D1', 'D2']
    assert r['hands_on'] == 'false' and r['visual_status'] == 'not_observed'
    assert r['checked_on'] == '2026-10-26'
    assert set(r['evidence_ids'].split(';')) <= all_e, r['flow_id']
    assert (BASE / r['case_path']).exists()

for product, slug, start, stop in [('M-011','bigme',8,12), ('M-055','hisense',3,9)]:
    prefix = product.replace('-', '')
    expected = [f'F-{prefix}-{n:02}' for n in range(start,stop)]
    assert [r['flow_id'] for r in rows if r['product_id'] == product] == expected
    case = BASE / f'cases/{product}-{slug}.md'
    text = case.read_text()
    definitions = re.findall(r'^\| (F-M\d{3}-\d{2}) \|', text, re.M)
    assert definitions == [f'F-{prefix}-{n:02}' for n in range(1,stop)]
    assert len(definitions) == len(set(definitions))
    assert set(re.findall(r'E-[A-Z]+-\d{2}',text)) <= all_e
    assert '功能目录与覆盖' in text
    assert '无实机' in text and '推断' in text
    assert '\ufffd' not in text
    assert all(line.rstrip() == line for line in text.splitlines())
    for line in text.splitlines():
        if re.match(r'^\| F-M\d{3}-\d{2} \|', line):
            assert len(line.strip('|').split('|')) == 7, line
    for title, link in re.findall(r'\[([^\]]+)\]\(([^)]+)\)', text):
        assert urlsplit(link).hostname in domains, (title,link)

assert Counter(r['depth'] for r in rows) == {'D2':8, 'D1':2}
print(json.dumps({'result':'PASS', 'evidence':13, 'new_flows':10,
    'depth':dict(Counter(r['depth'] for r in rows)),
    'case_flow_totals':{'M-011':11,'M-055':8},
    'old_coverage_updates':{'F-M011-01':'add E-BGD-02; still D2, video metadata only',
                            'F-M055-01':'add E-HSD-08; still D1'},
    'checks':'schema, unique and consecutive ids, old F retention, evidence refs, false observation flags, HTTPS/domain syntax, UTF-8, 7-column flow tables, whitespace',
    'limits':'not a live HTTP availability check or product behavior test; read only'},ensure_ascii=False))
