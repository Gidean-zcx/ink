"""Read-only pre-merge check for the bounded M055 batch. No network or writes."""
from pathlib import Path
import csv
import hashlib
import io
import json
import re
from collections import Counter
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'work/hisense-controls-deepening'
LIB = ROOT / 'docs/research/interaction-library'
BATCH = LIB / 'batches'
PREFIX = 'hisense-controls-deepening'

def read_json(p):
    return json.loads(p.read_text())

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def jsonl(p):
    return [json.loads(x) for x in p.read_text().splitlines() if x]

def csv_rows(p, strict_lf=True):
    data = p.read_bytes()
    if strict_lf:
        assert b'\\r\\n' not in data and b'\r' not in data, str(p)
    rows = list(csv.DictReader(io.StringIO(data.decode())))
    assert all(None not in row for row in rows), str(p)
    return rows

def check_paths(row):
    for key, hash_key, base in [
        ('raw_work_path', 'raw_sha256', ROOT),
        ('extracted_work_path', 'extracted_sha256', ROOT),
        ('html_shell_work_path', 'html_shell_sha256', ROOT),
        ('archived_artifact', 'sha256', LIB),
        ('visual_audit_path', 'visual_audit_sha256', LIB),
    ]:
        value = row.get(key)
        if value:
            assert not Path(value).is_absolute(), (key, value)
            if base == LIB:
                assert value.startswith('artifacts/hisense-controls-deepening/'), value
            else:
                assert value.startswith('work/hisense-controls-deepening/'), value
            assert sha(base / value) == row[hash_key], (key, value)
        elif key in row:
            assert row.get(hash_key) is None, key

baseline = read_json(WORK / 'baseline.json')
assert sha(WORK / 'case-before.md') == baseline['case_sha256']
assert baseline['reverse_dependencies'] == []
manifest = read_json(WORK / 'formal-manifest.json')
assert len(manifest) == 10
for x in manifest:
    assert sha(ROOT / x['path']) == x['sha256'], x['path']

old_e = {x['evidence_id']: x for x in jsonl(LIB / 'evidence.jsonl')}
old_f = {x['flow_id']: x for x in csv_rows(LIB / 'coverage.csv', strict_lf=False)}
new_e = jsonl(BATCH / f'{PREFIX}-evidence.jsonl')
new_f = csv_rows(BATCH / f'{PREFIX}-coverage.csv')
inventory = csv_rows(BATCH / f'{PREFIX}-inventory.csv')
assert len(new_e) == 6 and len(new_f) == 2 and len(inventory) == 114
assert {x['evidence_id'] for x in new_e} == {f'E-HSCD-{i:02}' for i in range(1, 7)}
assert {x['flow_id'] for x in new_f} == {'F-M055-09', 'F-M055-10'}
assert not ({x['evidence_id'] for x in new_e} & old_e.keys()), 'Already merged or ID collision'
assert not ({x['flow_id'] for x in new_f} & old_f.keys()), 'Already merged or ID collision'
old_urls = {x['url'] for x in old_e.values() if x.get('url')}
assert len({x['url'] for x in new_e}) == len(new_e)
assert not ({x['url'] for x in new_e} & old_urls), 'Existing URL should reuse E'
for x in new_e:
    assert x['supersedes'] is None and x['hands_on'] is False
    assert x['product_id'] == 'M-055'
    assert urlsplit(x['url']).scheme in ('http', 'https')
    check_paths(x)
    if x['visual_observed']:
        assert x['derived_from'] in ('E-HSD-05', 'E-HSD-09')
        assert x['observed_on'] == '2026-10-27'
assert sum(x['visual_observed'] for x in new_e) == 5
eids = set(old_e) | {x['evidence_id'] for x in new_e}
fids = set(old_f) | {x['flow_id'] for x in new_f}
visual_enums = {x['visual_status'] for x in old_f.values()}
for x in new_f:
    assert x['depth'] == 'D2' and x['hands_on'] == 'false'
    assert x['visual_status'] == 'static_reference_only' and x['visual_status'] in visual_enums
    assert set(x['evidence_ids'].split(';')) <= eids
    assert (LIB / x['case_path']).is_file()
ids = {x['inventory_id'] for x in inventory}
assert len(ids) == 114
for x in inventory:
    assert not x['parent_id'] or x['parent_id'] in ids
    assert set(filter(None, x['evidence_ids'].split(';'))) <= eids
    assert set(filter(None, x['flow_ids'].split(';'))) <= fids
summary = read_json(WORK / 'inventory-summary.json')
assert Counter(x['source_family'] for x in inventory) == summary['families']

updates = read_json(WORK / 'old-coverage-updates.json')
assert len(updates) == 5
assert sum(len(x['changed_fields']) for x in updates) == 22
for x in updates:
    assert old_f[x['flow_id']] == x['before'], x['flow_id']
    actual = {k: {'before': x['before'][k], 'after': v} for k, v in x['after'].items() if x['before'][k] != v}
    assert actual == x['changed_fields'], x['flow_id']
    assert x['after']['depth'] == x['before']['depth']
    assert x['after']['hands_on'] == 'false'
    assert set(x['after']['evidence_ids'].split(';')) <= eids
captures = read_json(WORK / 'old-evidence-additional-captures.json')
assert len(captures) == 4
for x in captures:
    previous = old_e[x['evidence_id']]
    assert previous['url'] == x['expected_url']
    assert previous.get('additional_captures', []) == x['before_additional_captures'], x['evidence_id']
    assert len(x['append_additional_captures']) == 1
    for cap in x['append_additional_captures']:
        assert cap['url'] == previous['url'] and cap['hands_on'] is False
        check_paths(cap)
assert read_json(WORK / 'old-evidence-field-updates.json') == []
case = (LIB / 'cases/M-055-hisense.md').read_text()
assert set(re.findall(r'F-M055-\d{2}', case)) == {f'F-M055-{i:02}' for i in range(1, 11)}
assert not any(s in case for s in ('待root', '待根', '主表未改', '等待合并'))
for path in re.findall(r'\]\((/home/workspace/reader/[^)]+)\)', case):
    assert Path(path).is_file(), path
print(json.dumps({
    'status': 'PASS', 'mode': 'pre_merge_read_only',
    'new_e': len(new_e), 'new_f': len(new_f), 'inventory_rows': len(inventory),
    'old_f_updates': len(updates), 'old_f_changed_fields': 22,
    'old_e_additional_captures': len(captures), 'observed_static_images': 5,
    'formal_files': len(manifest), 'case_sha256': sha(LIB / 'cases/M-055-hisense.md'),
    'manifest_sha256': sha(WORK / 'formal-manifest.json'),
    'state_changed_by_check': False,
}, ensure_ascii=False, indent=2))
