import csv
import hashlib
import json
import re
from collections import Counter
from pathlib import Path

R = Path('/home/workspace/reader')
L = R / 'docs/research/interaction-library'
W = R / 'work/britannica-controls-deepening'

def j(p):
    return json.loads(p.read_text())

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

raw, formal, denied = set(), set(), set()

def register(p):
    (formal if p.is_relative_to(L) else raw).add(p)

def check(x):
    if isinstance(x, list):
        for item in x:
            check(item)
    elif isinstance(x, dict):
        for pk, hk in [
            ('raw_work_path', 'raw_sha256'),
            ('text_work_path', 'text_sha256'),
            ('extracted_work_path', 'extracted_sha256'),
            ('extracted_text_work_path', 'extracted_text_sha256'),
            ('structure_path', 'structure_sha256'),
            ('source', 'source_sha256'),
        ]:
            if x.get(pk) and x.get(hk):
                p = R / x[pk]
                assert sha(p) == x[hk], p
                register(p)
        for pk, hk in [
            ('archived_artifact', 'sha256'),
            ('note_artifact', 'note_sha256'),
            ('operation_audit_path', 'operation_audit_sha256'),
        ]:
            if x.get(pk):
                p = L / x[pk]
                assert sha(p) == x[hk], p
                register(p)
        if x.get('visual_audit_path'):
            digest = x.get('visual_audit_sha256')
            if digest is None and x.get('archived_artifact') == x['visual_audit_path']:
                digest = x['sha256']
            p = L / x['visual_audit_path']
            assert digest and sha(p) == digest, p
            register(p)
        for name, digest in x.get('artifact_hashes', {}).items():
            p = L / name
            assert sha(p) == digest, p
            register(p)
        if x.get('path') and x.get('sha256'):
            p = R / x['path'] if x['path'].startswith(('work/', 'docs/')) else L / x['path']
            assert sha(p) == x['sha256'], p
            register(p)
        for v in x.values():
            if isinstance(v, (list, dict)):
                check(v)

E = [json.loads(line) for line in (L / 'evidence.jsonl').read_text().splitlines()]
F = list(csv.DictReader((L / 'coverage.csv').open()))
be = {e['evidence_id']: e for e in E}
bf = {f['flow_id']: f for f in F}
names = ['britannica-controls-deepening', 'weread-controls-deepening', 'meebook-controls-deepening']
newE, newF = [], []
for name in names:
    for kind, rows in [('evidence.jsonl', newE), ('coverage.csv', newF)]:
        p = L / 'batches' / f'{name}-{kind}'
        if not p.exists():
            assert name == 'meebook-controls-deepening', p
            continue
        rows.extend([json.loads(line) for line in p.read_text().splitlines()] if p.suffix == '.jsonl' else csv.DictReader(p.open()))
assert len(newE) == 11 and len(newF) == 4
assert Counter(f['depth'] for f in newF) == {'D2': 3, 'D4': 1}
for e in newE:
    assert be[e['evidence_id']] == e
    assert e['hands_on'] == (e['evidence_id'] in ['E-BRCD-06', 'E-BRCD-07'])
    assert not e.get('derived_from') or e['derived_from'] in be
for f in newF:
    assert bf[f['flow_id']] == f
    assert f['hands_on'] == ('true' if f['flow_id'] == 'F-M049-08' else 'false')
check(newE)

a = j(W / 'applied-reviewed-updates.json')
for x in a['flow_changes']:
    assert bf[x['id']][x['field']] == x['after'], x
for x in a['evidence_changes']:
    assert be[x['id']][x['field']] == x['after'], x
    check(x['after'])
for x in a['files']:
    assert sha(R / x['path']) == x['after_sha256'], x['path']
    assert sha(R / x['backup']) == x['before_sha256'], x['backup']
assert (a['old_tasks'], a['old_task_fields'], a['old_evidence_captures']) == (17, 63, 14)
assert bf['F-M049-05']['depth'] == 'D4' and bf['F-M013-01']['depth'] == 'D2'

fix = j(W / 'audit-id-fix.json')
assert sha(R / fix['audit_backup']) == fix['old_audit_sha256']
assert sha(R / fix['audit_path']) == fix['new_audit_sha256']
assert sha(R / fix['batch_backup']) == fix['before_sha256']
assert sha(R / fix['batch_path']) == fix['after_sha256']
old = {e['evidence_id']: e for e in map(json.loads, (R / fix['batch_backup']).read_text().splitlines())}
for x in fix['changes']:
    assert old[x['evidence_id']][x['field']] == x['before']
    old[x['evidence_id']][x['field']] = x['after']
assert len(fix['changes']) == 4
assert list(old.values()) == [e for e in newE if e['evidence_id'].startswith('E-BRCD-')]
audit = (R / fix['audit_path']).read_text()
for token in ['E-BRCD-06', 'E-BRCD-07', 'F-M049-05', 'F-M049-08']:
    assert token in audit

