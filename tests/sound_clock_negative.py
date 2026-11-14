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
p.add_argument('--review-only', action='store_true')
p.add_argument('--second-review-only', action='store_true')
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
    'pause': ('system/apps/clock/clock_infinitime.cpp', '    u.screen.ToggleRunning(); /* copied */\n    u.state = u.timer.IsRunning()',
              '    u.screen.ToggleRunning(); /* copied */\n    if (was) { u.screen.hourCounter.SetValue(0); u.screen.minuteCounter.SetValue(0); u.screen.secondCounter.SetValue(0); }\n    u.state = u.timer.IsRunning()', 'timer'),
    'expiry': ('system/apps/clock/clock_timer.c', 'return ink_member_exists(ck_member_id(t / CK_PER_MEMBER)) ? ink_interrupt_raise(&it) : -1;', 'return (void)it, -1;', 'timer'),
    'member': ('system/apps/clock/clock.c', 'const char *id = ink_member_current()->id; int free = -1;', 'const char *id = members[0].id[0] ? members[0].id : ink_member_current()->id; int free = -1;', 'timer'),
    'touch-feedback-order': ('system/ui/ui_task.c', 'UI.click_tone = true;', 'ink_audio_key_tone();', 'sound'),
    'preserve-preview': ('system/services/audio/audio_service.c', ' || (tone == INK_TONE_KEY && AU.tone_pending)', '', 'sound'),
    'input-priority': ('ports/sim/hal/hal_input_sim.c', '#define INPUT_PRIO OSAL_PRIO_MAX', '#define INPUT_PRIO 12', 'input'),
    'display-priority': ('ports/sim/hal/hal_display_sim.c', '.priority = OSAL_PRIO_MAX - 1, .core = c', '.priority = OSAL_PRIO_MAX, .core = c', 'input'),
    'capture-lease': ('system/services/audio/audio_service.c', 'AU.cap_on = true;\n    hold_writes_locked();', 'AU.cap_on = true;', 'capture'),
    'input-affinity': ('ports/sim/hal/hal_input_sim.c', '.priority = INPUT_PRIO, .core = 0', '.priority = INPUT_PRIO, .core = -1', 'startup'),
}
if a.review_only:
    variants = {name: value for name, value in variants.items() if name in ('touch-feedback-order', 'preserve-preview', 'input-priority', 'display-priority', 'capture-lease')}
if a.second_review_only:
    variants = {name: value for name, value in variants.items() if name == 'input-affinity'}
area = Path(tempfile.mkdtemp(prefix='sound-clock-negative-', dir=build / 'test-out'))
results = []
for name, (file, old, new, phase) in variants.items():
    source_path = ROOT / file
    source = source_path.read_text()
    assert source.count(old) == 1, name
    work = area / name
    work.mkdir()
    target = 'sim-hub-e2e' if phase == 'startup' else 'sim-configured' if phase in ('input', 'capture') else 'sim-sound-clock'
    obj, binary = work / 'variant.o', work / target
    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    source_flags = dict(flags)
    if file.startswith('ports/'):
        for line in (build / 'CMakeFiles/ink_port_sim.dir/flags.make').read_text().splitlines():
            if ' = ' in line:
                key, value = line.split(' = ', 1)
                source_flags[key] = shlex.split(value)
    cxx = file.endswith('.cpp') /* clock-3: the copied InfiniTime unit, with its shims first */
    language = ['-x', 'c++', '-I', str(ROOT / 'system/apps/clock/vendor/infinitime/port'), '-I', str(ROOT / 'system/apps/clock/vendor/infinitime/src'),
                '-fno-exceptions', '-fno-rtti'] if cxx else ['-x', 'c']
    prefix = 'CXX_' if cxx else 'C_'
    subprocess.run([link[0], *language, *source_flags[prefix + 'DEFINES'], *source_flags[prefix + 'INCLUDES'], *source_flags[prefix + 'FLAGS'],
        '-I', str(source_path.parent), '-c', '-', '-o', str(obj)],
        input=source.replace(old, new), text=True, cwd=build, check=True)
    link.insert(next(i for i, value in enumerate(link) if value.endswith('.a')), str(obj))
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True)
    runner = 'sound_clock_input.py' if phase == 'input' else 'sound_clock.py'
    cmd = [str(Path(os.sys.executable)), str(ROOT / 'tests' / runner), '--build', str(build), '--binary', str(binary)]
    if phase and phase not in ('input', 'capture', 'startup'):
        cmd += ['--phase', phase]
    if phase == 'capture':
        cmd = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
            '--script', str(ROOT / 'tests/scenarios/ai_tools.script'), '--hub', str(ROOT / 'tests/scenarios/ai_tools.hub.json'),
            '--budget', str(ROOT / 'tests/scenarios/ai_tools.budget.json'), '--assets', str(build / 'assets'),
            '--content', str(build / 'test-content'), '--out', str(work / 'out'), '--quiet', '--no-random-outages']
    if phase == 'startup':
        cmd = [os.sys.executable, str(ROOT / 'tests/hub/e2e.py'), '--sim', str(binary),
            '--parser', str(build / 'hub-json-check'), '--assets', str(build / 'assets'),
            '--content', str(build / 'test-content'), '--out', str(work / 'out')]
    r = subprocess.run(cmd, capture_output=True, text=True, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'}, timeout=180)
    (work / 'run.log').write_text(r.stdout + r.stderr)
    if phase == 'startup':
        measured = json.loads((work / 'out/sim/metrics.json').read_text())['metrics']
        failed = r.returncode != 0 and measured['test.hub_member_ready_ms']['max'] >= 3000
        print(json.dumps({'member_ready_ms': measured['test.hub_member_ready_ms']['max']}), flush=True)
    elif phase == 'capture':
        measured = json.loads((work / 'out/metrics.json').read_text())['metrics']
        failed = r.returncode != 0 and measured.get('audio.mic.lost_ms', 0) > 0
    elif phase == 'input':
        measured = json.loads(r.stdout.splitlines()[0])
        failed = r.returncode != 0 and (measured['keys_delivered'] == 6 or measured['dispatch_us'] > 1000)
    else:
        failed = r.returncode != 0 and re.search(r'fail=[1-9]\d*, finished=1', r.stdout) is not None
    results.append({'variant': name, 'exit': r.returncode, 'detected': failed})
    print(json.dumps(results[-1]), flush=True)
    if not failed:
        raise SystemExit(f'reversed fix was not detected: {name}\n{r.stdout}\n{r.stderr}')
(area / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
print(f'evidence: {area.relative_to(ROOT)}')
