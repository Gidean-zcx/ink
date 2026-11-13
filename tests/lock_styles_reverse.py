"""lock-styles-1 reverse controls: change one rule in a temporary copy of the product, relink the lock styles
simulator with it, and the unchanged case must fail. Production sources stay as they are."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--only')
a = p.parse_args()
build = a.build.resolve()
database = json.loads((build / 'compile_commands.json').read_text())
DRAWN, GLUE = ROOT / 'system/ui/lock_faces/drawn.c', ROOT / 'apps/mogo_lock/mogo_lock.c'
CONTROLS = [
    ('refresh', DRAWN, 'refresh', '        for (int y = rects[i].y1; y <= rects[i].y2; y++)',
     '        rects[i] = (lv_area_t){0, 0, INK_LOCK_CANVAS_W - 1, INK_LOCK_CANVAS_H - 1}; /* reverse: the whole canvas */\n'
     '        for (int y = rects[i].y1; y <= rects[i].y2; y++)'),
    ('almanac', GLUE, 'almanac', 'ink_almanac_date(r->now.year, r->now.month, r->now.day, a)',
     'ink_almanac_date(r->now.year, r->now.month, r->now.day % 28 + 1, a) /* reverse: another day */'),
    ('privacy-key', DRAWN, 'privacy',
     'static bool personal(int style) { return style == INK_LOCK_TICKET || style == INK_LOCK_COLLAGE || style == INK_LOCK_LIBRARY; }',
     'static bool personal(int style) { return style < 0; } /* reverse: books never depend on the open space */'),
    ('privacy-draw', GLUE, 'privacy', '    if (!r->open) return closed(fb, r, out);\n    last_book();', '    last_book();'),
]
link = shlex.split((build / 'CMakeFiles/sim-lock-styles.dir/link.txt').read_text())
for name, source, case, needle, replacement in CONTROLS:
    if a.only and a.only != name:
        continue
    entry = next(e for e in database if e['file'] == str(source))
    with tempfile.TemporaryDirectory(prefix='lock-styles-reverse-', dir=build) as folder:
        work = Path(folder)
        text = source.read_text()
        assert text.count(needle) == 1, (name, needle)
        mutated = work / source.name
        mutated.write_text(text.replace(needle, replacement))
        obj = work / (source.stem + '.o')
        command = shlex.split(entry['command'])
        command[command.index('-o') + 1] = str(obj)
        command[command.index(str(source))] = str(mutated)
        command += ['-I' + str(source.parent), '-iquote', str(source.parent)]
        subprocess.run(command, cwd=entry['directory'], check=True)
        binary = work / 'sim-lock-styles'
        words = list(link)
        words[words.index('-o') + 1] = str(binary)
        words.insert(words.index(str(binary)) + 1, str(obj))  # first: the archive's own object is never pulled in
        subprocess.run(words, cwd=build, check=True, capture_output=True)
        r = subprocess.run([os.sys.executable, '-B', str(ROOT / 'tests/lock_styles.py'), '--build', str(build),
                            '--binary', str(binary), '--case', case, '--expect-fail'], capture_output=True, text=True)
        print(r.stdout.strip())
        assert r.returncode == 0, f'reverse control {name}: the {case} case did not fail\n{r.stdout}{r.stderr[-3000:]}'
        print(f'PASS reverse control {name}: {source.name} changed, lock-styles[{case}] failed')