index = L / 'inventories/README.md'
paths = {(index.parent / p).resolve() for p in re.findall(r'\[[^\]]*\]\(([^)]+\.csv)\)', index.read_text())}
counts = {}
for p in paths:
    rows = list(csv.DictReader(p.open()))
    counts[str(p.relative_to(L))] = len(rows)
    for row in rows:
        assert None not in row and None not in row.values(), p
        for value in row.values():
            assert set(re.findall(r'(?<![A-Za-z0-9-])E-[A-Z0-9]+-\d{2}', value)) <= set(be), (p, value)
            assert set(re.findall(r'F-M\d{3}-\d{2}', value)) <= set(bf), (p, value)
assert (len(counts), sum(counts.values())) == (47, 4577), (len(counts), sum(counts.values()))
for name, count in zip(names, [163, 32, 49]):
    p = L / 'batches' / f'{name}-inventory.csv'
    rows = list(csv.DictReader(p.open()))
    key = next(iter(rows[0]))
    assert len(rows) == len({row[key] for row in rows}) == count
    assert b'\\r\\n' not in p.read_bytes()

for name, file in [
    ('britannica-controls-deepening', 'captures.json'),
    ('britannica-controls-deepening', 'render-manifest.json'),
    ('weread-controls-deepening', 'captures.json'),
    ('weread-controls-deepening', 'file-hashes.json'),
    ('meebook-controls-deepening', 'formal-manifest.json'),
]:
    check(j(R / 'work' / name / file))
for cap in map(json.loads, (R / 'work/meebook-controls-deepening/captures.jsonl').read_text().splitlines()):
    check(cap)
    if cap.get('http_status') == 403:
        denied.add(R / cap['raw_work_path'])
assert len(denied) == 5

B = W / 'browser'
states = j(B / 'observed-states.json')
actions = j(B / 'trace-actions.json')
assert len(states) == 35 and len(actions) == 29
assert sum(a['result'] != 'ok' for a in actions) == 3
assert len(j(B / 'browser-errors.json')) == 20
for state in states:
    name = state['name']
    for suffix in ['.json', '.html', '.png']:
        assert (B / (name + suffix)).is_file(), name + suffix
def frame(name):
    return j(B / (name + '-frame.json'))
assert 'Try again' in frame('13-wrong-doctor')['text']
assert 'Librarians help people find books' in frame('21-back-settled')['text']
assert 'clues' in frame('29-completion')['text']
assert frame('29-completion')['text'].count('Check box –') == 4
assert 'slide: Final' in frame('30-after-completion-next')['text']
for name in ['32-start-again', '34-frame-reloaded']:
    assert 'slide: Title' in frame(name)['text']
assert '429' in j(B / '36-catalog-error.json')['text']
catalog = 'https://britannicaeducation.com/resources/britannica-school-early-elementary-content-catalog/'
iframe = 'https://cdn.britannica.com/11/288011-184-BF60C826/default.html?autoplay=1'
status = [x for x in j(B / 'document-statuses.json') if x['url'] in [catalog, iframe]]
assert len(status) == 3 and all(x['status'] == 200 for x in status)
assert sum("f.goto(f.url" in x['action'] for x in actions) == 1
assert sum("go('" + catalog in x['action'] for x in actions) == 1

new_artifacts = [p for p in formal if any('/artifacts/' + name + '/' in str(p) for name in names)]
images = {p for p in new_artifacts if p.suffix.lower() in ['.png', '.jpg', '.jpeg']}
browser_images = {p for p in images if p.parent.name == 'browser'}
assert len(images) == 28 and len(browser_images) == 22, (len(images), len(browser_images))
known = {e['url'] for e in E}
for e in E:
    for cap in e.get('additional_captures', []):
        known.add(cap.get('capture_url', ''))
brief = (R / 'docs/briefs/020-child-feedback-and-resumable-state.md').read_text()
assert set(re.findall(r'\]\((https?://[^)]+)\)', brief)) <= known
assert len(E) == 1282 and len(F) == 787
assert Counter(f['depth'] for f in F) == {'D2': 692, 'D1': 80, 'D4': 15}
report = dict(
    new_evidence=11, new_tasks=4, new_D2=3, new_D4=1,
    old_tasks_amended=17, old_fields_amended=63, old_evidence_additional_captures=14,
    audit_id_fix_fields=4, inventories=47, inventory_rows=4577, new_inventory_rows=244,
    new_archived_images=len(images), static_images=len(images-browser_images), browser_images=len(browser_images),
    raw_files_checked=len(raw-denied), rejected_response_files_checked=len(denied),
    browser_states=len(states), browser_command_groups=len(actions), command_errors=3,
    errors=[],
)
(W / 'verification-result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(report, ensure_ascii=False))
