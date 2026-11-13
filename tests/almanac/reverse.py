"""Change one day's Yi in a temporary vendor copy; the unmodified fixture test must fail."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
vendor = ROOT / 'system/services/time/vendor/mogo-calendar'
source = vendor / 'main/time/calendar.c'
database = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in database if e['file'] == str(source) and 'almanac-model.dir' in e['command'])
with tempfile.TemporaryDirectory(prefix='almanac-reverse-', dir=build) as folder:
    work = Path(folder)
    original = source.read_text()
    needle = 'out->guardian_type=calendar_folk[calendar_day_roles[role][2]];'
    assert original.count(needle) == 1
    changed = original.replace(needle, needle + '\n    if(year==2026&&month==2&&day==17)out->yi="故意改错";')
    modified = work / 'calendar.c'
    modified.write_text(changed)
    obj = work / 'calendar.o'
    command = shlex.split(entry['command'])
    command[command.index('-o') + 1] = str(obj)
    command[command.index(str(source))] = str(modified)
    command += ['-I' + str(vendor / 'main/time')]
    subprocess.run(command, cwd=entry['directory'], check=True)
    link = shlex.split((build / 'CMakeFiles/almanac-model.dir/link.txt').read_text())
    binary = work / 'almanac-mutated'
    link[link.index('-o') + 1] = str(binary)
    index = next(i for i, word in enumerate(link) if word.endswith('/calendar.c.o'))
    link[index] = str(obj)
    subprocess.run(link, cwd=build, check=True)
    result = subprocess.run([os.sys.executable, str(ROOT / 'tests/almanac/run.py'),
                             '--binary', str(binary)], capture_output=True, text=True)
    assert result.returncode != 0 and '故意改错' in result.stderr and '2026, 2, 17' in result.stderr, result.stdout + result.stderr
    print('PASS reverse control: only 2026-02-17 Yi was corrupted; unchanged 50-day test failed (exit=1)')
