import csv
import hashlib
import json
import re
from collections import Counter
from pathlib import Path

R = Path('/home/workspace/reader')
L = R / 'docs/research/interaction-library'
W = R / 'work/research/batch54-integration'


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def rows(p):
    return list(csv.DictReader(p.open()))


def dump(p, value):
    p.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')


fs = rows(L / 'coverage.csv')
es = [json.loads(s) for s in (L / 'evidence.jsonl').read_text().splitlines()]
rp = rows(L / 'reference-points.csv')
fm = {f['flow_id']: f for f in fs}
em = {e['evidence_id']: e for e in es}
assert len(fs) == 987 and len(es) == 1753 and len(rp) == 165
assert Counter(f['depth'] for f in fs) == {'D2': 888, 'D1': 79, 'D4': 20}
assert len({e['url'] for e in es}) == 1581
assert sum(e.get('visual_observed') and e.get('visual_kind') != 'browser_sequence' for e in es) == 230
assert sum(e.get('hands_on') is True for e in es) == 21
assert len({r['reference_id'] for r in rp}) == 165
for r in rp:
    assert len(r) == 22 and all(r.values()), r['reference_id']
    ff = [fm[k] for k in r['flow_ids'].split(';')]
    ee = [em[k] for k in r['evidence_ids'].split(';')]
    assert all(f['product_id'] == r['product_id'] for f in ff)
    assert (L / r['artifact_path']).exists()
    expected = {
        'task_depths': {f['flow_id']: f['depth'] for f in ff},
        'task_hands_on': {f['flow_id']: f['hands_on'] == 'true' for f in ff},
        'source_types': {e['evidence_id']: e['source_type'] for e in ee},
        'access_modes': {e['evidence_id']: e['access_mode'] for e in ee},
        'evidence_hands_on': {e['evidence_id']: e['hands_on'] for e in ee},
        'operation_methods': {e['evidence_id']: e.get('operation_method', 'not_recorded') for e in ee},
    }
    for key, value in expected.items():
        assert json.loads(r[key]) == value, (r['reference_id'], key)
    paths = list(dict.fromkeys(e['operation_audit_path'] for e in ee if e.get('hands_on') and e.get('operation_audit_path')))
    assert r['operation_audit_paths'] == (';'.join(paths) or 'none_for_cited_evidence')

changes = json.loads((W / 'old-flow-updates.json').read_text())
assert len(changes) == 2
for change in changes:
    fid = change['before']['flow_id']
    assert fm[fid] == change['after']
    assert change['before']['depth'] == change['after']['depth'] == 'D2'
    assert change['after']['hands_on'] == 'false'
    assert sum(any(r['flow_id'] == fid for r in rows(p)) for p in (L / 'batches').glob('*-coverage.csv')) == 1
new = rows(L / 'batches/quizlet-media-and-sharing-coverage.csv')
assert [f['flow_id'] for f in new] == [f'F-M021-{i:02d}' for i in range(24, 33)]
assert Counter(f['depth'] for f in new) == {'D2': 8, 'D1': 1}
assert fm['F-M021-32']['depth'] == 'D1'
subset = [f for f in fs if f['product_id'] == 'M-021']
assert len(subset) == 32 and Counter(f['depth'] for f in subset) == {'D2': 31, 'D1': 1}
assert all(f['hands_on'] == 'false' and f['visual_status'] == 'not_observed' for f in subset)
case = (L / subset[0]['case_path']).read_text()
assert set(re.findall(r'F-M\d{3}-\d{2}', case)) == {f['flow_id'] for f in subset}

captures = json.loads((W / 'old-evidence-updates.json').read_text())
assert {x['before']['evidence_id'] for x in captures} == {'E-QZCD-01', 'E-QZCD-02', 'E-QZCD-04'}
for change in captures:
    e = em[change['before']['evidence_id']]
    assert e == change['after']
    assert e['additional_captures'][:-1] == change['before'].get('additional_captures', [])
    c = e['additional_captures'][-1]
    assert not c['hands_on'] and not c['visual_observed']
    assert sha(R / c['raw_work_path']) == c['raw_sha256']

