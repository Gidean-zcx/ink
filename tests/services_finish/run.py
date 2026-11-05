#!/usr/bin/env python3
"""Run service boundary regressions with the build's own compiler and link flags."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', default='build')
p.add_argument('--baseline', help='Read replacement service sources from this commit')
p.add_argument('suites', nargs='*', default=['files', 'storage'])
a = p.parse_args()
build = (ROOT / a.build).resolve()
work = Path(tempfile.mkdtemp(prefix='services-finish-', dir=build))
suites = {
    'file_events': ('sim-media', ['file_handoff_boundary.c', 'media_record_boundary.c'], '30s'),
    'file_paths': ('sim-media', ['file_handoff_boundary.c'], '30s'),
    'file_reuse': ('sim-media', ['file_handoff_boundary.c'], '30s'),
    'file_late': ('sim-media', ['file_late_boundary.c'], '25s'),
    'member_writes': ('sim-media', ['members_boundary.c'], '40s'),
    'late_writers': ('sim-media', ['late_app_boundary.c', 'late_media_boundary.c', 'late_notify_boundary.c', 'late_activity_boundary.c'], '25s'),
    'reader_late': ('sim-media', ['reader_boundary.c'], '35s'),
    'member_inventory': ('sim-media', [], '30s'),
    'wifi_undo': ('sim-media', ['wifi_boundary.c'], '35s'),
    'undo_schedules': ('sim-media', ['time_boundary.c', 'time_caps_boundary.c'], '40s'),
    'undo_owner': ('sim-media', ['cap_boundary.c'], '25s'),
    'replay': ('sim-media', ['cap_boundary.c'], '30s'),
    'undo_data': ('sim-media', ['data_caps_boundary.c', 'file_caps_boundary.c', 'notify_caps_boundary.c', 'time_caps_boundary.c', 'wifi_boundary.c'], '30s'),
    'removal_alarm': ('sim-media', ['members_boundary.c', 'time_boundary.c', 'task_boundary.c', 'notify_boundary.c', 'activity_boundary.c'], '30s'),
    'empty_removal': ('sim-media', ['members_boundary.c', 'file_clean_boundary.c'], '30s'),
    'update_history': ('sim-media', [], '30s'),
    'text_window': ('sim-media', [], '20s'),
    'keyboard_snapshot': ('sim-media', ['ui_boundary.c'], '20s'),
    'offpanel': ('sim-media', ['off_boundary.c', 'display_boundary.c'], '30s'),
    'files': ('sim-media', ['file_boundary.c'], '75s'),
    'storage': ('sim-media', [], '75s'),
    'update': ('sim-media', [], '80s'),
    'text': ('sim-media', ['text_boundary.c', 'text_caps_boundary.c'], '20s'),
    'bindings': ('sim-media', ['text_boundary.c', 'text_caps_boundary.c'], '25s'),
    'lowbattery': ('sim-media', [], '25s'),
    'audio': ('sim-media', [], '40s'),
    'voice': ('sim-media', ['audio_boundary.c'], '40s'),
    'handoff': ('sim-media', ['hub_boundary.c'], '40s'),
    'isolation': ('sim-media', ['audio_boundary.c'], '60s'),
    'shutdown': ('sim-media', [], '30s'),
    'actions': ('sim-media', [], '15s'),
    'policy': ('sim-media', ['settings_boundary.c', 'wifi_boundary.c'], '40s'),
    'permissions': ('sim-media', ['settings_boundary.c'], '25s'),
    'undo': ('sim-media', ['settings_boundary.c', 'cap_boundary.c'], '30s'),
    'playback': ('sim-media', ['audio_boundary.c'], '35s'),
    'activity': ('sim-media', [], '200s'),
    'upgrade_crash': ('sim-media', [], '20s'),
    'removal': ('sim-media', ['removal_files.c'], '25s'),
}

def compile_suite(name):
    template, extras, end = suites[name]
    if a.baseline and name in ('file_events', 'file_paths', 'file_reuse'):
        extras = [*extras, *[source for source in ('media_record_boundary.c', 'ogg_record_boundary.c') if source not in extras]]
    flags = {}
    for line in (build / 'CMakeFiles' / (template + '.dir') / 'flags.make').read_text().splitlines():
        if ' = ' in line:
            k, v = line.split(' = ', 1)
            flags[k] = shlex.split(v)
    link = shlex.split((build / 'CMakeFiles' / (template + '.dir') / 'link.txt').read_text())
    defines = []
    if a.baseline:
        # Read only: source copies and binaries live in the ignored build directory.
        for rel in ['system/ui/app_registry.c', 'system/services/audio/ogg.c', 'system/services/audio/media_service.c', 'apps/reader/reader.c', 'system/services/file/file_service.c', 'system/services/data/datastore.c', 'system/services/update/update_service.c', 'system/services/text/text_service.c', 'system/services/text/text_caps.c', 'system/services/audio/audio_service.c', 'system/services/power/power_service.c', 'system/services/ai/ai_service.c', 'system/policy/cap.c', 'system/policy/settings.c', 'system/policy/members.c', 'system/services/hub/hub_link.c', 'system/services/net/wifi.c', 'system/services/time/time_service.c', 'system/services/time/time_caps.c', 'system/services/res/task.c', 'system/services/data/data_caps.c', 'system/services/file/file_caps.c', 'system/ui/textfield.c', 'system/ui/ui_task.c', 'system/ui/shell/off_picture.c', 'system/services/notify/notify_service.c', 'system/services/notify/notify_caps.c']:
            target = work / Path(rel).name
            target.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':' + rel], cwd=ROOT))
            macro = 'INK_TEST_' + {'app_registry.c': 'APP', 'ogg.c': 'OGG', 'media_service.c': 'MEDIA', 'reader.c': 'READER', 'file_service.c': 'FILE', 'datastore.c': 'DATA', 'update_service.c': 'UPDATE', 'text_service.c': 'TEXT', 'text_caps.c': 'TEXT_CAPS', 'audio_service.c': 'AUDIO', 'power_service.c': 'POWER', 'ai_service.c': 'AI', 'cap.c': 'CAP', 'settings.c': 'SETTINGS', 'members.c': 'MEMBERS', 'hub_link.c': 'HUB', 'wifi.c': 'WIFI', 'time_service.c': 'TIME', 'time_caps.c': 'TIME_CAPS', 'task.c': 'TASK', 'data_caps.c': 'DATA_CAPS', 'file_caps.c': 'FILE_CAPS', 'textfield.c': 'TEXTFIELD', 'ui_task.c': 'UI', 'off_picture.c': 'OFF', 'notify_service.c': 'NOTIFY', 'notify_caps.c': 'NOTIFY_CAPS'}[Path(rel).name] + '_SOURCE'
            if rel.endswith('members.c') and 'ink_member_io_begin' not in target.read_text():
                defines += ['-DINK_TEST_OLD_MEMBER_IO=1']
            if rel.endswith('text_service.c') and 'ink_text_edit_revision' not in target.read_text():
                defines += ['-DINK_TEST_OLD_TEXT=1']
            if rel.endswith('time_service.c') and 'ink_alarm_add_versioned' not in target.read_text():
                defines += ['-DINK_TEST_OLD_ALARM_REVISION=1']
            for feature, token in [('MEMBERS', 'ink_member_generation'), ('TIME', 'ink_time_forget_member'), ('TASK', 'ink_task_forget_member'), ('FILE', 'ink_fs_file_revision'), ('UPDATE', 'check_rules_for'), ('TEXTFIELD', 'display_map'), ('OFF', 'ink_power_off_picture_result'), ('NOTIFY', 'ink_notify_cancel_if_ver')]:
                if macro == 'INK_TEST_' + feature + '_SOURCE' and token not in target.read_text(): defines += ['-DINK_TEST_OLD_' + feature + '=1']
            defines += ['-D' + macro + '=' + json.dumps(str(target))]
    objects = []
    for i, source in enumerate([name + '.c', *extras]):
        obj = work / (name + '-' + str(i) + '.o')
        cmd = [link[0], *flags['C_DEFINES'], *defines, *flags['C_INCLUDES'],
               '-I' + str(ROOT / 'system/services/file'), '-I' + str(ROOT / 'system/services/data'), '-I' + str(ROOT / 'system/services/text'), '-I' + str(ROOT / 'system/policy'), '-I' + str(ROOT / 'system/ui/shell'), '-I' + str(ROOT / 'system/services/time'), '-I' + str(ROOT / 'system/services/notify'),
               '-I' + str(ROOT / 'system/services/audio'),
               *flags['C_FLAGS'], '-Wno-unused-function', '-c', str(ROOT / 'tests/services_finish' / source), '-o', str(obj)]
        subprocess.run(cmd, cwd=build, check=True)
        objects.append(str(obj))
    for i, word in enumerate(link):
        if '/tests/integration/' in word and word.endswith('.o'):
            link[i] = objects[0]
            break
    binary = work / name
    link[link.index('-o') + 1] = str(binary)
    first_lib = next(i for i, word in enumerate(link) if word.endswith('.a'))
    link[first_lib:first_lib] = objects[1:]
    subprocess.run(link, cwd=build, check=True)
    return binary, end

def review_matrix(name, binary):
    all_ok = True
    def boot(path, label, env, checkpoint=None):
        nonlocal all_ok
        out = path / label
        cmd = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
               '--out', str(out), '--end', '25s', '--quiet', '--no-random-outages',
               '--flash-image', str(path / 'user.img'), '--sd-dir', str(path / 'sd')]
        if name == 'file_events': cmd += ['--script', str(ROOT / 'tests/services_finish/voice.script')]
        run = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=180,
                             env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1', **env})
        (path / (label + '.stdout.log')).write_text(run.stdout)
        (path / (label + '.stderr.log')).write_text(run.stderr)
        metrics = json.loads((out / 'metrics.json').read_text())
        m = metrics['metrics']
        complete = m.get(checkpoint) == 1 and m.get('power.reboot') == 1 if checkpoint else m.get('test.finished') == 1
        ok = run.returncode == 0 and complete and m.get('test.fail', 0) == 0
        print(name + '/' + path.name + '/' + label, 'PASS' if ok else 'FAIL',
              'exit=' + str(run.returncode), 'checks=' + str(m.get('test.pass', 0)), flush=True)
        if not ok:
            print(run.stderr[-2000:])
            for warning in metrics.get('warnings', []): print(warning)
        all_ok &= ok

    if name == 'file_events':
        for operation in ('handle', 'record', 'record_start', 'play_start', 'play_api_start', 'atomic', 'mkdir', 'remove', 'move', 'copy', 'delete'):
            for change in (False, True):
                path = work / (operation + ('-switch' if change else '-control'))
                (path / 'sd').mkdir(parents=True)
                env = {'INK_REVIEW_OPERATION': operation}
                if change: env['INK_REVIEW_CHANGE'] = '1'
                boot(path, 'run', env)
    elif name == 'file_paths':
        for operation in ('file.move', 'file.copy'):
            for root in ('/me', '/app/demo', '/members/owner', '/family'):
                stages = ('source', 'stat', 'setup') if operation == 'file.copy' and root != '/app/demo' else ('source', 'setup') if operation == 'file.copy' else ('source',)
                for stage in stages:
                    for change in (False, True):
                        path = work / (operation + root.replace('/', '-') + '-' + stage + ('-switch' if change else '-control'))
                        (path / 'sd').mkdir(parents=True)
                        env = {'INK_REVIEW_OPERATION': operation, 'INK_REVIEW_ROOT': root, 'INK_REVIEW_STAGE': stage}
                        if change: env['INK_REVIEW_CHANGE'] = '1'
                        boot(path, 'run', env)
            for change in (False, True):
                path = work / (operation + '-child' + ('-switch' if change else '-control'))
                (path / 'sd').mkdir(parents=True)
                env = {'INK_REVIEW_OPERATION': operation, 'INK_REVIEW_ROOT': '/me', 'INK_REVIEW_STAGE': 'source', 'INK_REVIEW_CHILD': '1'}
                if change: env['INK_REVIEW_CHANGE'] = '1'
                boot(path, 'run', env)
        for change in (False, True):
            path = work / ('copy-tree' + ('-switch' if change else '-control'))
            (path / 'sd').mkdir(parents=True)
            env = {'INK_REVIEW_OPERATION': 'file.copy', 'INK_REVIEW_ROOT': '/me', 'INK_REVIEW_STAGE': 'source', 'INK_REVIEW_TREE': '1'}
            if change: env['INK_REVIEW_CHANGE'] = '1'
            boot(path, 'run', env)
    elif name == 'upgrade_crash':
        spec = importlib.util.spec_from_file_location('inkfw', ROOT / 'tools/inkfw/make_inkfw.py')
        fw = importlib.util.module_from_spec(spec); spec.loader.exec_module(fw)
        for cut in ('control', 'before', 'after', 'error'):
            path = work / ('commit-' + cut)
            folder = path / 'sd/content/update'; folder.mkdir(parents=True)
            (folder / 'next.inkfw').write_bytes(fw.build('2.0.0', 200, app_size=8192, font=4096, resources=8192)[0])
            boot(path, 'install', {'INK_REVIEW_INSTALL': '1'}, 'test.finished')
            checkpoint = {'before': 'review.cut_before_valid', 'after': 'review.cut_after_valid', 'error': 'review.valid_error'}.get(cut)
            boot(path, 'trial', {'INK_REVIEW_CUT': cut} if checkpoint else {}, checkpoint)
            env = {'INK_REVIEW_EXPECT_ROLLBACK': '1'} if cut in ('before', 'error') else {}
            if cut == 'after': env['INK_REVIEW_EXPECT_RESOURCES'] = '1'
            boot(path, 'recovery', env)
            boot(path, 'next-boot', {'INK_REVIEW_EXPECT_ROLLBACK': '1'} if cut in ('before', 'error') else {})
    elif name == 'file_late':
        for operation in ('atomic', 'open'):
            for variant in ('control', 'removed', 'reused'):
                path = work / ('file-' + operation + '-' + variant)
                (path / 'sd').mkdir(parents=True)
                env = {'INK_REVIEW_FILE_OPEN': '1'} if operation == 'open' else {}
                if variant == 'control': env['INK_REVIEW_CONTROL'] = '1'
                if variant == 'removed': env['INK_REVIEW_NO_REUSE'] = '1'
                boot(path, 'run', env)
    elif name == 'late_writers':
        for writer in ('app_write', 'app_remove', 'media_flash', 'media_card', 'notify', 'notify_prefs', 'activity'):
            for variant in ('control', 'removed', 'reused'):
                path = work / (writer + '-' + variant)
                (path / 'sd').mkdir(parents=True)
                env = {'INK_REVIEW_WRITER': writer}
                if variant == 'control': env['INK_REVIEW_CONTROL'] = '1'
                if variant == 'removed': env['INK_REVIEW_NO_REUSE'] = '1'
                boot(path, 'run', env)
    elif name == 'reader_late':
        for callback in ('destroy', 'timer', 'submit'):
            for variant in ('control', 'removed', 'reused'):
                path = work / ('reader-' + callback + '-' + variant)
                (path / 'sd').mkdir(parents=True)
                env = {'INK_REVIEW_SAVE_TIMER': '1'} if callback == 'timer' else {}
                if callback == 'submit': env['INK_REVIEW_SAVE_GAP'] = '1'
                if variant == 'control': env['INK_REVIEW_CONTROL'] = '1'
                if variant == 'removed': env['INK_REVIEW_NO_REUSE'] = '1'
                boot(path, 'run', env)
    elif name in ('removal_alarm', 'empty_removal', 'update_history', 'member_inventory'):
        path = work / (name + '-state')
        (path / 'sd').mkdir(parents=True)
        phases = ('prepare', 'inspect') if name in ('removal_alarm', 'member_inventory') else ('legacy', 'rollback', 'inspect') if name == 'update_history' else ('run',)
        for phase in phases:
            env = {'INK_REVIEW_INSPECT': '1'} if phase == 'inspect' else {}
            env['INK_REVIEW_HISTORY'] = phase
            boot(path, phase, env)
    else:
        for failure in ('card', 'flash', 'cache', 'pin'):
            path = work / ('removal-' + failure)
            (path / 'sd').mkdir(parents=True)
            for phase in ('prepare', 'retry', 'inspect'):
                boot(path, phase, {'INK_REVIEW_PHASE': phase, 'INK_REVIEW_FAILURE': failure},
                     'review.removal_checkpoint' if phase != 'inspect' else None)
    return all_ok


def update_matrix(binary):
    spec = importlib.util.spec_from_file_location('inkfw', ROOT / 'tools/inkfw/make_inkfw.py')
    fw = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(fw)
    import hashlib
    import hmac
    import struct
    all_ok = True
    def package(code=200):
        return fw.build(str(code // 100) + '.0.0', code, app_size=8192, font=4096, resources=8192)[0]
    def chain(name):
        path = work / (name + '-state')
        (path / 'sd/content/update').mkdir(parents=True)
        return path
    def boot(path, phase, label, **env):
        nonlocal all_ok
        out = path / label
        cmd = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
               '--out', str(out), '--end', '80s', '--quiet', '--no-random-outages',
               '--flash-image', str(path / 'user.img'), '--sd-dir', str(path / 'sd')]
        if phase == 'download':
            script = path / 'network.script'
            script.write_text('@0 net wifi\n@100s end\n')
            cmd += ['--script', str(script)]
        run = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=180,
                             env={**os.environ, 'INK_FINISH_PHASE': phase, **env})
        metrics = json.loads((out / 'metrics.json').read_text())
        m = metrics['metrics']
        ok = run.returncode == 0 and m.get('test.fail', 0) == 0
        if env.get('INK_FINISH_LOSE_STATE'):
            ok &= m.get('update.self_check_failed') == 1 and not m.get('ota.marked_valid', 0)
        else:
            ok &= m.get('test.finished') == 1
            ok &= bool(m.get('power.reboot')) == (phase == 'install')
        print('update/' + label, 'PASS' if ok else 'FAIL', 'exit=' + str(run.returncode), flush=True)
        if not ok:
            for warning in metrics.get('warnings', []): print(warning)
        all_ok &= ok
        return m
    for failure in ('1', '2'):
        path = chain('state-refused-' + failure)
        (path / 'sd/content/update/next.inkfw').write_bytes(package())
        boot(path, 'refuse', 'state-refused-' + failure, INK_FINISH_REFUSE=failure)
    path = chain('resource-control')
    (path / 'sd/content/update/next.inkfw').write_bytes(package())
    boot(path, 'install', 'install')
    boot(path, 'inspect', 'recovered')
    path = chain('lost-state')
    (path / 'sd/content/update/next.inkfw').write_bytes(package())
    boot(path, 'install', 'install')
    boot(path, 'inspect', 'lost-state', INK_FINISH_LOSE_STATE='1')
    path = chain('resource-preserved')
    (path / 'sd/content/update/next.inkfw').write_bytes(package())
    boot(path, 'install', 'install')
    boot(path, 'inspect', 'partial-resources', INK_SIM_OTA_RESOURCE_FAILS='8192')
    (path / 'sd/content/update/next.inkfw').write_bytes(fw.build('3.0.0', 300, app_size=8192)[0])
    boot(path, 'blocked', 'resources-preserved', INK_SIM_OTA_RESOURCE_FAILS='8192')
    path = chain('tables')
    folder = path / 'sd/content/cases'
    folder.mkdir()
    normal = package()
    cases = {'normal': normal}
    def resign(data):
        size = struct.unpack_from('<I', data, 12)[0]
        data[size - 64:size] = bytes(64)
        data[size - 64:size - 32] = hmac.new(fw.KEY, hashlib.sha256(data[:size]).digest(), hashlib.sha256).digest()
        return data
    data = bytearray(normal); data[152:200] = data[96:144]; cases['overlap'] = resign(data)
    data = bytearray(normal); struct.pack_into('<I', data, 144, 0); cases['duplicate'] = resign(data)
    data = bytearray(normal); struct.pack_into('<I', data, 144, 99); cases['unknown'] = resign(data)
    data = bytearray(normal); struct.pack_into('<QQ', data, 96, 0, 0); data[112:144] = hashlib.sha256(b'').digest(); cases['zero'] = resign(data)
    data = bytearray(normal); struct.pack_into('<Q', data, 96, len(data) + 4096); cases['past_eof'] = resign(data)
    cases['truncated'] = normal[:-2048]
    data = bytearray(normal); struct.pack_into('<QQ', data, 96, 2**64 - 8, 32); cases['overflow'] = resign(data)
    data = bytearray(normal); data[96] ^= 0x20; cases['unsigned_damage'] = data
    for name, data in cases.items(): (folder / (name + '.inkfw')).write_bytes(data)
    boot(path, 'tables', 'tables')
    path = chain('offer-changed')
    old = path / 'old.inkfw'; new = path / 'new.inkfw'
    old.write_bytes(fw.build('2.0.0', 200, app_size=131072)[0])
    new.write_bytes(fw.build('3.0.0', 300, app_size=196608)[0])
    boot(path, 'download', 'offer-changed', INK_SIM_UPDATE_PKG=str(old), INK_FINISH_NEW_PKG=str(new))
    path = chain('offer-kept')
    boot(path, 'download', 'offer-kept', INK_SIM_UPDATE_PKG=str(old), INK_FINISH_NEW_PKG=str(new), INK_FINISH_OLD_PKG=str(old), INK_FINISH_RESTORE_OFFER='1')
    return all_ok

failed = False
for name in a.suites:
    binary, end = compile_suite(name)
    if name == 'update':
        failed |= not update_matrix(binary)
        continue
    if name in ('upgrade_crash', 'removal', 'removal_alarm', 'empty_removal', 'update_history', 'reader_late', 'member_inventory', 'late_writers', 'file_late', 'file_events', 'file_paths'):
        failed |= not review_matrix(name, binary)
        continue
    out = work / (name + '-out')
    cmd = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
           '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
           '--out', str(out), '--end', end, '--quiet', '--no-random-outages']
    if name in ('voice', 'handoff', 'isolation', 'file_reuse'): cmd += ['--script', str(ROOT / 'tests/services_finish/voice.script')]
    run = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=180)
    metrics = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    counters = metrics.get('metrics', {})
    (work / (name + '.stdout.log')).write_text(run.stdout)
    (work / (name + '.stderr.log')).write_text(run.stderr)
    ok = run.returncode == 0 and counters.get('test.finished') == 1 and counters.get('test.fail', 0) == 0
    print(name, 'PASS' if ok else 'FAIL', 'exit=' + str(run.returncode),
          'checks=' + str(counters.get('test.pass', 0)), 'failures=' + str(counters.get('test.fail', 0)), flush=True)
    if not ok:
        print(run.stderr[-2000:])
        for warning in metrics.get('warnings', []):
            print(warning)
    failed |= not ok
print('Evidence directory:', work.relative_to(ROOT))
raise SystemExit(1 if failed else 0)
