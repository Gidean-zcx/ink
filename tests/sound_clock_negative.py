"""Compile isolated reversed fixes, without touching the checkout or weakening tests."""
import argparse
import json
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline-clock', action='store_true')
a = p.parse_args()
build = a.build.resolve()
flags = {}
for line in (build / 'CMakeFiles/ink_system.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
if a.baseline_clock:
    area = Path(tempfile.mkdtemp(prefix='sound-clock-baseline-', dir=build / 'test-out'))
    file = 'system/apps/clock/clock.c'
    source = subprocess.check_output(['git', 'show', f'62b0e83da1d34f50629b5e576807db1381b26078:{file}'], cwd=ROOT, text=True)
    link = shlex.split((build / 'CMakeFiles/sim-configured.dir/link.txt').read_text())
    obj, binary = area / 'clock.o', area / 'sim-configured'
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
        '-I', str((ROOT / file).parent), '-x', 'c', '-c', '-', '-o', str(obj)], input=source, text=True, cwd=build, check=True)
    link.insert(next(i for i, value in enumerate(link) if value.endswith('.a')), str(obj))
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    r = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/scenarios/ai_page_version.script'), '--budget', str(ROOT / 'tests/scenarios/ai_page_version.budget.json'),
        '--hub', str(ROOT / 'tests/scenarios/ai_page_version.hub.json'), '--assets', str(build / 'assets'),
        '--content', str(build / 'test-content'), '--out', str(area / 'out'), '--quiet', '--no-random-outages'], capture_output=True, text=True)
    (area / 'run.log').write_text(r.stdout + r.stderr)
    m = json.loads((area / 'out/metrics.json').read_text())['metrics']
    print(json.dumps({'baseline_clock_exit': r.returncode, 'keys_delivered': m.get('input.key_delivery_ms', {}).get('count'),
        'tools_done': m.get('hub.tool_results.done'), 'tools_stale': m.get('hub.tool_results.stale')}))
    raise SystemExit(r.returncode)
variants = {
    'key-switch': ('system/services/audio/audio_service.c', 'if (ink_setting_get("audio.key_tone"))', 'if (true)', 'sound'),
    'vibration-switch': ('system/services/audio/audio_service.c', 'if (ink_setting_get("audio.vibration"))', 'if (true)', 'sound'),
    'ring-choice': ('system/services/audio/audio_service.c', 'int tone = (int)ink_setting_get(AU.ring_call ? "audio.call_ring" : "audio.alarm_ring");', 'int tone = 0;', 'sound'),
    'persistence': ('system/policy/settings.c', 'if (valid(&defs[i], SE.saved[k].v)) v = SE.saved[k].v;', 'if (valid(&defs[i], SE.saved[k].v)) v = defs[i].def;', None),
    'pause': ('system/apps/clock/clock.c', 't->left = !strcmp(c->cap->id, "clock.timer_pause") ? left : 300000000;', 't->left = !strcmp(c->cap->id, "clock.timer_pause") ? left - left : 300000000;', 'timer'),
    'expiry': ('system/apps/clock/clock.c', 'if (ink_member_exists(t->owner)) t->ringing = ink_interrupt_raise(&it);', '(void)it;', 'timer'),
    'member': ('system/apps/clock/clock.c', 'static countdown_t *my_timer(void) { /* UI task */', 'static countdown_t *my_timer(void) { /* UI task */ return &timers[0];', 'timer'),
}
area = Path(tempfile.mkdtemp(prefix='sound-clock-negative-', dir=build / 'test-out'))
results = []
for name, (file, old, new, phase) in variants.items():
    source_path = ROOT / file
    source = source_path.read_text()
    assert source.count(old) == 1, name
    work = area / name
    work.mkdir()
    obj, binary = work / 'variant.o', work / 'sim-sound-clock'
    link = shlex.split((build / 'CMakeFiles/sim-sound-clock.dir/link.txt').read_text())
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
        '-I', str(source_path.parent), '-x', 'c', '-c', '-', '-o', str(obj)],
        input=source.replace(old, new), text=True, cwd=build, check=True)
    link.insert(next(i for i, value in enumerate(link) if value.endswith('.a')), str(obj))
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    cmd = [str(Path(os.sys.executable)), str(ROOT / 'tests/sound_clock.py'), '--build', str(build), '--binary', str(binary)]
    if phase:
        cmd += ['--phase', phase]
    r = subprocess.run(cmd, capture_output=True, text=True, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, timeout=180)
    (work / 'run.log').write_text(r.stdout + r.stderr)
    failed = r.returncode != 0 and re.search(r'fail=[1-9]\d*, finished=1', r.stdout) is not None
    results.append({'variant': name, 'exit': r.returncode, 'detected': failed})
    print(json.dumps(results[-1]), flush=True)
    if not failed:
        raise SystemExit(f'reversed fix was not detected: {name}\n{r.stdout}\n{r.stderr}')
(area / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
print(f'evidence: {area.relative_to(ROOT)}')
