"""Require unchanged privacy regressions to reject removed ownership checks."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', required=True)
p.add_argument('--baseline')
a = p.parse_args()
build = Path(a.build).resolve()
work = build / 'device-events-private-reverse'
work.mkdir(exist_ok=True)
cases = {
    'page': ('system/ui/ui_task.c',
             'bool current = UI.noverlays ? snapshot_owner_matches(&UI.overlay_owner[UI.noverlays - 1], &identity) :\n'
             '                                 identity.authed && snapshot_owner_matches(&UI.page_owner, &identity);',
             'bool current = true;', 'stale page snapshot is unavailable before publication'),
    'input': ('system/services/text/text_service.c',
              'return !TS.session || (identity.authed && TS.owner.generation == identity.generation &&\n'
              '           !strcmp(TS.owner.member, identity.member));',
              'return true;', 'stale input snapshot is unavailable before publication'),
    'page-action': ('system/policy/cap.c',
                    'ink_ui_page_current() && !strcmp(ink_ui_top_name(), c->activity)',
                    '!strcmp(ink_ui_top_name(), c->activity)', 'new member cannot invoke old page capability'),
    'input-action': ('system/services/text/text_caps.c',
                     'if (!ink_text_session_current()) {\n        fail(c, INK_RC_STALE, "输入已换人，请重新读取");\n        return false;\n    }',
                     '/* Mutant: accept the old target. */', 'new member cannot edit old input session'),
    'speech': ('system/services/audio/audio_service.c',
               'return owner->generation == identity.generation && !strcmp(owner->member, identity.member) &&\n'
               '           (prio == INK_AUDIO_PRIO_STORY || (owner->epoch == identity.epoch &&\n'
               '            (prio != INK_AUDIO_PRIO_VOICE || identity.authed)));',
               '(void)owner; (void)prio; return true;', 'finished adult speech cannot play before member publication'),
}
target = build / 'CMakeFiles/sim-device-events-private.dir'
flags = {}
for line in (target / 'flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
original_link = shlex.split((target / 'link.txt').read_text())
for name, (source, before, after, expected) in cases.items():
    if a.baseline and name == 'input':
        source = 'system/services/text/text_caps.c'
    if a.baseline and name == 'page':
        # The old UI predates the policy's ownership query; its missing guard accepted any visible page.
        compatibility = '\nbool ink_ui_page_current(void) { return true; }\n'
    else:
        compatibility = ''
    folder = work / ((a.baseline[:8] + '-') if a.baseline else '') / name
    folder.mkdir(parents=True, exist_ok=True)
    text = (subprocess.check_output(['git', 'show', a.baseline + ':' + source], cwd=ROOT).decode()
            if a.baseline else (ROOT / source).read_text())
    if not a.baseline:
        assert text.count(before) == 1, (name, 'ownership check changed')
        text = text.replace(before, after, 1)
    changed = folder / Path(source).name
    changed.write_text(text + compatibility)
    obj = folder / 'service.o'
    link = original_link.copy()
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-I' + str((ROOT / source).parent), '-c', str(changed), '-o', str(obj)],
                   cwd=build, check=True, capture_output=True, text=True)
    binary = folder / 'sim-private'
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, word in enumerate(link) if word.endswith('.a')), str(obj))
    subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
    out = folder / 'out'
    run = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                          '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
                          '--hub', str(build / 'test-out/device-events/private-hub.json'), '--out', str(out),
                          '--end', '90s', '--quiet', '--no-random-outages'],
                         cwd=ROOT, capture_output=True, text=True, timeout=60)
    (folder / 'run.log').write_text(run.stdout + run.stderr)
    report = json.loads((out / 'metrics.json').read_text())
    metrics = report['metrics']
    failures = [w for w in report.get('warnings', []) if expected in w]
    assert run.returncode == 0 and metrics.get('test.finished') == 1 and metrics.get('test.fail', 0) > 0 and failures, (name, run.returncode, report.get('warnings'))
    print(f'{name}: compiler/link/simulator exit 0; test.finished=1; expected failure: {failures[0]}', flush=True)
