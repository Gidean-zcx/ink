#!/usr/bin/env python3
"""Render verified rows and confirmed findings from JSON into the incremental report."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def render():
    report = (ROOT / 'report.md').read_text()
    rows = json.loads((ROOT / 'verification.json').read_text())
    body = '| 编号 | 结论 | 证据 | 说明 |\n| --- | --- | --- | --- |\n'
    for r in rows:
        note = r['note'].replace('|', '\\|').replace('\n', ' ')
        body += f"| {r['id']} | {r['verdict']} | 【{r['evidence_type']}】 | {note} |\n"
    needed = [f'AUD3-{i:03}' for i in range(1, 5)] + [f'AUD2-{i:03}' for i in (10,12,16,19,20,22,23,24)]
    todo = [x for x in needed if x not in {r['id'] for r in rows}]
    body += '\n' + ('待核实：' + '、'.join(todo) if todo else '全部 12 项已核实。') + '\n\n'
    a = report.index('## 修复核实表\n') + len('## 修复核实表\n')
    b = report.index('## 分组证据与进度')
    report = report[:a] + '\n' + body + report[b:]
    fs = json.loads((ROOT / 'findings.json').read_text())
    body = '\n'
    for f in fs:
        body += f"### {f['id']}｜{f['severity']}｜{f['title']}\n\n"
        body += f"**范围**：{f['area']}。**证据**：【{f['evidence_type']}】。\n\n"
        body += '**位置**：' + '；'.join('`'+s+'`' for s in f['locations']) + '。\n\n'
        for field, label in [('reproduce','核实与复现'),('impact','影响'),('recommendation','建议')]:
            body += f"**{label}**：{f[field]}\n\n"
    if not fs:
        body += '尚未记录已确认的新问题；审计仍在进行。\n\n'
    a = report.index('## 新问题\n') + len('## 新问题\n')
    b = report.index('## 命令、覆盖与限制')
    report = report[:a] + body + report[b:]
    (ROOT / 'report.md').write_text(report)

if __name__ == '__main__':
    render()
