from pathlib import Path
import csv
import json
import re

ROOT = Path('/home/workspace/reader')
BASE = ROOT / 'docs/research/interaction-library'
WORK = ROOT / 'work/thin-audio-deepening-research'
JOURNAL = BASE / 'batches/thin-audio-deepening-evidence.jsonl'
COVERAGE = BASE / 'batches/thin-audio-deepening-coverage.csv'
SCHEMA = list(json.loads((BASE / 'evidence.jsonl').read_text().splitlines()[0]))
with (BASE / 'coverage.csv').open() as fh:
    COLUMNS = next(csv.reader(fh))

sources = []
flows = []
for slug, product, prefix, minimum in [('hiby', 'M-035', 'HBD', 3), ('yoto', 'M-037', 'YOD', 4)]:
    case = BASE / f'cases/{product}-{slug}.md'
    text = (WORK / f'{slug}.md').read_text()
    case.write_text(text)
    for line in text.splitlines():
        if line.startswith(f'| E-{prefix}-'):
            cells = [x.strip() for x in line.strip('|').split('|')]
            eid, link, locator = cells
            match = re.fullmatch(r'\[(.*?)\]\((.*?)\)', link)
            assert match, line
            title, url = match.groups()
            date = re.search(r'20\d{2}-\d{2}-\d{2}', locator)
            scope = ('HiBy R3II (2025); dynamic model manual, firmware reference 1.1_202501061153; actual firmware unknown'
                     if product == 'M-035' else
                     'Yoto Player 3rd Generation / Yoto Mini 2024 Edition; iOS/Android app; actual OS/app build unknown; excludes 4th Generation')
            row = dict.fromkeys(SCHEMA)
            row.update(evidence_id=eid, product_id=product, title=title, url=url,
                       checked_on='2026-10-26', source_type='official_manual' if eid == 'E-HBD-01' else 'official_help',
                       access_mode='opened_text' if product == 'M-035' else 'official_search_index_text',
                       version_scope=scope, source_updated_on=date.group() if date else None,
                       locator=locator, note=f'本轮新增定位；具体已查/未知见 {case.name}。',
                       hands_on=False, visual_observed=False,
                       capture_state='bibliography_and_short_note_only')
            if eid in ('E-HBD-02', 'E-YOD-19', 'E-YOD-20'):
                row['source_type'] = 'official_release_notes'
            if eid == 'E-HBD-01':
                row['note'] = '与 E-HBY-01 同一本手册；扩展目录/章节，非独立交叉验证。'
            if eid == 'E-HBD-02':
                row['firmware_version'] = '1.1_202501061153 (listed release; not measured on device)'
                row['note'] = '仅阅读版本记录；未取得用户升级包，Burn firmware 与 .upt 不作等同。'
            if eid == 'E-HBD-03':
                row['note'] = '明确包含本型号的跨机型 FAQ；第二卡槽示例、定时及音效冲突保留。'
            if eid == 'E-YOD-19':
                row['note'] = '页列稳定 3.34 / beta 3.35、历史 3.22 邀请码；不是设备构建测量。'
            if eid == 'E-YOD-20':
                row['note'] = '2.23.3 限生产批次、其余 2.23.2；旧播客续播条目不证明全内容跨设备断点。'
            sources.append(row)
        if not re.match(r'\| F-M\d{3}-\d{2} \|', line):
            continue
        cells = [x.strip() for x in line.strip('|').split('|')]
        assert len(cells) == 7, line
        fid, task, precondition, operation, ending, evidence, gap = cells
        assert int(fid.rsplit('-', 1)[1]) >= minimum
        depth = re.search(r'\bD[12]\b', task).group()
        ids = list(dict.fromkeys(re.findall(r'E-[A-Z]+-\d{2}', evidence)))
        assert ids, fid
        flows.append(dict(zip(COLUMNS, [fid, product, task, depth, ';'.join(ids),
                                         ending, gap, 'not_observed', 'false',
                                         str(case.relative_to(BASE)), '2026-10-26'])))

JOURNAL.write_text(''.join(json.dumps(row, ensure_ascii=False) + '\n' for row in sources))
with COVERAGE.open('w', newline='') as fh:
    writer = csv.DictWriter(fh, fieldnames=COLUMNS)
    writer.writeheader()
    writer.writerows(flows)
print(json.dumps({'evidence': len(sources), 'flows': len(flows),
                  'depth': {d: sum(x['depth'] == d for x in flows) for d in ['D1', 'D2']},
                  'files': [str(JOURNAL.relative_to(ROOT)), str(COVERAGE.relative_to(ROOT))]}, ensure_ascii=False))
