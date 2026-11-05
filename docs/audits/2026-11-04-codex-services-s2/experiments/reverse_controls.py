#!/usr/bin/env python3
"""Remove one protection in an isolated copy; link the existing, unchanged test objects.

All compiler inputs, executables and logs go under build-audit/reverse-controls.
No source-tree files are changed. A nonzero test exit is the expected outcome.
"""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[4]
BUILD = ROOT / 'build-audit'
DEST = BUILD / 'reverse-controls'
PY = os.environ.get('INK_AUDIT_PY', sys.executable)


def replace_once(text, old, new):
    assert text.count(old) == 1, (old, text.count(old))
    return text.replace(old, new)


def compile_case(name, target, source, transform, replace_object=None):
    out = DEST / name
    out.mkdir(parents=True, exist_ok=True)
    path = ROOT / source
    content = transform(path.read_text())
    mutant = out / path.name
    mutant.write_text(content)
    flags = {}
    for line in (BUILD / 'CMakeFiles' / (target + '.dir') / 'flags.make').read_text().splitlines():
        if ' = ' in line:
            k, v = line.split(' = ', 1)
            flags[k] = shlex.split(v)
    link = shlex.split((BUILD / 'CMakeFiles' / (target + '.dir') / 'link.txt').read_text())
    obj = out / 'mutant.o'
    compile_cmd = [link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                   '-I', str(path.parent), '-c', str(mutant), '-o', str(obj)]
    log = out / 'build.log'
    with log.open('w') as f:
        built = subprocess.run(compile_cmd, cwd=BUILD, stdout=f, stderr=f)
        assert built.returncode == 0, log
        if replace_object:
            matched = [i for i, s in enumerate(link) if s.endswith(replace_object)]
            assert len(matched) == 1, matched
            link[matched[0]] = str(obj)
        else:
            pos = next(i for i, s in enumerate(link) if s.endswith('.a'))
            link.insert(pos, str(obj))
        link[link.index('-o') + 1] = str(out / 'sim')
        linked = subprocess.run(link, cwd=BUILD, stdout=f, stderr=f)
        assert linked.returncode == 0, log
    return out, compile_cmd, link


def run_case(name, target, source, transform, test, env=None, profile='metalio-plus', replace_object=None):
    out, compile_cmd, link = compile_case(name, target, source, transform, replace_object)
    if test == 'update':
        cmd = ['cmake', '-DSIM=' + str(out / 'sim'), '-DPROFILE=' + str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '-DASSETS=' + str(BUILD / 'assets'), '-DCONTENT=' + str(BUILD / 'test-content'), '-DOUT=' + str(out / 'out'),
               '-DSRC=' + str(ROOT / 'tests/integration'), '-DPY=' + PY,
               '-DTOOL=' + str(ROOT / 'tools/inkfw/make_inkfw.py'), '-P', str(ROOT / 'tests/integration/update.cmake')]
    else:
        budget = 'textinput' if test.startswith('textinput_') else test
        cmd = [str(out / 'sim'), '--profile', str(ROOT / ('simulator/profiles/' + profile + '.json')),
               '--script', str(ROOT / ('tests/integration/' + test + '.script')),
               '--budget', str(ROOT / ('tests/integration/' + budget + '.budget.json')),
               '--assets', str(BUILD / 'assets'), '--content', str(BUILD / 'test-content'),
               '--out', str(out / 'out'), '--quiet', '--no-random-outages']
    with (out / 'run.log').open('w') as log:
        ran = subprocess.run(cmd, cwd=ROOT, env={**os.environ, **(env or {})}, stdout=log, stderr=log, timeout=150)
    metrics = []
    for path in sorted((out / 'out').rglob('metrics.json')):
        m = json.loads(path.read_text())
        metrics.append({'path': str(path.relative_to(BUILD)), 'finished': m['metrics'].get('test.finished', 0),
                        'fail': m['metrics'].get('test.fail', 0), 'warnings': m.get('warnings', [])})
    result = {'id': name, 'compile_exit': 0, 'link_exit': 0, 'test_exit': ran.returncode,
              'commands': [shlex.join(compile_cmd), shlex.join(link), shlex.join(cmd)], 'environment': env or {}, 'metrics': metrics}
    (out / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    assert ran.returncode != 0 and any(m['fail'] > 0 for m in metrics), result
    print(json.dumps({'id': name, 'test_exit': ran.returncode, 'metrics': metrics}, ensure_ascii=False), flush=True)
    return result


def schema(text):
    start = text.index('    {.id = "data.get",')
    a = text.index('     .input = ', start)
    b = text.index('\n', a)
    return text[:a] + text[b:]


def old_interrupt(text):
    old = subprocess.check_output(['git', 'show', 'a81c1b8:system/services/notify/interrupt.c'], cwd=ROOT, text=True)
    old = old.replace('#include "notify_internal.h"', '#include "' + str(ROOT / 'system/services/notify/notify_internal.h') + '"')
    old_path = DEST / 'interrupt-apply' / 'interrupt-old.c'
    old_path.write_text(old)
    return replace_once(text, '#include "../../system/services/notify/interrupt.c"', '#include "' + str(old_path) + '"')


cases = [
    ('missing-input-schema', 'sim-ai-surface', 'system/services/data/data_caps.c', schema, 'ai_surface', {}),
    ('interrupt-apply', 'sim-races', 'tests/integration/races_interrupt.c', old_interrupt, 'races', {'replace_object': '/tests/integration/races_interrupt.c.o'}),
    ('confirmed-member', 'sim-races', 'system/policy/confirm.c', lambda t: replace_once(t, '.member = h->member, ', ''), 'races', {}),
    ('clipboard-sensitive', 'sim-textinput', 'system/services/text/clipboard.c', lambda t: replace_once(t, 'if (allowed && !(remote && s))', 'if (allowed && !(remote && s && false))'), 'textinput_main', {'env': {'INK_TEXT_PHASE': 'main'}}),
    ('pin-wait', 'sim-people', 'system/policy/members.c', lambda t: replace_once(t, 'if (*wait_s) {', 'if (false) {'), 'people', {}),
    ('wake-gate', 'sim-listening', 'system/services/ai/ai_service.c', lambda t: replace_once(t, '(ink_ui_overlay_up() || !ink_member_space_open())', '(false)'), 'listening', {'profile': 'metalio-plus-handsfree'}),
    ('upgrade-signature', 'sim-update', 'system/services/update/update_service.c', lambda t: replace_once(t, 'e = hal_crypto_verify_fw(digest, sig, sizeof sig, p->key_id)', 'e = (true || hal_crypto_verify_fw(digest, sig, sizeof sig, p->key_id))'), 'update', {}),
    ('media-resume-back', 'sim-media', 'system/services/audio/media_service.c', lambda t: replace_once(t, '#define RESUME_BACK_MS 3000', '#define RESUME_BACK_MS 0'), 'media', {}),
]

if __name__ == '__main__':
    results = []
    for name, target, source, transform, test, kwargs in cases:
        if len(sys.argv) > 1 and name not in sys.argv[1:]:
            continue
        results.append(run_case(name, target, source, transform, test, **kwargs))
        (DEST / 'results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')
