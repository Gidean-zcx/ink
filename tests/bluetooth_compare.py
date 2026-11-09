"""Compare the tested, settled Bluetooth frame with the read-only design export."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--design', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
source = build / 'test-out/bluetooth/write'
out = build / 'bluetooth-compare'
target = out / 'SetBluetooth'
target.mkdir(parents=True, exist_ok=True)
shutil.copyfile(source / 'snap-SetBluetooth.png', target / 'snap-scene.png')
shutil.copyfile(source / 'metrics.json', target / 'metrics.json')
subprocess.run([sys.executable, str(ROOT / 'tools/ui_compare.py'), '--build', str(build),
    '--design', str(a.design.resolve()), '--out', str(out), '--reuse', '--scenes', 'SetBluetooth'],
    check=True, cwd=ROOT)
