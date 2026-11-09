"""Remove each UI fix in a replacement object; never edit the product sources."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'backup-pages-reverse'
area.mkdir(parents=True, exist_ok=True)
source = ROOT / 'system/ui/shell/backup_pages.c'
original = source.read_text()
before = hashlib.sha256(source.read_bytes()).hexdigest()
cases = [
    ('summary', '"{\\"summary\\":true}"', '"{\\"summary\\":false}"',
     'large manifest produces a usable summary preview', 'summary'),
    ('failure-visible',
     '    if (!changed && (B.page == BACKUP_FAIL || B.page == RESTORE_FAIL) &&\n'
     '        (!strcmp(phase, "none") || !strcmp(phase, "saved") || !strcmp(phase, "preview") || !strcmp(phase, "done"))) { osal_free(j); return; }\n',
     '', 'page BackupFail is presented', 'summary'),
    ('ai-routing',
     '    if (!strcmp(phase, "collecting") || !strcmp(phase, "uploading") || !strcmp(phase, "saved")) B.restore = false;\n',
     '', 'AI backup replaces a restore preview with Backup', 'routing')]
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
commands, summary = [], []


def run(cmd, **kwargs):
    commands.append([str(part) for part in cmd])
    return subprocess.run(cmd, cwd=build, capture_output=True, text=True, timeout=180, **kwargs)


for name, needle, replacement, expected, probe in cases:
    assert needle in original, name
    obj, binary = area / (name + '.o'), area / name
    link = shlex.split((build / 'CMakeFiles/sim-backup-pages.dir/link.txt').read_text())
    compiled = run([link[0], *options, '-iquote', str(source.parent), '-x', 'c', '-', '-c', '-o', str(obj)],
                   input=original.replace(needle, replacement))
    assert compiled.returncode == 0, compiled.stderr
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, arg in enumerate(link) if arg.endswith('libink_system.a')), str(obj))
    linked = run(link)
    assert linked.returncode == 0, linked.stderr
    result = run([os.sys.executable, str(ROOT / 'tests/backup_pages.py'), '--build', str(build), '--binary', str(binary)],
                 env={**os.environ, 'INK_BACKUP_PAGE_PROBE': probe, 'INK_KEEP_EVIDENCE': '1'})
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode != 0 and expected in result.stdout + result.stderr, (name, result.stdout, result.stderr)
    metrics = json.loads((build / 'test-out' / ('backup-pages-' + name) / 'device/metrics.json').read_text())['metrics']
    assert metrics.get('test.finished') == 1 and metrics.get('test.fail', 0) > 0, name
    summary.append({'case': name, 'exit': result.returncode, 'failures': metrics['test.fail'], 'expected': expected})
    print(f'{name}: expected test failure, exit={result.returncode}', flush=True)
assert before == hashlib.sha256(source.read_bytes()).hexdigest()
(area / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
