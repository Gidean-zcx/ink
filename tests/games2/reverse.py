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
p.add_argument('--case', help='Run only this mutation; the default runs all cases')
a = p.parse_args()
build = a.build.resolve()
area = build / 'games2-reverse'
area.mkdir(exist_ok=True)
db = json.loads((build / 'compile_commands.json').read_text())
cases = [
    ('memory-pair', 'memory/engine.c', 's->pairs++;', 's->pairs += 0;', 'all pairs complete with counted attempts'),
    ('memory-hide', 'memory/engine.c', 's->open[s->first] = s->open[s->second] = 0;', 's->open[s->first] = s->open[s->second] = 1;', 'mismatch flips back'),
    ('memory-moves', 'memory/engine.c', 's->moves++;', 's->moves += 0;', 'mismatch counts one attempt'),
    ('snake-growth', 'snake/engine.c', 's->length++;', 's->length += 0;', 'food grows snake and scores'),
    ('snake-self', 'snake/engine.c', 'if (s->body[i] == head)', 'if (false && s->body[i] == head)', 'self collision ends game'),
    ('snake-pause', 'snake/engine.c', 's->paused || s->over', 'false || s->over', 'paused engine stays still'),
    ('memory-member', 'memory/memory.c', '!M.db || M.epoch != ink_member_epoch()', '!M.db', 'memory best scores do not cross members'),
    ('snake-member', 'snake/snake.c', '!S.db || S.epoch != ink_member_epoch()', '!S.db', 'snake high scores do not cross members'),
    ('snake-leave', 'snake/snake.c', 'if (S.timer) lv_timer_delete(S.timer);\n    S.timer = NULL; S.s.paused = true;', 'S.timer = NULL;', 'leaving page stops snake'),
    ('memory-best', 'memory/memory.c', 'else M.best = best;', 'else M.best = (best_t){0};', 'best moves and elapsed seconds stored'),
    ('snake-best', 'snake/snake.c', 'S.best = best;', 'S.best = 0;', 'AI-guided eating updates length and member high score'),
    ('memory-background', 'memory/memory.c', 'ink_app_open("memory", NULL, NULL); changed();', 'changed();', 'capability and page agree'),
]
summary = []
for name, relative, old, new, expected in cases:
    if a.case and a.case != name:
        continue
    source = ROOT / 'system/apps' / relative
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
    link = shlex.split((build / 'CMakeFiles/sim-games2.dir/link.txt').read_text())
    binary = work / 'sim-games2'
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, v in enumerate(link) if v.endswith('libink_system.a')), str(obj))
    result = subprocess.run(link, cwd=build, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    result = subprocess.run([os.sys.executable, str(ROOT / 'tests/games2/run.py'), '--build', str(build),
        '--binary', str(binary), '--phases', 'main'], cwd=ROOT, capture_output=True, text=True,
        env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, timeout=300)
    output = result.stdout + result.stderr
    (work / 'run.log').write_text(output)
    assert result.returncode != 0 and expected in output, (name, output[-3000:])
    assert hashlib.sha256(source.read_bytes()).hexdigest() == digest
    summary.append({'case': name, 'exit': result.returncode, 'expected_failure': expected})
    print(f'{name}: expected failure, exit={result.returncode}', flush=True)
assert summary, 'unknown case'
(area / (f'summary-{a.case}.json' if a.case else 'summary.json')).write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
