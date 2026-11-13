"""Manual reverse controls: each case breaks one thing in a temporary copy and the music tests must fail on it.

original  the unchanged build passes (control)
timeout   music capabilities lose their 30 s deadline: the first call after opening the page times out
stack     the decoders at the --baseline revision overflow the media stack (needs --baseline)
skip      the progress bar seeks with a stale duration (the first song's) instead of the playing song's
rebind    every present registers another touch handler on the page
compat    the favorites are read with another key: the old music's favorites are not found
"""
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
p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline')
p.add_argument('--case', choices=['original', 'timeout', 'stack', 'skip', 'rebind', 'compat'], required=True)
a = p.parse_args()
build = a.build.resolve()
db = json.loads((build / 'compile_commands.json').read_text())
LIBRARY = {'apps/music/music.c': 'ink_apps', 'third_party/kiikoread_music/app_music.c': 'ink_music_native',
           'system/services/audio/codec_formats.c': 'ink_system'}


def baseline(name):
    assert a.baseline, 'this case needs --baseline'
    return subprocess.check_output(['git', 'show', f'{a.baseline}:{name}'], cwd=ROOT, text=True)


def compile_copy(name, text, folder, target):
    source = ROOT / name
    entry = next(e for e in db if Path(e['file']) == source and f'CMakeFiles/{target}.dir' in e['command'])
    copy = folder / source.name
    copy.write_text(text)
    obj = folder / (source.name + '.o')
    cmd = shlex.split(entry['command'])
    cmd[cmd.index('-o') + 1] = str(obj)
    cmd[cmd.index(str(source))] = str(copy)
    cmd += ['-iquote', str(source.parent)]
    subprocess.run(cmd, cwd=build, check=True, capture_output=True, text=True)
    return obj


def mutate(name, old, new):
    text = (ROOT / name).read_text()
    assert text.count(old) == 1, old
    return text.replace(old, new)


player = a.case in ('rebind', 'compat')
with tempfile.TemporaryDirectory(prefix='music-regression-', dir=build) as directory:
    folder = Path(directory)
    changes = {}
    if a.case == 'timeout':
        changes['apps/music/music.c'] = mutate('apps/music/music.c', '.timeout_ms = 30000, ', '')
    elif a.case == 'skip':
        name = 'third_party/kiikoread_music/app_music.c'
        changes[name] = mutate(name, '''        const uint32_t dur = music_duration_ms();
        if (dur) {
            int rel = x - L.bar.x;''', '''        const uint32_t dur = music_track(0) ? music_track(0)->duration_ms : 0;
        if (dur) {
            int rel = x - L.bar.x;''')
    elif a.case == 'rebind':
        changes['apps/music/music.c'] = mutate('apps/music/music.c', 'static void presented_ui(void *arg) {\n    (void)arg;\n',
                                               'static void rebound(lv_event_t *e) { (void)e; }\nstatic void presented_ui(void *arg) {\n    (void)arg;\n'
                                               '    if (U.screen) lv_obj_add_event_cb(U.screen, rebound, LV_EVENT_CLICKED, NULL);\n')
    elif a.case == 'compat':
        changes['apps/music/music.c'] = mutate('apps/music/music.c', 'return (ink_crc32(0, p, strlen(p)) & 0x7fffffff) + 1;',
                                               'return (ink_crc32(0, p, strlen(p)) & 0x7fffffff) + 2;')
    elif a.case == 'stack':
        name = 'system/services/audio/codec_formats.c'
        text = (ROOT / name).read_text()
        for vendor in ['dr_flac.h', 'dr_wav.h', 'stb_vorbis.c']:
            path = folder / vendor
            path.write_text(baseline('third_party/audio_formats/' + vendor))
            text = text.replace('../../../third_party/audio_formats/' + vendor, str(path))
        changes[name] = text
    archives = {}
    for name, text in changes.items():
        library = LIBRARY[name]
        archive = archives.get(library)
        if not archive:
            archive = archives[library] = folder / f'lib{library}.a'
            shutil.copyfile(build / f'lib{library}.a', archive)
        obj = compile_copy(name, text, folder, library)
        subprocess.run(['ar', 'r', str(archive), str(obj)], check=True, capture_output=True)
    target = 'sim-music-player' if player else 'sim-music'
    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    link = [str(archives[Path(x).name[3:-2]]) if Path(x).name.startswith('lib') and Path(x).name[3:-2] in archives else x for x in link]
    binary = build / (target + '-control-' + a.case)
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    r = subprocess.run([sys.executable, '-B', str(ROOT / 'tests/music/run.py'), '--build', str(build), '--binary', str(binary)],
                       capture_output=True, text=True, errors='replace', env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    binary.unlink()
    if a.case == 'original':
        assert r.returncode == 0, r.stdout + r.stderr
    else:
        expected = {'timeout': 'timed out', 'stack': 'may overflow', 'skip': 'skip uses updated progress',
                    'rebind': 'no new event callbacks after 200 refreshes', 'compat': 'the old favorite is a favorite'}[a.case]
        assert r.returncode != 0 and expected in r.stdout + r.stderr, r.stdout[-3000:] + r.stderr[-3000:]
    print(f'{a.case}: exit={r.returncode}; expected outcome verified')
    print(r.stdout.strip())
    for line in r.stderr.splitlines():
        if 'CHECK FAILED' in line or 'may overflow' in line:
            print(line[line.index(')') + 2:] if 'CHECK FAILED' in line else line[line.index('task '):])