audit = json.loads((L / 'artifacts/quizlet-media-and-sharing/source-audit.json').read_text())
assert len(audit['sources']) == 23 and not audit['account_used'] and not audit['images_viewed']
phrases = {
    1: ['term and definition side', 'to add it to your definition', 'one image'],
    2: ['JPG, GIF, or PNG up to 3MB', "can't currently add an image to the term side", 'Done to save'],
    3: ['Release the spacebar', 'to save and play back your recording', 'Create to save a new set', 'Done to save an existing set'],
    4: ['at least two points to save', 'MBP', 'Click and drag over text to blur it'],
    5: ['must be published to sync across devices', 'private and visible only to the creator'],
    6: ['Only public sets and folders can be shared', 'sending the password separately'],
    7: ['Editable by', 'Your set is private and only you can edit it'],
    8: ['Visible To menu', 'Enter the password again to confirm it'],
    9: ['Tags work like subfolders', 'Select Saved', 'unless all the sets in it are private'],
    10: ['select will be highlighted', 'Select Done', 'unless all the sets in it are private'],
    11: ['Accept Invite', 'limited to 8 members', 'Know and Still Learning', 'Leave group'],
    12: ['number of viewers', 'requirements to be a top creator can change over time'],
    13: ['not notified when you leave a rating', 'Select Submit'],
    14: ["can't delete it, but you can edit", 'Select Submit'],
    15: ["won't get a notification", 'not possible to turn ratings off'],
    16: ['terms paired with their matching definitions', 'terms and optional definitions'],
    17: ['by school or course', 'number of terms', 'filter your results by username'],
    18: ['Request to join class', 'Once your teacher accepts your request'],
    19: ['available globally for learners ages 13+', 'optimized for U.S. college students', 'After answering some questions', 'Study Bites appear once you begin studying'],
    20: ['Copy HTML', 'Choose a Study Mode', 'Quizlet logo, and that cannot be removed'],
    21: ['class owner', 'but you cannot drop it', 'add you back'],
    22: ['Windows 7', 'Windows 8', 'Reload the page'],
    23: ['Australia, Austria, Belgium, Canada, France, Germany, Ireland, Japan', 'South Korea, the United Kingdom, and the United States', 'Under Manage subscription, select your subscription', 'Select Save'],
}
for src in audit['sources']:
    e = em[src['evidence_id']]
    raw = R / src['raw_work_path']
    assert sha(raw) == src['raw_sha256'] == e['raw_sha256']
    assert e['url'] == src['url'] and src['body_read_by_root']
    text = raw.read_text()
    section = text[text.index(src['title'] + ' – Quizlet Help Center ('):].split('-' * 80)[0]
    assert src['url'] in section and src['entry_url'] in section
    lo, hi = map(int, re.findall(r'\d+', src['body_locator']))
    body = '\n'.join(line for line in section.splitlines() if (m := re.match(r'L(\d+):', line)) and lo <= int(m[1]) <= hi)
    normal = ' '.join(body.split())
    for phrase in phrases[int(src['evidence_id'][-2:])]:
        assert phrase in normal, (src['evidence_id'], phrase)
    if src['entry_url'] != src['url']:
        assert 'Redirected to URL: ' + src['url'] in section
    assert not e['hands_on'] and not e['visual_observed']
    assert sha(L / e['archived_artifact']) == e['sha256']
    for path, digest in e['artifact_hashes'].items():
        assert sha(L / path) == digest
for suffix in ['12', '16']:
    src = next(x for x in audit['sources'] if x['evidence_id'] == 'E-QZMS-' + suffix)
    assert src['flow_ids'] == []

