"""Remove format dispatch in a temporary object; the reference test must fail."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
root = Path(__file__).resolve().parents[2]
source = root / 'system/services/audio/codec_formats.c'
entry = next(e for e in json.loads((build / 'compile_commands.json').read_text())
             if Path(e['file']) == source and 'test-music-formats' in e['command'])
with tempfile.TemporaryDirectory(prefix='music-reverse-', dir=build) as tmp:
    temp = Path(tmp)
    mutant = temp / 'codec_formats.c'
    text = source.read_text()
    assert 'if (!r->kind ||' in text
    mutant.write_text(text.replace('if (!r->kind ||', 'if (true || !r->kind ||', 1))
    obj = temp / 'codec_formats.o'
    cmd = shlex.split(entry['command'])
    cmd[cmd.index('-o') + 1] = str(obj)
    cmd[cmd.index(str(source))] = str(mutant)
    cmd += ['-iquote', str(source.parent)]
    subprocess.run(cmd, cwd=build, check=True)
    link = shlex.split((build / 'CMakeFiles/test-music-formats.dir/link.txt').read_text())
    link = [str(obj) if 'codec_formats.c.o' in x else x for x in link]
    link[link.index('-o') + 1] = str(temp / 'test-reversed')
    subprocess.run(link, cwd=build, check=True)
    result = subprocess.run([str(temp / 'test-reversed'), str(root / 'tests/music/fixtures')],
                            cwd=build, capture_output=True, text=True)
    assert result.returncode != 0, 'reverse control unexpectedly passed'
    print(f'dispatch removed: exit={result.returncode}; reference open assertion failed as required')
    print(result.stderr[-400:])
