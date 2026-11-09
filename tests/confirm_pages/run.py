"""Check real confirmation pages; optional isolated mutations leave product sources unchanged."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SCENES = ('ConfirmWeb', 'ConfirmParent', 'ConfirmCall', 'ConfirmSetting', 'ConfirmExpired', 'StressConfirm')
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--scene', choices=SCENES)
p.add_argument('--mutant', choices=('ai-source', 'queue-reject', 'queue-target'))
p.add_argument('--baseline')
a = p.parse_args()
build = a.build.resolve()
tag = a.mutant or ('baseline' if a.baseline else 'current')
area = build / 'test-out' / ('confirm-pages-' + tag)
shutil.rmtree(area, ignore_errors=True)
area.mkdir(parents=True)
binary = build / 'sim-confirm-pages'
if a.mutant or a.baseline:
    flags = {}
    for line in (build / 'CMakeFiles/sim-confirm-pages.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / 'CMakeFiles/sim-confirm-pages.dir/link.txt').read_text())
    if a.mutant == 'queue-reject':
        source = (ROOT / 'tests/confirm_pages/pages.c').read_text()
        guard = 'if (i == 1) { read_all(tokens[i]); press("确认"); }'
        assert source.count(guard) == 1, 'mutation must change exactly the second queue answer'
        source_file = area / 'pages.c'
        source_file.write_text(source.replace(guard, 'if (i == 1) { read_all(tokens[i]); press("取消"); }'))
        defines, replace = [], 'pages.c.o'
    elif a.mutant:
        source = (ROOT / 'system/policy/confirm.c').read_text()
        if a.mutant == 'ai-source':
            guard, replacement = 'src != INK_SRC_TOUCH && src != INK_SRC_KEY', 'false'
        else:
            guard = 'if (ink_cap_call(h->cap->id, &h->args, &o) == 0) return;'
            replacement = 'if (!strcmp(h->cap->id, "test.stress_page")) { ink_args_init(&h->args); ink_args_int(&h->args, "n", 12); }\n    ' + guard
        assert source.count(guard) == 1, 'mutation must change exactly one confirmation guard or dispatch'
        path = area / 'confirm.c'
        path.write_text(source.replace(guard, replacement))
        source_file = ROOT / 'tests/confirm_pages/boundary.c'
        defines = ['-DINK_TEST_CONFIRM_SOURCE=' + json.dumps(str(path))]
        replace = 'boundary.c.o'
    else:
        path = area / 'confirm_card.c'
        path.write_text(subprocess.check_output(['git', 'show', a.baseline + ':system/ui/shell/confirm_card.c'], cwd=ROOT, text=True))
        source_file, defines, replace = path, [], None
    obj = area / 'replacement.o'
    subprocess.run([link[0], *flags['C_DEFINES'], *defines, *flags['C_INCLUDES'], *flags['C_FLAGS'],
                    '-I' + str(ROOT / 'system/ui/shell'), '-I' + str(ROOT / 'tests/confirm_pages'),
                    '-c', str(source_file), '-o', str(obj)], cwd=build, check=True)
    if replace:
        index = next(i for i, part in enumerate(link) if part.endswith(replace))
        link[index] = str(obj)
    else:
        index = next(i for i, part in enumerate(link) if part.endswith('.a'))
        link.insert(index, str(obj))
    binary = area / 'sim-confirm-pages'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)

ok = True
for scene in (a.scene,) if a.scene else SCENES:
    out = area / scene
    command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
               '--script', str(ROOT / 'tests/confirm_pages/pages.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'),
               '--hub', str(ROOT / 'tests/ui_shell.hub.json'), '--assets', str(build / 'assets'),
               '--content', str(build / 'test-content'), '--out', str(out), '--quiet', '--no-random-outages']
    result = subprocess.run(command, env={**os.environ, 'INK_CONFIRM_SCENE': scene}, capture_output=True, text=True, timeout=90)
    out.mkdir(exist_ok=True)
    (out / 'stdout.log').write_text(result.stdout)
    (out / 'stderr.log').write_text(result.stderr)
    data = json.loads((out / 'metrics.json').read_text()) if (out / 'metrics.json').exists() else {}
    counters = data.get('metrics', {})
    passed = (result.returncode == 0 and counters.get('test.finished') == 1 and not counters.get('test.fail', 0)
              and not data.get('warnings') and counters.get('test.snapshot.settled') == counters.get('test.snapshot.' + scene) == 1
              and (out / 'snap-scene.png').exists())
    print(f'{scene}: {"PASS" if passed else "FAIL"} exit={result.returncode} assertions={counters.get("test.pass", 0)} '
          f'failures={counters.get("test.fail", 0)} internal_min={counters.get("mem.internal_total.min_free_bytes", 0)}', flush=True)
    if not passed:
        print(result.stderr[-3000:], data.get('warnings', []), flush=True)
    ok = ok and passed
if ok and not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
else:
    print('Evidence:', area.relative_to(ROOT), flush=True)
raise SystemExit(0 if ok else 1)
