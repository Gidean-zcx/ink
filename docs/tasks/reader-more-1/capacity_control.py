"""Manual settings-b control: remove only the four new registrations in a temporary object."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, required=True)
args = parser.parse_args()
build = args.build.resolve()
source = ROOT / 'apps/reader/reader.c'
entry = next(x for x in json.loads((build / 'compile_commands.json').read_text()) if Path(x['file']) == source)
ids = ['reader.bookmarks', 'reader.add_bookmark', 'reader.remove_bookmark', 'reader.goto_bookmark']
with tempfile.TemporaryDirectory(prefix='reader-capacity-', dir=build) as directory:
    temp = Path(directory)
    lines = source.read_text().splitlines(keepends=True)
    removed = [line for line in lines if any(f'.id="{name}"' in line for name in ids)]
    assert len(removed) == 4
    text = ''.join(line for line in lines if line not in removed)
    text = text.replace('int ink_reader_actions_init(void) {', 'int ink_reader_actions_init(void) {\n    (void)act_bookmarks; (void)act_bookmark;')
    mutant, obj = temp / source.name, temp / 'reader.c.o'
    mutant.write_text(text)
    command, skip = [], False
    for arg in shlex.split(entry['command']):
        if skip:
            skip = False
        elif arg in ('-o', '-MT', '-MF', '-MQ'):
            skip = True
        elif arg != '-MD':
            command.append(str(mutant) if arg == str(source) else arg)
    subprocess.run(command + ['-I', str(source.parent), '-o', str(obj)], cwd=entry['directory'], check=True, capture_output=True)
    library = temp / 'libink_apps.a'
    shutil.copyfile(build / library.name, library)
    subprocess.run(['ar', 'r', str(library), str(obj)], check=True, capture_output=True)
    binary = temp / 'sim-settings-capacity-control'
    link = shlex.split((build / 'CMakeFiles/sim-settings-b.dir/link.txt').read_text())
    link[link.index('-o') + 1] = str(binary)
    link = [str(library) if Path(arg).name == library.name else arg for arg in link]
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    result = subprocess.run([sys.executable, str(ROOT / 'tests/settings_b.py'), '--build', str(build), '--binary', str(binary)],
        cwd=ROOT, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, text=True, capture_output=True)
    print(result.stdout, end='')
    if result.returncode:
        print(result.stderr[-3000:])
        raise SystemExit(result.returncode)
    print('capacity control: unchanged settings-b passes with only the four bookmark registrations removed')
