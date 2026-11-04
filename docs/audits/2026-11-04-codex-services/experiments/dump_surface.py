#!/usr/bin/env python3
"""Capture and parse the real tools/list replies in the existing integration test."""
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
dest = ROOT / 'build-audit/audit-probes/surface-capture'
dest.mkdir(parents=True, exist_ok=True)
source = ROOT / 'tests/integration/ai_surface.c'
content = source.read_text()
def include(match):
    path = source.parent / match.group(1)
    return '#include "' + str(path.resolve()) + '"' if path.exists() else match.group(0)
content = re.sub(r'#include "([^"]+)"', include, content)
old = 'size_t n = strlen(reply);'
assert content.count(old) == 1
content = content.replace(old, 'fprintf(stderr, "AUDIT_PAGE %s\\n", reply);\n        ' + old)
test = dest / 'surface_capture.c'
test.write_text(content)
log = dest / 'capture.log'
cmd = [sys.executable, str(HERE / 'run_probe.py'), str(test), '--template', 'sim-ai-surface',
       '--script', 'tests/integration/ai_surface.script', '--budget', 'tests/integration/ai_surface.budget.json']
with log.open('w') as stream:
    result = subprocess.run(cmd, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
assert result.returncode == 0, log
pages = [json.loads(line[len('AUDIT_PAGE '):]) for line in log.read_text().splitlines() if line.startswith('AUDIT_PAGE ')]
tools = []
for page in pages:
    result = page.get('result', page)
    for tool in result['tools']:
        tools.append(dict(name=tool['name'], form=tool.get('form', 'call'), inputSchema=tool.get('inputSchema'), outputSchema=tool.get('outputSchema')))
names = {item['name'] for item in tools}
assert len(names) == len(tools), 'duplicate tool across pages'
table = (ROOT / 'docs/system/system-services-design.md').read_text().split('### 1.3 能力总表', 1)[1].split('## 2.', 1)[0]
expected = set()
for row in table.splitlines():
    parts = row.split('|')
    if len(parts) > 3 and parts[1].strip() in ('数据', '文件', '时间', '通知', '确认卡', '应用', '系统'):
        expected.update(re.findall(r'`([^`]+)`', parts[2]))
inventory = dict(commit='a81c1b8ed805c044bcc24a120c14bb8e3a4d011e',
                 command='python3 docs/audits/2026-11-04-codex-services/experiments/dump_surface.py',
                 pages=len(pages), tools_count=len(tools),
                 document_stage1_and_system_missing=sorted(expected-names),
                 runtime_extra_to_selected_document_rows=sorted(names-expected),
                 forbidden_present=sorted(names & {'confirm.accept', 'confirm.reject'}), tools=tools)
(HERE / 'surface_inventory.json').write_text(json.dumps(inventory, ensure_ascii=False, indent=2) + '\n')
print(json.dumps({k:v for k,v in inventory.items() if k != 'tools'}, ensure_ascii=False, indent=2))
