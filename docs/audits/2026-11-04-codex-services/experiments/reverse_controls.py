#!/usr/bin/env python3
"""Isolated negative controls: copies in build-audit, never edit the repository sources."""
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[4]
HERE = Path(__file__).resolve().parent
OUT = ROOT / 'build-audit/audit-probes/reverse-controls'
OUT.mkdir(parents=True, exist_ok=True)

def absolute_includes(text, origin):
    def fix(match):
        candidate = origin.parent / match.group(1)
        return '#include "' + str(candidate.resolve()) + '"' if candidate.exists() else match.group(0)
    return re.sub(r'#include "([^"]+)"', fix, text)

cases = [
    ('snapshot-version', 'tests/integration/data.c', 'system/services/data/datastore.c',
     '(!is_log || f.ver > c->last_ver)', '(f.ver > c->last_ver)', 'sim-data', 'tests/integration/data.script', 'tests/integration/data.budget.json'),
    ('child-family-write', 'tests/integration/files.c', 'system/services/file/file_service.c',
     'if (write && !adult) return INK_FS_ERR_DENIED;', 'if (false) return INK_FS_ERR_DENIED;', 'sim-files', 'build-audit/test-out/files.script', 'tests/integration/files.budget.json'),
    ('confirm-source', 'tests/integration/confirm.c', 'system/policy/confirm.c',
     'if (src != INK_SRC_TOUCH && src != INK_SRC_KEY) {', 'if (false) {', 'sim-confirm', 'tests/integration/confirm.script', 'tests/integration/confirm.budget.json'),
    ('hidden-ai-tool', 'tests/integration/ai_surface.c', 'system/services/notify/notify_caps.c',
     '.id = "notify.post",', '.id = "notify.post", .expose = INK_EXPOSE_OWNER,', 'sim-ai-surface', 'tests/integration/ai_surface.script', 'tests/integration/ai_surface.budget.json'),
    ('missing-input-schema', 'tests/integration/ai_surface.c', 'system/services/data/data_caps.c',
     None, None, 'sim-ai-surface', 'tests/integration/ai_surface.script', 'tests/integration/ai_surface.budget.json'),
]
results = []
for cid, test_path, source_path, old, new, template, script, budget in cases:
    source = ROOT / source_path
    content = source.read_text()
    if cid == 'missing-input-schema':
        start = content.index('{.id = "data.get",')
        end = content.index('.run = cap_get}', start)
        part = content[start:end]
        changed, count = re.subn(r'     \.input = .*\n', '', part)
        assert count == 1
        content = content[:start] + changed + content[end:]
    else:
        assert content.count(old) == 1, cid
        content = content.replace(old, new)
        if cid == 'snapshot-version':
            # Removing is_log's last use is intentional: suppress only that audit mutation's unused warning.
            content = '#pragma GCC diagnostic ignored "-Wunused-parameter"\n' + content
    mutant = OUT / (cid + '-product.c')
    mutant.write_text(absolute_includes(content, source))
    test = ROOT / test_path
    test_text = absolute_includes(test.read_text(), test)
    extra = []
    if cid == 'snapshot-version':
        test_text = test_text.replace(str(source), str(mutant))
        assert str(mutant) in test_text
    else:
        extra = ['--extra', str(mutant)]
    main = OUT / (cid + '.c')
    main.write_text(test_text)
    cmd = [sys.executable, str(HERE / 'run_probe.py'), str(main), '--template', template,
           '--script', script, '--budget', budget, *extra]
    log = OUT / (cid + '.log')
    with log.open('w') as stream:
        result = subprocess.run(cmd, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    last = ROOT / 'build-audit/audit-probes' / cid / 'last-run.txt'
    metrics_path = Path(last.read_text().strip()) / 'metrics.json' if last.exists() else None
    metrics = json.loads(metrics_path.read_text())['metrics'] if metrics_path and metrics_path.exists() else {}
    evidence = [line.strip() for line in log.read_text().splitlines() if 'CHECK FAILED' in line or 'budget:' in line]
    row = dict(id=cid, exit_code=result.returncode, test_fail=metrics.get('test.fail', 0),
               test_finished=metrics.get('test.finished', 0), evidence=evidence[:8], source=source_path,
               metrics=str(metrics_path.relative_to(ROOT)) if metrics_path else None)
    results.append(row)
    print(json.dumps(row, ensure_ascii=False), flush=True)
    (OUT / 'results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')
