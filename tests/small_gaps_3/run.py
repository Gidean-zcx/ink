"""Real services, synthetic inputs; a manual binary override supports reverse checks."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--case', choices=['backup', 'sample', 'notes', 'empty-notes'], required=True)
p.add_argument('--binary', type=Path)
a = p.parse_args()
build = a.build.resolve()
binary = a.binary.resolve() if a.binary else build / 'sim-small-gaps-3'
area = build / 'test-out' / ('small-gaps-3-' + a.case + ('-reverse' if a.binary else ''))
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
peer = area / 'hub'
peer.mkdir()
package = area / 'test-only.inkfw'
subprocess.run([sys.executable, str(ROOT / 'tools/inkfw/make_inkfw.py'), str(package),
                '--version', '1.3.0', '--code', '130', '--app-size', '1024'], check=True, capture_output=True)
notes = '合成开头\n' + ''.join(f'第{i}项：这是一段合成更新说明，用来验证完整分页，不访问真实服务器。\n' for i in range(1, 45)) + '合成结尾'
env = dict(os.environ, INK_GAPS_CASE=a.case, INK_SIM_BACKUP_DIR=str(peer),
           INK_SIM_UPDATE_PKG=str(package), UBSAN_OPTIONS='halt_on_error=1')
if a.case == 'notes':
    env['INK_SIM_UPDATE_NOTES'] = notes
else:
    env.pop('INK_SIM_UPDATE_NOTES', None)
command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
           '--script', str(ROOT / 'tests/small_gaps_3/scenes.script'), '--assets', str(build / 'assets'),
           '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--content', str(build / 'test-content'), '--out', str(area / 'device'), '--quiet', '--no-random-outages']
r = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=180)
(area / 'test.log').write_text(r.stdout + r.stderr)
metrics_file = area / 'device/metrics.json'
data = json.loads(metrics_file.read_text()) if metrics_file.exists() else {}
counts = data.get('metrics', {})
summary = {'case': a.case, 'exit': r.returncode, 'checks': counts.get('test.pass', 0),
           'failed': counts.get('test.fail', 0), 'finished': counts.get('test.finished', 0), 'warnings': data.get('warnings', [])}
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(summary, ensure_ascii=False), flush=True)
ok = r.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail') and not data.get('warnings')
if not ok:
    print(r.stderr[-4000:])
if ok and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
raise SystemExit(0 if ok else 1)
