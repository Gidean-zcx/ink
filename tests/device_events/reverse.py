"""Compile narrow reversions in an ignored directory; require the relevant regression to fail."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', required=True)
p.add_argument('cases', nargs='*')
a = p.parse_args()
build = Path(a.build).resolve()
work = build / 'device-events-reverse'
work.mkdir(exist_ok=True)
mutations = {
    'event': ('cap', [('\\"seq\\":%u,\\"boot\\":%u,', ''), ('id, (long long)at, seq, CB.boot_tag, whose);', 'id, (long long)at, whose);')], 'event carries boot, sequence and emitting member'),
    'member': ('cap', [('if (!list && (ink_json_str(j,', 'if (false && !list && (ink_json_str(j,')], 'missing member metadata rejected'),
    'accept': ('cap', [('t->epoch = (uint32_t)epoch;', 't->epoch = ink_hub_member_epoch();')], 'validation-to-allocation handoff cannot rebind'),
    'epoch': ('cap', [('if (!ink_hub_member_matches(t->member, t->epoch, t->connection)) {', 'if (false && !ink_hub_member_matches(t->member, t->epoch, t->connection)) {'),
                      ('bool delivery = !p->o.hub_epoch || ink_hub_member_matches(p->member, p->o.hub_epoch, p->o.hub_connection);', 'bool delivery = true;')], 'queued old incarnation cannot execute'),
    'directory': ('cap', [('if (next >= 0) {\n        char cursor[64];', 'if (false && next >= 0) {\n        char cursor[64];')], 'partial directory does not replace old view'),
    'replay': ('cap', [('ev_sent = ev_acked;', '(void)ev_acked;')], 'unacknowledged event replayed after reconnect'),
    'budget': ('hub', [('bool ai = !strcmp(p->method, "ai.listen") || !strcmp(p->method, "ai.ask");', 'bool ai = false;')], 'wire budget is remaining time'),
    'scope': ('hub', [('r->member_epoch = member ? epoch : ink_hub_member_epoch();', 'r->member_epoch = ink_hub_member_epoch();'),
                      ('member ? member : ink_member_current()->id);', 'ink_member_current()->id);')], 'private request cannot rebind'),
    'undo': ('cap', [('if (undo >= 0 || (t->asked && (!strcmp(t->name, \"message.send\") || !strcmp(t->name, \"call.dial\"))))', 'if (false && (undo >= 0 || (t->asked && (!strcmp(t->name, \"message.send\") || !strcmp(t->name, \"call.dial\")))))')], 'unsupported automatic window never sends'),
    'deadline': ('hub', [('p->deadline = r->deadline;', 'p->deadline = now + 20000000;')], 'queue delay cannot restart an expired request deadline'),
    'capture': ('ai', [('if (AI.capturing && osal_now_us() - AI.t_press >= CAPTURE_LIMIT_US)', 'if (false && AI.capturing && osal_now_us() - AI.t_press >= CAPTURE_LIMIT_US)')], 'capture stops at 30 seconds'),
    'downlink': ('ai', [('if (AI.tts_stream > 0 && ink_audio_stream_push(AI.tts_stream, opus, len) != 0)', 'if (false && AI.tts_stream > 0 && ink_audio_stream_push(AI.tts_stream, opus, len) != 0)')], 'downlink refusal aborts'),
}
sources = {'cap': 'system/policy/cap.c', 'hub': 'system/services/hub/hub_link.c', 'ai': 'system/services/ai/ai_service.c'}
flags = {}
for line in (build / 'CMakeFiles/sim-device-events.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
base_link = shlex.split((build / 'CMakeFiles/sim-device-events.dir/link.txt').read_text())
for name in a.cases or mutations:
    source, replacements, expected = mutations[name]
    folder = work / name
    folder.mkdir(exist_ok=True)
    text = (ROOT / sources[source]).read_text()
    for before, after in replacements:
        assert before in text, (name, before)
        text = text.replace(before, after)
    changed = folder / Path(sources[source]).name
    changed.write_text(text)
    link = list(base_link)
    for part in ('protocol', 'cap_boundary', 'hub_boundary', 'ai_boundary'):
        obj = folder / (part + '.o')
        defines = [] if part == 'protocol' or not part.startswith(source) else ['-DINK_TEST_' + source.upper() + '_SOURCE=' + json.dumps(str(changed))]
        cmd = [link[0], *flags['C_DEFINES'], *defines, *flags['C_INCLUDES'], *flags['C_FLAGS'], '-c',
               str(ROOT / 'tests/device_events' / (part + '.c')), '-o', str(obj)]
        subprocess.run(cmd, cwd=build, check=True, capture_output=True, text=True)
        index = next(i for i, item in enumerate(link) if item.endswith('/' + part + '.c.o'))
        link[index] = str(obj)
    binary = folder / 'sim-device-events'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
    output = folder / 'out'
    run = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'), '--assets', str(build / 'assets'),
                          '--content', str(build / 'test-content'), '--out', str(output), '--end', '120s', '--quiet', '--no-random-outages'],
                         cwd=ROOT, capture_output=True, text=True, timeout=60)
    (folder / 'run.log').write_text(run.stdout + run.stderr)
    metrics = json.loads((output / 'metrics.json').read_text())
    failures = [w for w in metrics.get('warnings', []) if expected in w]
    assert failures and metrics['metrics'].get('test.fail', 0) > 0, (name, run.returncode, metrics.get('warnings'))
    print(f'{name}: compiler/link exit 0; simulator exit {run.returncode}; expected failure: {failures[0]}')
