"""Compile replacement objects, leaving product sources and the normal binary unchanged."""
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
build = Path(sys.argv[1]).resolve()
area = build / 'privacy-records-reverse'
area.mkdir(parents=True, exist_ok=True)
flags = (build / 'CMakeFiles/ink_system.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
link = shlex.split((build / 'CMakeFiles/sim-privacy-records.dir/link.txt').read_text())
cases = [
    ('member', 'system/policy/cap.c',
     '(ink_member_current()->role == INK_ROLE_ADULT && m->role == INK_ROLE_CHILD)',
     'true', 'child cannot request adult records'),
    ('deletion', 'system/policy/cap.c',
     'ink_db_del(db, rec.id, c->src)', '(c->src == INK_SRC_SYSTEM ? INK_DB_ERR_ARG : INK_DB_OK)', 'real deletion clears retained and expired records'),
    ('navigation', 'system/ui/shell/settings_family.c',
     '{"shell.ai_log", "shell.ai_home", "shell.ai_memory", "shell.runtime_records"}',
     '{"shell.ai_home", "shell.ai_home", "shell.ai_memory", "shell.runtime_records"}', 'activity row opens AiLog'),
    ('privacy', 'system/ui/shell/settings_pages.c',
     'if (!strcmp(r->id, "privacy.child")) action->args_json = "{\\"children\\":true}";',
     'if (!strcmp(r->id, "privacy.child")) action->args_json = NULL;', 'child privacy entry opens child segment'),
]
summary = []
for name, filename, old, new, expected in cases:
    path = ROOT / filename
    original = path.read_bytes()
    text = original.decode()
    assert text.count(old) == 1, name
    obj, binary = area / (name + '.o'), area / name
    compiled = subprocess.run([link[0], *options, '-iquote', str(path.parent), '-x', 'c', '-', '-c', '-o', str(obj)],
                              input=text.replace(old, new), text=True, capture_output=True, cwd=build)
    assert compiled.returncode == 0, compiled.stderr
    command = link.copy()
    command[command.index('-o') + 1] = str(binary)
    command.insert(next(i for i, part in enumerate(command) if part.endswith('libink_system.a')), str(obj))
    linked = subprocess.run(command, text=True, capture_output=True, cwd=build)
    assert linked.returncode == 0, linked.stderr
    result = subprocess.run([sys.executable, str(ROOT / 'tests/privacy_records/run.py'), '--build', str(build), '--binary', str(binary)],
                            text=True, capture_output=True, timeout=240, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode == 1 and expected in result.stdout + result.stderr, (name, result.stdout, result.stderr)
    metrics = json.loads((build / 'test-out' / ('privacy-records-' + name) / 'metrics.json').read_text())['metrics']
    assert metrics.get('test.fail', 0) > 0
    assert path.read_bytes() == original
    summary.append({'case': name, 'compile_exit': compiled.returncode, 'link_exit': linked.returncode,
                    'test_exit': result.returncode, 'failures': metrics['test.fail'],
                    'source_sha256': hashlib.sha256(original).hexdigest()})
    print(name, 'rejected', 'failures', metrics['test.fail'], flush=True)
    obj.unlink()
    binary.unlink()
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
