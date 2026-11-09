"""Compile narrow counterexamples without editing product sources."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'storage-reverse'
area.mkdir(parents=True, exist_ok=True)
path = ROOT / 'system/services/file/file_service.c'
before = hashlib.sha256(path.read_bytes()).hexdigest()
source = path.read_text()
ui_path = ROOT / 'system/ui/shell/settings_pages.c'
ui_before = hashlib.sha256(ui_path.read_bytes()).hexdigest()
ui_source = ui_path.read_text()
flags = (build / 'CMakeFiles/sim-storage-usage.dir/flags.make').read_text()
options = []
for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
    options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
cases = [
    ('member-scope', 'if (index_under(e.path, own)) bytes[0][k]', 'if (true) bytes[0][k]', 'owner category'),
    ('user-directory', 'int k = cache ? 1 : ext && !strcmp(ext, TMP_SUFFIX) ? 0 : ext && !strcmp(ext, PART_SUFFIX) ? 2 : -1;',
     'int k = cache ? 1 : (ext ? 0 : 0);', 'ordinary user files, suffix directories and backups survive cleanup'),
    ('confirmation-title', '.title = "清理临时文件", .effect = INK_FX_LOCAL_DESTRUCTIVE',
     '.title = "清理临时文件、缓存和未传完的文件", .effect = INK_FX_LOCAL_DESTRUCTIVE', 'confirmation title stays complete UTF-8'),
    ('usage-cache', 'bool cached = ready && storage.revision == revision',
     'bool cached = false && ready && storage.revision == revision', 'second query reads no indexed file sizes'),
    ('card-stat', 'int64_t size = f ? 16 : -1;', 'int64_t size = f ? hal_fs_size(INDEX_CARD) : -1;',
     'second query reads no indexed file sizes'),
    ('usage-rescan', 'bool ready = !r && storage_index_ready();',
     'bool ready = !r && storage_index_ready();\n    if (!r && !ready) r = index_scan(&c, db, card, "/family");',
     'unbuilt index reports organizing without scanning the card'),
    ('usage-expired', 'ready = ready && inverse_ready(revision) && version',
     'ready = ready && version', 'write during size lookup discards the expired revision'),
    ('index-notify', 'storage_index_ready() && (!was_ready || version != ink_db_last_ver(db))',
     'storage_index_ready() && (true || !was_ready || version != ink_db_last_ver(db))',
     'unchanged repeated indexing emits no file change'),
    ('storage-units', 'unit == 1024 ? "KB" : unit == 1048576 ? "MB" : "GB"',
     'unit == 1024 ? "KiB" : unit == 1048576 ? "MiB" : "GiB"', 'production snapshot uses'),
    ('free-units', 'unit > 1024 * 1024 ? "GB" : unit > 1024 ? "MB" : "KB"',
     'unit > 1024 * 1024 ? "GB" : unit > 1024 ? "MB" : "KiB"', 'remaining space also uses'),
]
start = source.index('    uint32_t version = db ? ink_db_last_ver(db) : 0;')
end = source.index('    ink_db_close(db);', start)
locked = source[start:end]
cases.append(('usage-lock', locked, locked.replace('    osal_mutex_unlock(FS.edits);\n', '').replace(
    '    osal_mutex_lock(FS.edits, OSAL_WAIT_FOREVER);\n', ''), 'size reads hold no file edit lock'))
summary = []
for name, needle, replacement, expected in cases:
    ui = name in ('storage-units', 'free-units')
    original = ui_source if ui else source
    assert original.count(needle) == 1, name
    changed = area / (name + '.c')
    changed.write_text(original.replace(needle, replacement))
    obj, binary = area / (name + '.o'), area / name
    link = shlex.split((build / 'CMakeFiles/sim-storage-usage.dir/link.txt').read_text())
    cmd = [link[0], *options, '-iquote', str((ui_path if ui else path).parent)]
    if not ui:
        cmd += ['-DINK_STORAGE_SOURCE=' + json.dumps(str(changed))]
    cmd += ['-c', str(changed if ui else ROOT / 'tests/storage_usage/boundary.c'), '-o', str(obj)]
    subprocess.run(cmd, cwd=build, check=True, capture_output=True)
    if ui:
        link.insert(1, str(obj))
    else:
        boundary = next(i for i, part in enumerate(link) if part.endswith('/boundary.c.o'))
        link[boundary] = str(obj)
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    result = subprocess.run([os.sys.executable, str(ROOT / 'tests/storage_usage/run.py'), '--build', str(build), '--binary', str(binary)],
                            cwd=ROOT, capture_output=True, text=True, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, timeout=180)
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode and expected in result.stdout + result.stderr, (name, result.stdout, result.stderr)
    metrics = json.loads((build / 'test-out' / ('storage-usage-' + name) / 'device/metrics.json').read_text())['metrics']
    assert metrics.get('test.finished') == 1 and metrics.get('test.fail', 0) > 0, name
    summary.append({'case': name, 'exit': result.returncode, 'failures': metrics['test.fail'], 'expected': expected})
    print(f'{name}: expected failure (exit={result.returncode}, failures={metrics["test.fail"]})', flush=True)
assert before == hashlib.sha256(path.read_bytes()).hexdigest()
assert ui_before == hashlib.sha256(ui_path.read_bytes()).hexdigest()
(area / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
