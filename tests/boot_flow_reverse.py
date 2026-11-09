"""Focused negative controls using replacement objects; product sources and binaries stay intact."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = 'fa7c18803f8f0eefc44db39acc2da053c7aa9386'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--asan-build', type=Path, required=True)
a = p.parse_args()
build, asan_build = a.build.resolve(), a.asan_build.resolve()
out = build / 'boot-reverse'
out.mkdir(parents=True, exist_ok=True)
paths = ['system/ui/textfield.c', 'system/services/hub/pairing.c', 'system/ui/shell/boot_flow.c']
before = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in paths}
commands, results = [], []


def command(args, cwd=ROOT, source=None, env=None):
    commands.append([str(x) for x in args])
    return subprocess.run(args, cwd=cwd, input=source, env=env, capture_output=True, text=True, timeout=90)


def run(binary, label, mode, folder, expected=None, sanitizer=False):
    if mode == 'reboot':
        result = command([str(Path(os.sys.executable)), '-B', str(ROOT / 'tests/boot_flow_reboot.py'),
            '--build', str(folder), '--binary', str(binary)], env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        (out / (label + '-run.log')).write_text(result.stdout + result.stderr)
        assert result.returncode != 0 and expected in result.stdout + result.stderr, (label, result.stdout, result.stderr)
        results.append({'case': label, 'exit': result.returncode, 'expected': expected})
        print(json.dumps(results[-1]), flush=True)
        return
    result = command([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--script', str(ROOT / 'tests/boot_flow.script'), '--assets', str(folder / 'assets'),
        '--content', str(folder / 'test-content'), '--out', str(out / (label + '-out')),
        '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--no-random-outages', '--quiet'],
        env={**os.environ, 'INK_BOOT_MODE': mode, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (out / (label + '-run.log')).write_text(result.stdout + result.stderr)
    if sanitizer:
        assert result.returncode != 0 and 'AddressSanitizer: stack-use-after-scope' in result.stderr, label
        row = {'case': label, 'exit': result.returncode, 'diagnostic': 'ASan stack-use-after-scope'}
    else:
        data = json.loads((out / (label + '-out') / 'metrics.json').read_text())
        metrics, warnings = data['metrics'], data.get('warnings', [])
        assert metrics.get('test.finished') == 1, (label, 'driver incomplete', warnings)
        if expected:
            assert result.returncode != 0 and metrics.get('test.fail', 0) > 0 and any(expected in w for w in warnings), (label, warnings)
        else:
            assert result.returncode == 0 and not metrics.get('test.fail', 0) and not warnings, (label, warnings)
        row = {'case': label, 'exit': result.returncode, 'pass': metrics.get('test.pass'),
               'fail': metrics.get('test.fail', 0), 'finished': 1}
    results.append(row)
    print(json.dumps(row), flush=True)


def variant(label, source, mode, expected=None, sanitizer=False):
    folder = asan_build if sanitizer else build
    flags = (folder / 'CMakeFiles/ink_system.dir/flags.make').read_text()
    options = []
    for key in ('C_DEFINES', 'C_INCLUDES', 'C_FLAGS'):
        options += shlex.split(re.search(r'^' + key + r' = (.*)$', flags, re.M)[1])
    link = shlex.split((folder / 'CMakeFiles/sim-boot-flow.dir/link.txt').read_text())
    obj = out / (label + '.o')
    compiled = command([link[0], *options, '-x', 'c', '-', '-c', '-o', str(obj)], folder, source)
    (out / (label + '-compile.log')).write_text(compiled.stdout + compiled.stderr)
    assert compiled.returncode == 0, (label, compiled.stderr)
    link[link.index('-o') + 1] = str(out / label)
    link.insert(next(i for i, part in enumerate(link) if part.endswith('libink_system.a')), str(obj))
    linked = command(link, folder)
    (out / (label + '-link.log')).write_text(linked.stdout + linked.stderr)
    assert linked.returncode == 0, (label, linked.stderr)
    run(out / label, label, mode, folder, expected, sanitizer)


run(build / 'sim-boot-flow', 'positive-password', 'probe-password', build)
run(build / 'sim-boot-flow', 'positive-offer', 'probe-offer', build)
pair, boot = (ROOT / paths[1]).read_text(), (ROOT / paths[2]).read_text()
needle = '    snprintf(B.message, sizeof B.message, "正在连接 %s", B.picked);\n'
assert boot.count(needle) == 1
variant('joining-message', boot.replace(needle, ''), 'probe-offer', 'retry replaces the old password error')
old_field = subprocess.check_output(['git', 'show', BASE + ':' + paths[0]], cwd=ROOT, text=True)
variant('textfield', old_field, 'probe-password', 'password uses the focused F2 short input host')
needle = ' || c->src == INK_SRC_SYSTEM) ink_cap_result(c, result);'
assert pair.count(needle) == 1
variant('hotspot', pair.replace(needle, ') ink_cap_result(c, result);'), 'probe-password', 'FirstBoot renders the real setup QR')
state_only = boot.replace('static void changed(void *ctx, const char *id, const ink_args_t *args) {',
    'static void changed(void *ctx, int h, const char *id) {').replace(
    '(void)ctx; (void)id; (void)args;\n    if (current()) request("pairing.status"',
    '(void)ctx; (void)h; (void)id;\n    if (current()) request("pairing.status"').replace(
    'ink_cap_subscribe("pairing.changed", changed, NULL, INK_EXEC_UI)',
    'ink_state_watch("pairing.", changed, NULL, INK_EXEC_UI)')
assert state_only != boot
variant('pair-events', state_only, 'probe-offer', 'arrive at BootConfirm')
needle = 'B.actions[i] = (ink_ui_action_spec_t){B.ids[i], label, cap, B.args[i], {NULL, 0}, true};'
assert boot.count(needle) == 1
borrowed = boot.replace(needle, 'B.actions[i] = (ink_ui_action_spec_t){!strcmp(cap, "shell.launch") ? value : cap, label, cap, B.args[i], {NULL, 0}, true};')
variant('borrowed-id', borrowed, 'probe-password', sanitizer=True)
needle = '    int page = B.resume;'
assert boot.count(needle) == 1
variant('auto-entry', boot.replace(needle, '    int page = -1;'), 'reboot', 'blank device starts setup without a network settings action')
needle = '    remember(B.page);\n'
assert boot.count(needle) == 1
variant('save-progress', boot.replace(needle, ''), 'reboot', 'saved Wi-Fi page rescans after restart')
needle = '                if (already_pairing) publish();'
assert pair.count(needle) == 1
variant('claim-ready-event', pair.replace(needle, '                (void)already_pairing;'), 'reboot', 'claim page survives restart')
needle = 'B.txid[0] && B.claim[0] && ink_pairing_state() == INK_PAIR_PAIRING'
assert boot.count(needle) == 1
variant('confirm-ready', boot.replace(needle, 'B.txid[0] && ink_pairing_state() == INK_PAIR_PAIRING'), 'reboot', 'restored invitation completes through real services')
assert before == {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in paths}
(out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
(out / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
print('All nine negative controls rejected; product sources and normal binaries unchanged.')
