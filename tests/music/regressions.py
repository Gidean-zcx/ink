"""Manual reverse controls; only this mode needs the supplied git baseline."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline', required=True)
p.add_argument('--case', choices=['original', 'timeout', 'stack', 'skip'], required=True)
a = p.parse_args()
build = a.build.resolve()
db = json.loads((build / 'compile_commands.json').read_text())


def baseline(name):
    return subprocess.check_output(['git', 'show', f'{a.baseline}:{name}'], cwd=ROOT, text=True)


def compile_copy(name, text, folder, target):
    source = ROOT / name
    entry = next(e for e in db if Path(e['file']) == source and target in e['command'])
    copy = folder / source.name
    copy.write_text(text)
    obj = folder / (source.name + '.o')
    cmd = shlex.split(entry['command'])
    cmd[cmd.index('-o') + 1] = str(obj)
    cmd[cmd.index(str(source))] = str(copy)
    cmd += ['-iquote', str(source.parent)]
    subprocess.run(cmd, cwd=build, check=True, capture_output=True, text=True)
    return obj


with tempfile.TemporaryDirectory(prefix='music-regression-', dir=build) as directory:
    folder = Path(directory)
    changes = {}
    if a.case == 'timeout':
        for name in ['system/apps/music/music.c', 'system/services/audio/media_service.c']:
            changes[name] = baseline(name)
    elif a.case == 'skip':
        name = 'system/apps/music/music.c'
        text = (ROOT / name).read_text()
        start = text.index('static void skip_forward(')
        end = text.index('static void slider(', start)
        text = text[:start] + text[end:]
        old = 'ink_ui_pill(s, TR("快进10秒"), INK_MARGIN + w / 2, y, w / 2, INK_CONTROL, false, skip_forward, NULL);'
        assert old in text
        changes[name] = text.replace(old, 'char json[48]; snprintf(json, sizeof json, "{\\"ms\\":%u}", (unsigned)(V.st.position_ms + 10000)); button(s, TR("快进10秒"), "music.seek", json, INK_MARGIN + w / 2, y, w / 2);')
    elif a.case == 'stack':
        name = 'system/services/audio/codec_formats.c'
        text = (ROOT / name).read_text()
        for vendor in ['dr_flac.h', 'dr_wav.h', 'stb_vorbis.c']:
            path = folder / vendor
            path.write_text(baseline('third_party/audio_formats/' + vendor))
            text = text.replace('../../../third_party/audio_formats/' + vendor, str(path))
        changes[name] = text
    archive = folder / 'libink_system.a'
    shutil.copyfile(build / 'libink_system.a', archive)
    for name, text in changes.items():
        obj = compile_copy(name, text, folder, 'ink_system')
        subprocess.run(['ar', 'r', str(archive), str(obj)], check=True, capture_output=True)
    link = shlex.split((build / 'CMakeFiles/sim-music.dir/link.txt').read_text())
    link = [str(archive) if Path(x).name == 'libink_system.a' else x for x in link]
    if a.case in ['original', 'timeout']:
        obj = compile_copy('tests/music/product.c', baseline('tests/music/product.c'), folder, 'sim-music')
        link = [str(obj) if x.endswith('/product.c.o') else x for x in link]
    binary = build / ('music-control-' + a.case)
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    r = subprocess.run([sys.executable, '-B', str(ROOT / 'tests/music/run.py'), '--build', str(build),
                        '--binary', str(binary)], capture_output=True, text=True,
                       env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    if a.case == 'original':
        assert r.returncode == 0, r.stdout + r.stderr
    else:
        expected = {'timeout': 'timed out', 'stack': 'may overflow', 'skip': 'skip uses updated progress'}[a.case]
        assert r.returncode != 0 and expected in r.stdout + r.stderr, r.stdout + r.stderr
    print(f'{a.case}: exit={r.returncode}; expected outcome verified')
    print(r.stdout.strip())
    for line in r.stderr.splitlines():
        if 'CHECK FAILED' in line or 'may overflow' in line:
            print(line[line.index(')') + 2:] if 'CHECK FAILED' in line else line[line.index('task '):])
