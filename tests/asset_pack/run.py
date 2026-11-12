"""Packed assets, PSRAM reclamation and a required icon missing from a page."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix='asset-pack-', dir=build))
binary = a.binary.resolve() if a.binary else build / 'sim-asset-pack'
assert (build / 'assets/icons/plus-line-106.inki').is_file(), 'negative control needs a real excluded raw icon'
for phase in ('map', 'missing'):
    out = work / phase
    r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
                        '--out', str(out), '--end', '20s', '--quiet', '--no-random-outages'],
                       env={**os.environ, **({'INK_ASSET_MISSING': '1'} if phase == 'missing' else {}),
                            'UBSAN_OPTIONS': 'halt_on_error=1'}, capture_output=True, text=True, timeout=90)
    (work / (phase + '.log')).write_text(r.stdout + r.stderr)
    report = json.loads((out / 'metrics.json').read_text())
    m = report['metrics']
    if phase == 'map':
        assert r.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail', 0), r.stderr[-2500:]
        assert not report.get('warnings'), report.get('warnings')
    else:
        assert r.returncode != 0 and m.get('panic.count') == 1 and m.get('warnings.count', 0) >= 1, report
        assert any('icons/plus-line-106.inki' in w for w in report.get('warnings', [])), report
    print(f'{phase}: exit={r.returncode}, panic={m.get("panic.count", 0)}, warnings={m.get("warnings.count", 0)}')
if os.environ.get('INK_KEEP_EVIDENCE'):
    print('evidence:', work.relative_to(ROOT))
else:
    shutil.rmtree(work)