inv = rows(L / 'batches/quizlet-controls-deepening-inventory.csv')
by_order = {r['order']: r for r in inv}
ic = json.loads((W / 'inventory-updates.json').read_text())
assert len(inv) == 70 and len(by_order) == 70
assert Counter(r['entry_kind'] for r in inv) == {'child_topic': 64, 'parent_group': 4, 'linked_article': 2}
assert Counter(x['kind'] for x in ic) == {'old_directory_body': 21, 'linked_article_added': 2}
for change in ic:
    assert by_order[change['after']['order']] == change['after']
    if change['before']:
        assert change['before']['coverage'] == '仅目录；正文未在本例展开'
    else:
        assert change['after']['entry_kind'] == 'linked_article'
assert by_order['69']['parent_order'] == '11' and by_order['70']['parent_order'] == '28'
assert 'D1' in by_order['28']['coverage'] and 'D1' in by_order['70']['coverage']
for r in inv:
    assert all(k in fm for k in r['flow_ids'].split(';') if k)
    assert all(k in em for k in r['evidence_ids'].split(';') if k)
    if r['entry_kind'] in ['child_topic', 'linked_article']:
        assert r['article_url'] and '仅目录' not in r['coverage']
        assert any('index' not in em[k]['source_type'] for k in r['evidence_ids'].split(';'))

cleanup = json.loads((W / 'cache-cleanup.json').read_text())
assert cleanup['deleted_bytes'] == 9338113 and len(cleanup['removed_files']) == 86
assert sum(x['bytes'] for x in cleanup['removed_files']) == cleanup['deleted_bytes']
assert not cleanup['formal_evidence_deleted'] and cleanup['sources_unchanged'] and cleanup['retained_files_unchanged']
for target in cleanup['targets']:
    assert not (R / target).exists()
for item in cleanup['removed_files']:
    assert item['cache'].endswith('.pyc') and item['source'].endswith('.py')
    assert not (R / item['cache']).exists() and sha(R / item['source']) == item['source_sha256']
for path, digest in cleanup['retained_package_files'].items():
    assert sha(R / path) == digest

assert sha(R / 'docs/requirements.md') == 'e141320e2714df805c009fe64dd92d17decd6429d5728f8ce42d6d868b775087'
briefs = sorted((R / 'docs/briefs').glob('[0-9]*.md'))
assert len(briefs) == 54 and all(p.name in (R / 'docs/briefs/README.md').read_text() for p in briefs)
check = json.loads((W / 'final-check.json').read_text())
assert check['errors'] == [] and check['flows'] == 987 and check['evidence_records'] == 1753
assert check['partial_cases'] == check['products'] == 65
before = json.loads((W / 'before-manifest.json').read_text())
now = {str(p.relative_to(R)): sha(p) for p in [R / 'README.md', R / 'AGENTS.md', *sorted((R / 'docs').rglob('*'))] if p.is_file()}
newfiles = sorted(set(now) - set(before))
modified = sorted(k for k in set(now) & set(before) if now[k] != before[k])
deleted = sorted(set(before) - set(now))
assert not deleted and 'AGENTS.md' not in modified and 'docs/requirements.md' not in modified
result = dict(
    status='passed', flows=987, evidence=1753, distinct_urls=1581, reference_points=165,
    new_flows=9, new_D2=8, new_D1=1, old_flows_updated=2, new_help_evidence=23,
    old_directory_captures=3, original_article_rows_with_body=64, linked_article_rows=2,
    original_rows_updated=21, inventory_rows=70, new_actual_operations=0,
    requirements_unchanged=True, cache_only_cleanup_sources_and_retained_files_unchanged=True,
    new_formal_files=newfiles, modified_formal_files=modified, deleted_formal_files=deleted,
    scope='Source identity and selected main-body claims; task, evidence, reference and inventory consistency. No product operation or learning-effect verification.',
)
dump(W / 'root-final-verification.json', result)
dump(W / 'final-manifest.json', now)
print(json.dumps({k: result[k] for k in ['status', 'flows', 'evidence', 'reference_points', 'original_article_rows_with_body', 'requirements_unchanged']}, ensure_ascii=False))
print('Formal changes:', len(newfiles), 'new', len(modified), 'modified', len(deleted), 'deleted')
