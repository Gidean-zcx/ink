"""Remove each required upstream free in an isolated object; the reopen check must fail."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = Path(tempfile.mkdtemp(prefix='google-reopen-reverse-', dir=build))
db = json.loads((build / 'compile_commands.json').read_text())
base = ROOT / 'system/services/text/vendor/googlepinyin/share'
changes = [
    ('spelling', 'spellingtrie.cpp', '  if (root_) { free_son_trie(root_); ink_gp_delete(root_); }\n  ink_gp_delete(dumb_node_);\n  ink_gp_delete(splitter_node_);\n', ''),
    ('index', 'dicttrie.cpp', '  if (NULL != lma_idx_buf_)\n    free(lma_idx_buf_);\n  lma_idx_buf_ = NULL;\n', ''),
    ('sync', 'userdict.cpp', '  free(syncs_);\n', ''),
]
for name, unit, before, after in changes:
    source = base / unit
    original = source.read_text()
    assert original.count(before) == 1
    folder = area / name
    folder.mkdir()
    mutated = folder / unit
    mutated.write_text(original.replace(before, after))
    entry = next(e for e in db if Path(e['file']) == source and 'test-s31-google-ime.dir' in e['command'])
    obj = folder / (unit + '.o')
    command = shlex.split(entry['command'])
    old_obj = command[command.index('-o') + 1]
    command[command.index('-o') + 1] = str(obj)
    command[command.index(str(source))] = str(mutated)
    command += ['-I' + str(source.parent)]
    subprocess.run(command, cwd=entry['directory'], check=True, capture_output=True, text=True)
    link = shlex.split((build / 'CMakeFiles/test-s31-google-ime.dir/link.txt').read_text())
    binary = folder / 'test-s31-google-ime'
    link[link.index('-o') + 1] = str(binary)
    link = [str(obj) if x == old_obj else x for x in link]
    # CMake writes a relative object path in the link command.
    assert str(obj) in link
    subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
    r = subprocess.run([str(binary), str(ROOT / 'assets/ime/dict_pinyin.dat')], capture_output=True, text=True)
    (folder / 'stderr.txt').write_text(r.stderr)
    assert r.returncode != 0 and 'retained' in r.stderr, (name, r.returncode, r.stderr)
    print(name, 'exit', r.returncode, r.stderr.splitlines()[0], flush=True)
print('evidence:', area)
