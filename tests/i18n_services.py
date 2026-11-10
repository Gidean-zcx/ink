"""Real service receipts, confirmation cards and interruptions in both languages."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--negative', action='store_true')
a = p.parse_args()
build = a.build.resolve()
name = 'sim-i18n-services' + ('-negative' if a.negative else '')
out = build / 'test-out' / name
shutil.rmtree(out, ignore_errors=True)
out.mkdir(parents=True)
r = subprocess.run([str(build / name), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--script', str(ROOT / 'tests/i18n_lang.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
    '--quiet', '--no-random-outages'], cwd=ROOT, capture_output=True, text=True, timeout=150,
    env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
(out / 'run.log').write_text(r.stdout + r.stderr)
data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
counts = data.get('metrics', {})
print(f'{name}: exit={r.returncode} pass={counts.get("test.pass", 0)} '
      f'fail={counts.get("test.fail", 0)} missing={counts.get("i18n.missing", 0)}', flush=True)
if a.negative:
    if not counts.get('test.fail') or not any('translated confirmation title and template' in w for w in data.get('warnings', [])):
        raise SystemExit('negative control did not detect untranslated confirmation: ' + r.stderr[-6000:])
else:
    if r.returncode or counts.get('test.fail') or counts.get('test.finished') != 1 or data.get('warnings'):
        raise SystemExit(r.stderr[-12000:] + '\n' + str(data.get('warnings', [])))
    for frame in 'ConfirmCall-en ConfirmSetting-en Notifications-en Notifications-zh Channels-en Channels2-en Battery-en IncomingCall-en ActiveCall-en Alarm-en'.split():
        if not (out / f'snap-{frame}.png').is_file():
            raise SystemExit(f'missing frame: {frame}')
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(out)
