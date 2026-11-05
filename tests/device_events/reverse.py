"""Compile narrow reversions in an ignored directory; require the relevant regression to fail."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', required=True)
p.add_argument('--baseline', help='Compile the selected service from this commit without mutations')
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
    'downlink': ('ai', [('else if (ink_audio_stream_push(AI.tts_stream, opus, len) != 0)', 'else if (false && ink_audio_stream_push(AI.tts_stream, opus, len) != 0)')], 'downlink refusal aborts'),
    'hello': ('hub', [('if (!sync_member()) return;', '/* Mutant: do not synchronize after hello. */')], 'handshake handoff reaches the current member and epoch'),
    'send-boundary': ('hub', [('    if (p->member[0] && !ink_hub_member_matches(p->member, p->member_epoch, H.ws_gen)) {\n        osal_free(buf);\n        finish(p, INK_HUB_ERR_LOST, NULL, -1);\n        return -1;\n    }',
                              '    /* Mutant: omit the final identity check. */')], 'handoff during wire allocation cannot send'),
    'speech-id': ('ai', [('? p->req_id == AI.req_id : p->req_id == AI.seq;', '? p->req_id == AI.req_id : p->kind == 4 ? p->req_id == AI.stream : p->req_id == AI.seq;')], 'real speech failure remains visible'),
    'speech-notify': ('ai', [('ink_probe_count("ai.downlink_overflow");\n            post(AI.session_seq, 4, 0, NULL);',
                            'ink_probe_count("ai.downlink_overflow"); /* Mutant: omit only the failure notification. */')], 'real speech failure remains visible'),
    'speaker-id': ('ai', [('int turn = AI.session_seq;\n    osal_mutex_unlock(AI.lock);\n    if (ours)',
                         'int turn = AI.stream;\n    osal_mutex_unlock(AI.lock);\n    if (ours)')], 'real speech failure remains visible'),
    'speech-open': ('ai', [('if (AI.tts_stream <= 0) {\n            AI.tts_stream = 0;\n            AI.tts_done_for = st;\n            ink_probe_count("ai.downlink_unavailable");\n            post(AI.session_seq, 4, 0, NULL);\n        } else if (ink_audio_stream_push(AI.tts_stream, opus, len) != 0)',
                          'if (AI.tts_stream > 0 && ink_audio_stream_push(AI.tts_stream, opus, len) != 0)')], 'real speech failure remains visible'),
    'speech-delta': ('ai', [('set_status(AI.speech_failed ? "声音没能播放，文字回答在上面" : "回答中");', 'set_status("回答中");')], 'speech failure remains visible when the first text arrives'),
}
review_cases = {'hello', 'send-boundary', 'speech-id', 'speech-notify', 'speaker-id', 'speech-open', 'speech-delta'}
sources = {'cap': 'system/policy/cap.c', 'hub': 'system/services/hub/hub_link.c', 'ai': 'system/services/ai/ai_service.c'}
flags = {}
for line in (build / 'CMakeFiles/sim-device-events.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
for name in a.cases or mutations:
    source, replacements, expected = mutations[name]
    folder = work / (a.baseline[:8] + '-' + name if a.baseline else name)
    folder.mkdir(exist_ok=True)
    text = (subprocess.check_output(['git', 'show', a.baseline + ':' + sources[source]], cwd=ROOT).decode()
            if a.baseline else (ROOT / sources[source]).read_text())
    for before, after in (() if a.baseline else replacements):
        assert before in text, (name, before)
        text = text.replace(before, after)
    changed = folder / Path(sources[source]).name
    changed.write_text(text)
    target = 'sim-device-events-review' if name in review_cases else 'sim-device-events'
    link = shlex.split((build / 'CMakeFiles' / (target + '.dir') / 'link.txt').read_text())
    parts = ('review', 'hub_observer', 'hub_boundary', 'ai_boundary') if name in review_cases else ('protocol', 'cap_boundary', 'hub_boundary', 'ai_boundary')
    for part in parts:
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
                          '--content', str(build / 'test-content'), '--out', str(output), '--end', '240s' if name in review_cases else '120s', '--quiet', '--no-random-outages'],
                         cwd=ROOT, capture_output=True, text=True, timeout=60)
    (folder / 'run.log').write_text(run.stdout + run.stderr)
    metrics = json.loads((output / 'metrics.json').read_text())
    failures = [w for w in metrics.get('warnings', []) if expected in w]
    assert run.returncode == 0 and failures and metrics['metrics'].get('test.fail', 0) > 0, (name, run.returncode, metrics.get('warnings'))
    print(f'{name}: compiler/link exit 0; simulator exit {run.returncode}; expected failure: {failures[0]}')
