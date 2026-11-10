"""i18n-1: switch to English and back on the real first-boot and settings pages, then restart on the same
flash and find English kept. Frames stay in test-out/i18n-lang/<phase>/snap-*.png."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FRAMES = {
    'switch': 'FirstBoot-zh FirstBoot-en BootWifi-en BootNoHub-en BootHello-en AiAnswer-en Settings-en SetAbout-en Settings-zh'.split(),
    'restart': ['Settings-en-restart'],
}
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'test-out' / 'i18n-lang'
if area.exists():
    shutil.rmtree(area)
area.mkdir(parents=True)
for phase, frames in FRAMES.items():
    out = area / phase
    result = subprocess.run([str(build / 'sim-i18n-lang'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/i18n_lang.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--flash-image', str(area / 'user.img'), '--sd-dir', str(area / 'sd'), '--quiet', '--no-random-outages'],
        cwd=ROOT, capture_output=True, text=True, timeout=120,
        env={**os.environ, 'INK_I18N_PHASE': phase, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    out.mkdir(parents=True, exist_ok=True)
    (out / 'stdout.log').write_text(result.stdout)
    (out / 'stderr.log').write_text(result.stderr)
    data = json.loads((out / 'metrics.json').read_text(errors='replace')) if (out / 'metrics.json').exists() else {}
    counts = data.get('metrics', {})
    if result.returncode or counts.get('test.finished') != 1 or counts.get('test.fail', 0) or data.get('warnings'):
        raise SystemExit(f'{phase}: FAIL exit={result.returncode}\n{result.stderr[-6000:]}\n{data.get("warnings", [])}')
    for frame in frames:
        if counts.get(f'test.snapshot.{frame}') != 1 or not (out / f'snap-{frame}.png').is_file():
            raise SystemExit(f'{phase}: missing checked frame {frame}')
    if phase == 'switch' and not counts.get('i18n.missing'):
        raise SystemExit('switch: the untranslated key was not counted')
    print(f'{phase}: PASS checks={counts.get("test.pass")} frames={len(frames)}', flush=True)
