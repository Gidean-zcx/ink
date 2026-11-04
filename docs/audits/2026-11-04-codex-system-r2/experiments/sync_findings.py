#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Keep the report's finding section in sync with the incremental structured ledger."""
import json
from pathlib import Path

p = Path(__file__).resolve().parent.parent
findings = json.loads((p / 'findings.json').read_text())
r = (p / 'report.md').read_text()
body = '\n\n'.join(
    '### ' + f['id'] + '（' + f['severity'] + '）' + f['title'] + '\n\n'
    + ''.join('【' + x + '】' for x in f['evidence_type']) + '位置：'
    + '、'.join('`' + x + '`' for x in f['locations']) + '。\n\n'
    + '影响：' + f['impact'] + '\n\n证据：' + f['reproduce']
    + '\n\n建议：' + f['recommendation'] for f in findings
)
before, tail = r.split('## 3. 新问题\n', 1)
_, after = tail.split('## 4. 分范围核查记录\n', 1)
r = before + '## 3. 新问题\n\n' + body + '\n\n## 4. 分范围核查记录\n' + after
verifications = sorted(json.loads((p / 'verification.json').read_text()), key=lambda f: f['id'])
table = '| 编号 | 结论 | 证据 | 说明 |\n| --- | --- | --- | --- |\n'
table += ''.join('| ' + ' | '.join(v[k].replace('|', '\\|') for k in ['id', 'verdict', 'evidence', 'note']) + ' |\n' for v in verifications)
before, tail = r.split('## 2. 修复核实\n', 1)
_, after = tail.split('## 3. 新问题\n', 1)
(p / 'report.md').write_text(before + '## 2. 修复核实\n\n' + table + '\n## 3. 新问题\n' + after)
