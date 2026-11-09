"""Capture each real settings scene, then use the project's comparison tool."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--design', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
out = build / 'settings-b-compare'
pages = ['SetMembers', 'SetKid', 'Wallpaper', 'Records', 'AiMemory']
for page in pages:
    target = out / page
    target.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(build / 'sim-settings-b'), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/settings_b.script'), '--assets', str(build / 'assets'),
        '--content', str(build / 'test-content'), '--out', str(target), '--no-random-outages', '--quiet'],
        cwd=ROOT, env={**os.environ, 'INK_SETTINGS_B_SCENE': page}, capture_output=True, text=True, timeout=120)
    (target / 'stdout.log').write_text(result.stdout)
    (target / 'stderr.log').write_text(result.stderr)
    if result.returncode:
        sys.exit(f'{page}: {result.stderr}')
subprocess.run([sys.executable, str(ROOT / 'tools/ui_compare.py'), '--build', str(build),
    '--design', str(a.design.resolve()), '--out', str(out), '--reuse', '--scenes', *pages], check=True, cwd=ROOT)
