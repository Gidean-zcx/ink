"""Manual source reverse controls; no history dependency in registered tests."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--before', required=True)
a = p.parse_args()
build = a.build.resolve()
work = build / 'asset-reverse'
work.mkdir(exist_ok=True)
product = ROOT / 'ports/sim/hal/hal_assets_sim.c'
db = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in db if Path(e['file']) == product)
for mode in ('raw-loader', 'unmap'):
    source = work / (mode + '.c')
    if mode == 'raw-loader':
        source.write_bytes(subprocess.check_output(['git', 'show', f'{a.before}:{product.relative_to(ROOT)}'], cwd=ROOT))
    else:
        old = 'if (data && pools_owns(data)) osal_free((void *)data);'
        assert product.read_text().count(old) == 1
        source.write_text(product.read_text().replace(old, '(void)data;'))
    obj, binary = work / (mode + '.o'), work / mode
    command = shlex.split(entry['command'])
    command[command.index(entry['file'])] = str(source)
    command[command.index('-o') + 1] = str(obj)
    subprocess.run(command, cwd=entry['directory'], capture_output=True, text=True, check=True)
    link = shlex.split((build / 'CMakeFiles/sim-asset-pack.dir/link.txt').read_text())
    link.insert(next(i for i, part in enumerate(link) if part.endswith('.a')), str(obj))
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, capture_output=True, text=True, check=True)
    out = work / (mode + '-out')
    r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
                        '--out', str(out), '--end', '20s', '--quiet', '--no-random-outages'],
                       env={**os.environ, **({'INK_ASSET_MISSING': '1'} if mode == 'raw-loader' else {})},
                       capture_output=True, text=True, timeout=90)
    (work / (mode + '.log')).write_text(r.stdout + r.stderr)
    report = json.loads((out / 'metrics.json').read_text())
    m = report['metrics']
    if mode == 'raw-loader':
        assert r.returncode == 0 and m.get('test.finished') == 1 and not report.get('warnings'), r.stderr[-2500:]
        print('raw-loader: excluded icon renders successfully; missing-icon acceptance correctly rejects the baseline')
    else:
        assert m.get('test.fail', 0) >= 1 and any('unmap reclaims' in w for w in report.get('warnings', [])), r.stderr[-2500:]
        print('unmap: removing reclamation fails the PSRAM assertion')
print('evidence:', work.relative_to(ROOT))
