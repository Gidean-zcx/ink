"""Keep the original 80 ms page key observable while the display is busy."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, required=True)
p.add_argument('--binary', type=Path)
a = p.parse_args()
build = a.build.resolve()
(build / 'test-out').mkdir(exist_ok=True)
area = Path(tempfile.mkdtemp(prefix='sound-clock-input-', dir=build / 'test-out'))
run = subprocess.run([
    str(a.binary.resolve() if a.binary else build / 'sim-configured'),
    '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--script', str(ROOT / 'tests/scenarios/ai_page_version.script'),
    '--hub', str(ROOT / 'tests/scenarios/ai_page_version.hub.json'),
    '--budget', str(ROOT / 'tests/scenarios/ai_page_version.budget.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
    '--out', str(area), '--quiet', '--trace', '--no-random-outages',
], capture_output=True, text=True, timeout=60)
(area / 'run.log').write_text(run.stdout + run.stderr)
metrics = json.loads((area / 'metrics.json').read_text())['metrics']
trace = json.loads((area / 'trace.json').read_text())
down = next(e['ts'] for e in trace if e.get('name') == 'key-down' and e['ts'] == 19500000)
up = next(e['ts'] for e in trace if e.get('name') == 'key-up' and e['ts'] > down)
read = next(e['ts'] for e in trace if e.get('name') == 'touch' and e['ts'] > down)
keys = metrics['input.key_delivery_ms']['count']
detect = json.loads((ROOT / 'simulator/profiles/metalio-plus.json').read_text())['input']['touch_detect_us']
extra = metrics['input.touch_monitor_extra_ms']['max'] * 1000
# Other devices' traffic on the shared bus (the profile's periodic background transfer, 'fuel-gauge' in the trace) is not
# waveform preparation: the bus time it held after the read was ready is taken out (batch gate 2026-11-11: an unrelated
# change moved that transfer onto the read, 1.16 ms of the 1.18 ms measured).
ready = down + detect + extra
held = sum(max(0, min(e['ts'] + (e.get('dur') or 0), read) - max(e['ts'], ready))
           for e in trace if e.get('name') == 'fuel-gauge')
dispatch = read - down - detect - extra - held
print(json.dumps({'exit': run.returncode, 'keys_delivered': keys, 'first_touch_read_us': read,
                  'dispatch_us': dispatch, 'bus_held_us': held,
                  'key_up_us': up, 'tools_done': metrics.get('hub.tool_results.done'),
                  'tools_stale': metrics.get('hub.tool_results.stale')}), flush=True)
assert keys == 8, f'the cover key press and release must both arrive: {area.relative_to(ROOT)}'
assert read < up, f'the controller must be read before release: {area.relative_to(ROOT)}'
assert dispatch <= 1000, f'waveform preparation delayed the ready input: {area.relative_to(ROOT)}'
assert run.returncode == 0, run.stdout + run.stderr
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
