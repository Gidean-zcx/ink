"""Manual phone-2 negative control; restore all sources even when a check fails."""
import json
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
os.chdir(root)
(root / 'work').mkdir(exist_ok=True)
model = root / 'ports/sim/hal/hal_net_sim.c'
audio = root / 'system/services/audio/audio_service.c'
phone = root / 'system/apps/phone/phone.c'
original = {p: p.read_text() for p in (model, audio, phone)}
out = root / 'build/test-out/phone-negative'
out.mkdir(parents=True, exist_ok=True)

def build(stage):
    with (root / f'work/build-{stage}.log').open('w') as log:
        subprocess.run(['cmake', '--build', 'build', '--target', 'sim-phone-calls', '-j2'], stdout=log, stderr=subprocess.STDOUT, check=True)

def run(stage):
    dest = out / stage
    command = ['build/sim-phone-calls', '--profile', 'simulator/profiles/metalio-plus.json', '--script', 'tests/phone_calls/calls.script',
               '--budget', 'tests/ui_scene.budget.json', '--assets', 'build/assets', '--content', 'build/test-content',
               '--flash-image', str(out / f'{stage}.img'), '--sd-dir', str(out / f'{stage}-sd'), '--out', str(dest), '--quiet', '--no-random-outages']
    result = subprocess.run(command, env={**os.environ, 'INK_PHONE_PHASE': 'controls'}, capture_output=True, text=True)
    (root / f'work/{stage}.log').write_text(result.stdout + result.stderr)
    data = json.loads((dest / 'metrics.json').read_text())
    print(stage, 'exit=', result.returncode, 'pass=', data['metrics'].get('test.pass', 0), 'fail=', data['metrics'].get('test.fail', 0), flush=True)
    return result, data

try:
    source = original[model]
    start = source.index('    case K_CMUT_Q:')
    end = source.index('    case K_CLCC:', start)
    model.write_text(source[:start] + '    case K_CMUT_Q: case K_CMUT: case K_VTS: r = HAL_AT_ERROR; break;\n' + source[end:])
    audio.write_text(original[audio].replace('    bool go = want != AU.route;\n', '    bool go = want != AU.route;\n    for (int i = 0; i < MAX_FOCUS && go; i++)\n        if (AU.focus[i].token && AU.focus[i].prio == INK_AUDIO_PRIO_CALL) go = false;\n'))
    phone.write_text(original[phone].replace('    ink_cap_call(a->capability, NULL, &o);', '    o.done = NULL;\n    ink_cap_call(a->capability, NULL, &o);'))
    build('negative')
    result, data = run('negative')
    failures = '\n'.join(data.get('warnings', []))
    assert result.returncode != 0
    for message in ['mute button waits', 'sequence accepted by AT+VTS', 'active call really switches', 'record button explains']:
        assert message in failures, message
    print('negative: all four regression categories rejected', flush=True)
finally:
    for p, source in original.items():
        p.write_text(source)
    build('restored')

result, data = run('restored')
assert result.returncode == 0 and data['metrics'].get('test.fail', 0) == 0
print('restored: controls pass', flush=True)
