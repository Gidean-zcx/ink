from pathlib import Path
from urllib.parse import urlsplit
from collections import Counter
import csv
import json
import re

root = Path('/home/workspace/reader')
base = root / 'docs/research/interaction-library'
batch = base / 'batches/thin-audio-deepening-evidence.jsonl'
coverage = base / 'batches/thin-audio-deepening-coverage.csv'
evidence = [json.loads(line) for line in batch.read_text().splitlines()]
master = [json.loads(line) for line in (base / 'evidence.jsonl').read_text().splitlines()]
with coverage.open() as f:
    reader = csv.DictReader(f)
    fields = reader.fieldnames
    rows = list(reader)
with (base / 'coverage.csv').open() as f:
    master_fields = next(csv.reader(f))
assert fields == master_fields
assert len(evidence) == 24 and len(rows) == 19
assert len({e['evidence_id'] for e in evidence}) == len(evidence)
assert len({r['flow_id'] for r in rows}) == len(rows)
all_eids = {e['evidence_id'] for e in evidence + master}
for e in evidence:
    assert set(e) == set(master[0]), e['evidence_id']
    assert e['hands_on'] is False and e['visual_observed'] is False
    assert e['archived_artifact'] is None and e['sha256'] is None
    assert e['checked_on'] == '2026-10-26'
    url = urlsplit(e['url'])
    assert url.scheme == 'https' and url.hostname in ['guide.hiby.com', 'support.yotoplay.com']
for r in rows:
    assert r['depth'] in ['D1', 'D2'] and r['hands_on'] == 'false'
    assert r['visual_status'] == 'not_observed'
    assert set(r['evidence_ids'].split(';')) <= all_eids, r['flow_id']
    assert (base / r['case_path']).exists()
    assert r['checked_on'] == '2026-10-26'
for product, slug, start, stop in [('M-035', 'hiby', 3, 10), ('M-037', 'yoto', 4, 16)]:
    selected = [r for r in rows if r['product_id'] == product]
    flow_prefix = product.replace('-', '')
    expected = [f'F-{flow_prefix}-{n:02d}' for n in range(start, stop)]
    assert [r['flow_id'] for r in selected] == expected
    case = base / f'cases/{product}-{slug}.md'
    text = case.read_text()
    definitions = re.findall(r'^\| (F-M\d{3}-\d{2})(?: \|| )', text, re.M)
    assert definitions == [f'F-{flow_prefix}-{n:02d}' for n in range(1, stop)]
    assert set(re.findall(r'E-[A-Z]+-\d{2}', text)) <= all_eids
    assert '功能目录与覆盖' in text and '无' in text
    assert not any(line.rstrip() != line for line in text.splitlines())
    assert '\ufffd' not in text
    for line in text.splitlines():
        if re.match(r'\| F-M\d{3}-\d{2} \|', line):
            assert len(line.strip('|').split('|')) == 7
    for title, link in re.findall(r'\[([^\]]+)\]\(([^)]+)\)', text):
        assert urlsplit(link).hostname in ['guide.hiby.com', 'support.yotoplay.com'], (title, link)
print(json.dumps({'result':'PASS', 'evidence':len(evidence), 'flows':len(rows),
                  'depth':dict(Counter(r['depth'] for r in rows)),
                  'case_flow_totals':{'M-035':9,'M-037':15},
                  'checks':'schema, unique ids, append sequence, legacy ids, evidence references, scope flags, URL syntax/domain, UTF-8, table width, whitespace',
                  'limits':'not HTTP availability or hands-on behavior verification; read only'}, ensure_ascii=False))
