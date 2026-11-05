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
    'files': ('sim-media', ['file_boundary.c'], '75s'),
    'storage': ('sim-media', [], '75s'),
    'update': ('sim-media', [], '80s'),
    'text': ('sim-media', ['text_boundary.c'], '20s'),
    'audio': ('sim-media', [], '40s'),
    'voice': ('sim-media', ['audio_boundary.c'], '40s'),
    'shutdown': ('sim-media', [], '30s'),
    'actions': ('sim-media', [], '15s'),
    'policy': ('sim-media', ['settings_boundary.c'], '40s'),
}

def compile_suite(name):
    template, extras, end = suites[name]
    flags = {}
    for line in (build / 'CMakeFiles' / (template + '.dir') / 'flags.make').read_text().splitlines():
        if ' = ' in line:
            k, v = line.split(' = ', 1)
            flags[k] = shlex.split(v)
    link = shlex.split((build / 'CMakeFiles' / (template + '.dir') / 'link.txt').read_text())
    defines = []
    if a.baseline:
        # Read only: source copies and binaries live in the ignored build directory.
        for rel in ['system/services/file/file_service.c', 'system/services/data/datastore.c', 'system/services/update/update_service.c', 'system/services/text/text_service.c', 'system/services/audio/audio_service.c', 'system/services/power/power_service.c', 'system/services/ai/ai_service.c', 'system/policy/cap.c', 'system/policy/settings.c']:
            target = work / Path(rel).name
            target.write_bytes(subprocess.check_output(['git', 'show', a.baseline + ':' + rel], cwd=ROOT))
            macro = 'INK_TEST_' + {'file_service.c': 'FILE', 'datastore.c': 'DATA', 'update_service.c': 'UPDATE', 'text_service.c': 'TEXT', 'audio_service.c': 'AUDIO', 'power_service.c': 'POWER', 'ai_service.c': 'AI', 'cap.c': 'CAP', 'settings.c': 'SETTINGS'}[Path(rel).name] + '_SOURCE'
            defines += ['-D' + macro + '=' + json.dumps(str(target))]
    objects = []
    for i, source in enumerate([name + '.c', *extras]):
        obj = work / (name + '-' + str(i) + '.o')
        cmd = [link[0], *flags['C_DEFINES'], *defines, *flags['C_INCLUDES'],
               '-I' + str(ROOT / 'system/services/file'), '-I' + str(ROOT / 'system/services/data'), '-I' + str(ROOT / 'system/services/text'), '-I' + str(ROOT / 'system/policy'),
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
        path = work / name
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
    out = work / (name + '-out')
    cmd = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
           '--assets', str(build / 'assets'), '--content', str(build / 'test-content'),
           '--out', str(out), '--end', end, '--quiet', '--no-random-outages']
    if name == 'voice': cmd += ['--script', str(ROOT / 'tests/services_finish/voice.script')]
    run = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=180)
    metrics = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    counters = metrics.get('metrics', {})
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
