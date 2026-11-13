"""tap-loss-1 / D-127: the status bar's listening mark comes in the AI panel's frame when the AI key is pressed, not in
a refresh of its own. Runs the ai_page_version scenario with a trace and counts updates that cover the status bar alone
while the AI key is held (three turns of 1.2 s): none. Before the change each turn's mark took one (18.42 s: id 20).
The mark goes when the audio task reports the capture stopped: that refresh stays, after the key is let go."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
(build / 'test-out').mkdir(exist_ok=True)
area = Path(tempfile.mkdtemp(prefix='listen-mark-', dir=build / 'test-out'))
run = subprocess.run([
    str(build / 'sim-configured'),
    '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--script', str(ROOT / 'tests/scenarios/ai_page_version.script'),
    '--hub', str(ROOT / 'tests/scenarios/ai_page_version.hub.json'),
    '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
    '--out', str(area), '--quiet', '--trace', '--no-random-outages',
], capture_output=True, text=True, timeout=120)
(area / 'run.log').write_text(run.stdout + run.stderr)
trace = [e for e in json.loads((area / 'trace.json').read_text()) if 'ts' in e]
downs = sorted(e['ts'] for e in trace if e.get('name') == 'key-down')
ups = sorted(e['ts'] for e in trace if e.get('name') == 'key-up')
presses = [(d, min(u for u in ups if u > d)) for d in downs]
ai = [(d, u) for d, u in presses if u - d >= 1000000]  # the scenario holds the AI key 1.2 s, the page key 80 ms
updates = [e for e in trace if isinstance(e.get('args'), dict) and 'id' in e['args'] and 'w' in e['args']]
bar_only = [e for e in updates if e['args'].get('x') == 0 and e['args'].get('w', 9999) <= 120]
during = [(round(e['ts'] / 1e6, 3), e['args']['id']) for e in bar_only if any(d <= e['ts'] <= u for d, u in ai)]
after = [any(u < e['ts'] <= u + 1500000 for e in bar_only) for d, u in ai]
print(json.dumps({'exit': run.returncode, 'ai_presses': len(ai), 'updates': len(updates), 'bar_only_while_held': during,
                  'bar_only_total': len(bar_only), 'mark_cleared_after_release': after}))
assert len(ai) == 3, f'three AI turns in the scenario: {area.relative_to(ROOT)}'
assert not during, f'the listening mark took a refresh of its own while the key was held: {during}'
assert all(after), f'the mark is cleared after each release, once the capture stopped: {after}'
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
