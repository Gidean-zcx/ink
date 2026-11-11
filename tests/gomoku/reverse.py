"""Compile isolated source copies; the unchanged acceptance must catch each removed behavior."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'gomoku-reverse'
area.mkdir(exist_ok=True)
db = json.loads((build / 'compile_commands.json').read_text())
cases = [
    ('state-refresh', 'gomoku.c', '(next != G.cursor && (i == next || i == G.cursor))', '(i == next || i == G.cursor)', 'state reads do not request a refresh'),
    ('win', 'vendor/crossmux/GomokuBoard.cpp', 'if (detectWin(r, c, color))', 'if (false && detectWin(r, c, color))', 'five in a row wins'),
    ('move', 'gomoku.c', 'ai_turn() || ink_gomoku_play(&G.s, 1, (int)r, (int)col)', 'true', 'games.move: invalid'),
    ('member', 'gomoku.c', '!G.db || G.epoch != ink_member_epoch()', '!G.db', 'move count 0'),
]
summary = []
for name, relative, old, new, expected in cases:
    source = ROOT / 'system/apps/gomoku' / relative
    original = source.read_text()
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    assert original.count(old) == 1, name
    work = area / name
    work.mkdir(exist_ok=True)
    mutant = work / source.name
    mutant.write_text(original.replace(old, new))
    entry = next(e for e in db if Path(e['file']) == source)
    compile_cmd = shlex.split(entry['command']) if 'command' in entry else entry['arguments'].copy()
    obj = work / 'mutant.o'
    compile_cmd[compile_cmd.index('-o') + 1] = str(obj)
    compile_cmd = [str(mutant) if v == str(source) else v for v in compile_cmd]
    compile_cmd += ['-I' + str(source.parent)]
    result = subprocess.run(compile_cmd, cwd=entry['directory'], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    link = shlex.split((build / 'CMakeFiles/sim-gomoku.dir/link.txt').read_text())
    binary = work / 'sim-gomoku'
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, v in enumerate(link) if v.endswith('libink_system.a')), str(obj))
    result = subprocess.run(link, cwd=build, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([os.sys.executable, str(ROOT / 'tests/gomoku/run.py'), '--build', str(build),
        '--binary', str(binary), '--phases', 'main'], cwd=ROOT, capture_output=True, text=True,
        env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, timeout=300)
    output = result.stdout + result.stderr
    (work / 'run.log').write_text(output)
    assert result.returncode != 0 and expected in output, (name, output[-3000:])
    assert hashlib.sha256(source.read_bytes()).hexdigest() == digest
    summary.append({'case': name, 'exit': result.returncode, 'expected_failure': expected})
    print(f'{name}: expected failure, exit={result.returncode}', flush=True)
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
