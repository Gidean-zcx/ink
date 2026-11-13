"""Run the real music app on synthetic storage, without opening a window."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import wave

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--format', choices=['mp3', 'flac', 'wav', 'ogg'])
p.add_argument('--phases', default='main', help='space-separated INK_MUSIC_PHASE values, one process each on one flash image')
p.add_argument('--no-font', action='store_true', help='leave the family font off the card')
p.add_argument('--content', choices=['full', 'flat', 'empty'], default='full', help='the player test: songs with folders, flat songs only, or none')
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-music'
area = build / 'music-evidence' / (binary.name + ('-' + a.format if a.format else '') + ('-' + a.content if a.content != 'full' else '') + ('-nofont' if a.no_font else ''))
shutil.rmtree(area, ignore_errors=True)


def family_font():
    """The family's one font copy (/family/fonts/InkSC.ttf): Mogo's tool, made once per build from the CJK font."""
    font = build / 'music-font' / 'InkSC.ttf'
    if not font.exists():
        source = next(line.split('=', 1)[1] for line in (build / 'CMakeCache.txt').read_text().splitlines() if line.startswith('INK_CJK_FONT:'))
        python = next((x for x in dict.fromkeys([sys.executable, shutil.which('python3')]) if x and subprocess.run([x, '-c', 'import fontTools'], capture_output=True).returncode == 0), None)
        assert python, 'fontTools is required to make the music page font'
        subprocess.run([python, str(ROOT / 'tools/mogo_font.py'), source, str(font)], check=True, capture_output=True)
    return font


def tone(path, seconds, rate=16000):
    with wave.open(str(path), 'wb') as song:
        song.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
        pcm = (ROOT / 'tests/music/fixtures/tone-16000-1.wav.pcm').read_bytes()
        song.writeframes((pcm * (seconds // 3 + 1))[:seconds * rate * 2])


player = a.binary is not None and a.binary.name.startswith('sim-music-player')
sd = area / 'sd' / 'content' / 'audio'
sd.mkdir(parents=True)
for source in (ROOT / 'tests/music/fixtures').iterdir():
    if source.suffix != '.pcm' and a.content == 'full' and not (player and source.name == 'bad.mp3'):
        shutil.copyfile(source, sd / source.name)
if a.content == 'full':
    (sd / '文件夹').mkdir()
    shutil.copyfile(sd / 'tone-16000-1.flac', sd / '文件夹' / '独立曲目.flac')
    tone(sd / '文件夹' / '快进测试.wav', 30)
if not a.no_font:
    (area / 'sd/content/fonts').mkdir(parents=True)
    shutil.copyfile(family_font(), area / 'sd/content/fonts/InkSC.ttf')
if player and a.content != 'empty':
    # The owner's own songs (/me/音乐), with a sub-folder and a format this firmware cannot play yet.
    mine = area / 'sd/content/.m/owner/音乐'
    mine.mkdir(parents=True)
    for name, seconds in (('甲.wav', 20), ('乙.wav', 20), ('丙.wav', 20)):
        tone(mine / name, seconds)
    shutil.copyfile(ROOT / 'tests/music/fixtures/tone-16000-1.lrc', mine / '甲.lrc')
    if a.content == 'full':
        (mine / '专辑').mkdir()
        tone(mine / '专辑' / '丁.wav', 4)
        tone(mine / '长歌.wav', 90)
        tone(sd / '0首.wav', 20)  # the first song of the library, long enough to look at
        shutil.copyfile(ROOT / 'tests/music/fixtures/tone-16000-1.lrc', mine / '长歌.lrc')
        (mine / '不能放.m4a').write_bytes(b'\0\0\0\x18ftypM4A synthetic')
        (area / 'sd/content/.m/kid/音乐').mkdir(parents=True)
        tone(area / 'sd/content/.m/kid/音乐/孩子.wav', 4)
out = area / 'out'
for phase in a.phases.split():
    run_out = out if phase == 'main' else area / ('out-' + phase)
    r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(run_out),
        '--end', '900s' if player else '180s', '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'),
        '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')],
        capture_output=True, text=True, errors='replace', timeout=280,
        env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1', 'INK_MUSIC_PHASE': phase, 'INK_MUSIC_CONTENT': a.content,
             **({'INK_MUSIC_STACK_FORMAT': a.format} if a.format else {})})
    (area / (phase + '.log')).write_text(r.stdout + r.stderr)
    data = json.loads((run_out / 'metrics.json').read_text(errors='replace')) if (run_out / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    print(f'music {phase}: exit={r.returncode}, pass={m.get("test.pass", 0)}, fail={m.get("test.fail", 0)}, finished={m.get("test.finished", 0)}', flush=True)
    if r.returncode or m.get('test.fail', 0) or m.get('test.finished') != 1 or data.get('warnings'):
        raise SystemExit(r.stdout[-1000:] + r.stderr[-16000:] + '\n' + '\n'.join(data.get('warnings', [])))
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
