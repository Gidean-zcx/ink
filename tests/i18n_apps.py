"""Capture the migrated application pages in both languages, keeping checked frames in test-out."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--case', choices=['apps', 'memo'], required=True)
a = p.parse_args()
build = a.build.resolve()
for lang in ('zh', 'en'):
    out = build / 'test-out' / 'i18n-apps' / a.case / lang
    if out.exists():
        shutil.rmtree(out)
    run = subprocess.run([str(build / f'sim-i18n-{a.case}'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/i18n_apps.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(out),
        '--quiet', '--no-random-outages'], cwd=ROOT, capture_output=True, text=True, timeout=300,
        env={**os.environ, 'INK_I18N_APPS_LANG': lang, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    out.mkdir(parents=True, exist_ok=True)
    (out / 'stdout.log').write_text(run.stdout)
    (out / 'stderr.log').write_text(run.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    m = data.get('metrics', {})
    print(a.case, lang, 'exit', run.returncode, 'checks', m.get('test.pass', 0),
          'failures', m.get('test.fail', 0), 'frames', m.get('test.frames', 0), flush=True)
    if run.returncode or m.get('test.finished') != 1 or m.get('test.fail', 0) or data.get('warnings'):
        raise SystemExit(run.stderr[-12000:] + '\n' + str(data.get('warnings', [])))
    pages = [json.loads(line) for line in (out / 'pages.ndjson').read_text().splitlines()]
    if len(pages) != m['test.frames'] or any(not (out / f'snap-{x["scene"]}.png').is_file() for x in pages):
        raise SystemExit('Missing screenshot evidence')
