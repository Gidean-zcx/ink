#!/usr/bin/env python3
"""Build/test without modifying sources. Raw build output stays outside the repository."""
import argparse, json, os, pathlib, shutil, subprocess, time
R = pathlib.Path(__file__).resolve().parents[4]
A = pathlib.Path(__file__).resolve().parents[1]
W = pathlib.Path(os.environ.get('INK_AUDIT_WORK', R.parent / 'ink-audit-work'))
CMAKE = shutil.which('cmake') or '/root/.local/bin/cmake'
CTEST = str(pathlib.Path(CMAKE).with_name('ctest'))
FLAGS = {'normal': [], 'werror': ['-DINK_WERROR=ON'],
         'asan': ['-DINK_SANITIZE=ON'], 'debug': ['-DCMAKE_BUILD_TYPE=Debug'],
         'clang': ['-DCMAKE_C_COMPILER=clang', '-DINK_WERROR=ON'],
         'clang-no-werror': ['-DCMAKE_C_COMPILER=clang']}
def run(mode):
    b = W / ('build-' + mode); W.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy(); result = {'mode': mode, 'steps': []}
    commands = [[CMAKE, '-S', str(R), '-B', str(b), '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                 '-DFETCHCONTENT_SOURCE_DIR_LVGL=' + os.environ.get('LVGL', '/opt/lvgl')] + FLAGS[mode],
                [CMAKE, '--build', str(b), '-j4'], [CTEST, '--test-dir', str(b), '-N'],
                [CTEST, '--test-dir', str(b), '--output-on-failure', '-j1']]
    for i, cmd in enumerate(commands):
        start = time.monotonic()
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
        raw = p.stdout.decode(errors='replace')
        (W / f'{mode}-{i}.log').write_text(raw)
        result['steps'].append({'command': cmd, 'exit': p.returncode,
                                'seconds': round(time.monotonic()-start,3), 'tail': raw[-2500:]})
        if i >= 2: (A / 'logs' / f'{mode}-{i}.txt').write_text(raw[-150000:])
        if p.returncode:
            if mode == 'asan' and i == 3:
                # LSan cannot enumerate threads in this environment; ASan and UBSan stay enabled.
                env['ASAN_OPTIONS'] = 'detect_leaks=0'
                start = time.monotonic()
                q = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
                tail = q.stdout.decode(errors='replace')
                (A / 'logs' / 'asan-no-lsan.txt').write_text(tail[-150000:])
                result['retry'] = {'command': cmd, 'env': {'ASAN_OPTIONS':'detect_leaks=0'},
                                   'exit': q.returncode, 'seconds':round(time.monotonic()-start,3), 'tail':tail[-2500:]}
            break
    (A / 'logs' / f'build-{mode}.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
    print(mode, [(x['exit'], x['seconds']) for x in result['steps']], flush=True)
if __name__ == '__main__':
    p=argparse.ArgumentParser(); p.add_argument('modes', nargs='*', default=['normal','werror','asan','debug'])
    for m in p.parse_args().modes: run(m)
