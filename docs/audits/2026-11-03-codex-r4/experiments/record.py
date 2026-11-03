"""Small incremental audit recorder; writes only the r4 audit directory."""
import json
from pathlib import Path

BASE = Path(__file__).resolve().parent.parent

def write_json(name, value):
    (BASE / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n")

def record(rows, heading, body):
    old = json.loads((BASE / 'verification.json').read_text())
    by_id = {r['id']: r for r in old}
    by_id.update({r['id']: r for r in rows})
    order = ['AUD4-001','AUD4-002','AUD4-003','AUD4-004','AUD2-019','AUD2-023','AUD3-002','N3-3']
    rows = [by_id[k] for k in order if k in by_id]
    write_json('verification.json', rows)
    p = BASE / 'report.md'
    text = p.read_text()
    a = text.index('## 修复核实表')
    b = text.index('## 新问题')
    table = '## 修复核实表\n\n| 编号 | 结论 | 证据 | 说明 |\n| --- | --- | --- | --- |\n'
    for r in rows:
        note = r['note'].replace('|','\\|').replace('\n',' ')
        table += f"| {r['id']} | {r['verdict']} | 【{r['evidence_type']}】 | {note} |\n"
    pending = [k for k in order if k not in by_id]
    table += '\n' + ('待核实：' + '、'.join(pending) + '。' if pending else '全部 8 项已核实。') + '\n\n'
    text = text[:a] + table + text[b:]
    title = '### ' + heading
    section_prefix = '### ' + heading.split('.', 1)[0] + '.'
    if section_prefix in text:
        a = text.index(section_prefix)
        b = text.find('\n##', a + len(section_prefix))
        if b < 0: b = len(text)
        text = text[:a] + title + '\n\n' + body.strip() + '\n' + text[b:]
    else:
        text = text.replace('## 命令、覆盖与限制', title + '\n\n' + body.strip() + '\n\n## 命令、覆盖与限制')
    p.write_text(text)

def finding(row):
    p = BASE / 'findings.json'
    old = json.loads(p.read_text())
    by_id = {r['id']: r for r in old}
    by_id[row['id']] = row
    rows = list(by_id.values())
    write_json('findings.json', rows)
    p = BASE / 'report.md'
    text = p.read_text()
    a = text.index('## 新问题')
    b = text.index('## 分组核查记录')
    section = '## 新问题\n\n'
    for r in rows:
        section += f"### {r['id']}（{r['severity']}）：{r['title']}\n\n"
        section += f"- 证据：【{r['evidence_type']}】{r['reproduce']}\n"
        section += '- 位置：' + '；'.join('`' + s + '`' for s in r['locations']) + '。\n'
        section += f"- 影响：{r['impact']}\n- 建议：{r['recommendation']}\n\n"
    p.write_text(text[:a] + section + text[b:])
