"""Manually relink integration-races with baseline SMS; never registered in ctest."""
import argparse
import json
import sys
from pathlib import Path
import shlex
import shutil
import subprocess
parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
build = args.build.resolve()
target = build / 'test-out/races-baseline'
target.mkdir(parents=True, exist_ok=True)
original = build / 'test-out/races/metrics.json'
if original.exists() and not (target / 'ordinary-full-metrics.json').exists():
    shutil.copyfile(original, target / 'ordinary-full-metrics.json')
entry = next(e for e in json.loads((build / 'compile_commands.json').read_text()) if Path(e['file']) == root / 'system/apps/sms/sms.c')
source = target / 'sms.c'
source.write_bytes(subprocess.check_output(['git', 'show', '83ec2e4e4483d530866603c2e93474ed30ce96a3:system/apps/sms/sms.c'], cwd=root))
command = shlex.split(entry['command'])
command[command.index('-o') + 1] = str(target / 'sms.o')
command[-1] = str(source)
command[1:1] = ['-iquote', str(root / 'system/apps/sms')]
subprocess.run(command, cwd=build, check=True)
link = shlex.split((build / 'CMakeFiles/sim-races.dir/link.txt').read_text())
link[1:1] = [str(target / 'sms.o')]
link[link.index('-o') + 1] = str(target / 'sim-races-baseline')
subprocess.run(link, cwd=build, capture_output=True, text=True, check=True)
ctest = Path(sys.executable).with_name('ctest')
tests = json.loads(subprocess.check_output([str(ctest) if ctest.exists() else 'ctest', '--test-dir', str(build), '--show-only=json-v1'], text=True))['tests']
command = next(t['command'] for t in tests if t['name'] == 'integration-races')
command[0] = str(target / 'sim-races-baseline')
command[command.index('--out') + 1] = str(target / 'run')
result = subprocess.run(command, capture_output=True, text=True)
(target / 'process.log').write_text(result.stdout + result.stderr)
metrics = json.loads((target / 'run/metrics.json').read_text())
counts = metrics['metrics']
print(f'baseline SMS, integration-races: exit={result.returncode} pass={counts.get("test.pass", 0)} fail={counts.get("test.fail", 0)} finished={counts.get("test.finished", 0)}')
for warning in metrics.get('warnings', []):
    print(warning.replace(str(root) + '/', ''))

raise SystemExit(result.returncode)
