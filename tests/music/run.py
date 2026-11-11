"""Run the real music app on synthetic storage, without opening a window."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import wave

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
p.add_argument('--format', choices=['mp3', 'flac', 'wav', 'ogg'])
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-music'
area = build / 'music-evidence' / (binary.name + ('-' + a.format if a.format else ''))
shutil.rmtree(area, ignore_errors=True)
sd = area / 'sd' / 'content' / 'audio'
sd.mkdir(parents=True)
for source in (ROOT / 'tests/music/fixtures').iterdir():
    if source.suffix != '.pcm':
        shutil.copyfile(source, sd / source.name)
(sd / '文件夹').mkdir()
shutil.copyfile(sd / 'tone-16000-1.flac', sd / '文件夹' / '独立曲目.flac')
with wave.open(str(sd / '文件夹' / '快进测试.wav'), 'wb') as song:
    song.setparams((1, 2, 16000, 0, 'NONE', 'not compressed'))
    song.writeframes((ROOT / 'tests/music/fixtures/tone-16000-1.wav.pcm').read_bytes() * 10)
out = area / 'out'
r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
    '--end', '180s', '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'),
    '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')],
    capture_output=True, text=True, timeout=120,
    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1',
         **({'INK_MUSIC_STACK_FORMAT': a.format} if a.format else {})})
(area / 'run.log').write_text(r.stdout + r.stderr)
data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
m = data.get('metrics', {})
print(f'music: exit={r.returncode}, pass={m.get("test.pass", 0)}, fail={m.get("test.fail", 0)}, finished={m.get("test.finished", 0)}', flush=True)
if r.returncode or m.get('test.fail', 0) or m.get('test.finished') != 1 or data.get('warnings'):
    raise SystemExit(r.stdout[-1000:] + r.stderr[-16000:] + '\n' + '\n'.join(data.get('warnings', [])))
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
