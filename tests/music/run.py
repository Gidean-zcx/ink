"""Run the real music app on synthetic storage, without opening a window."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-music'
area = build / 'music-evidence' / binary.name
shutil.rmtree(area, ignore_errors=True)
sd = area / 'sd' / 'content' / 'audio'
sd.mkdir(parents=True)
for source in (ROOT / 'tests/music/fixtures').iterdir():
    if source.suffix != '.pcm':
        shutil.copyfile(source, sd / source.name)
(sd / '文件夹').mkdir()
shutil.copyfile(sd / 'tone-16000-1.flac', sd / '文件夹' / '独立曲目.flac')
out = area / 'out'
r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
    '--end', '180s', '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'),
    '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')],
    capture_output=True, text=True, timeout=120,
    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
(area / 'run.log').write_text(r.stdout + r.stderr)
data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
m = data.get('metrics', {})
print(f'music: exit={r.returncode}, pass={m.get("test.pass", 0)}, fail={m.get("test.fail", 0)}, finished={m.get("test.finished", 0)}', flush=True)
if r.returncode or m.get('test.fail', 0) or m.get('test.finished') != 1 or data.get('warnings'):
    raise SystemExit(r.stdout[-1000:] + r.stderr[-16000:] + '\n' + '\n'.join(data.get('warnings', [])))
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
