"""Manual reverse comparison; normal CTest never depends on Git history."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--baseline', required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'small-gaps-3-reverse'
area.mkdir(exist_ok=True)
db = json.loads((build / 'compile_commands.json').read_text())
results = []
for case, files in {
    'member': ['system/ui/shell/member_gate.c'],
    'backup': ['system/services/backup/backup_service.c', 'system/ui/shell/backup_pages.c'],
    'notes': ['system/services/update/update_service.c', 'system/ui/shell/update_pages.c'],
    'empty-notes': ['system/services/update/update_service.c', 'system/ui/shell/update_pages.c'],
    'sample': ['apps/reader/reader.c', 'system/ui/shell/boot_flow.c'],
}.items():
    folder = area / case
    folder.mkdir(exist_ok=True)
    target = 'sim-semantics-shell' if case == 'member' else 'sim-small-gaps-3'
    objects = []
    for file in files:
        entry_file = ROOT / ('tests/semantics_shell/member.c' if case == 'member' else file)
        entry = next(e for e in db if Path(e['file']) == entry_file)
        original = subprocess.check_output(['git', 'show', f'{a.baseline}:{file}'], cwd=ROOT, text=True)
        source = folder / Path(file).name
        source.write_text(original)
        obj = source.with_suffix('.o')
        cmd = shlex.split(entry['command'])
        cmd[cmd.index('-o') + 1] = str(obj)
        if case == 'member':
            cmd += ['-I' + str(ROOT / 'system/ui/shell'), '-DINK_TEST_SHELL_SOURCE=' + json.dumps(str(source))]
        else:
            cmd[cmd.index(str(entry_file))] = str(source)
            cmd += ['-I' + str(entry_file.parent)]
        r = subprocess.run(cmd, cwd=entry['directory'], capture_output=True, text=True)
        (folder / (source.stem + '.compile.log')).write_text(r.stdout + r.stderr)
        if r.returncode:
            raise SystemExit(f'{case}: reverse compilation failed: {r.stderr[-2000:]}')
        objects.append(str(obj))
    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    binary = folder / ('reverse-' + case)
    link[link.index('-o') + 1] = str(binary)
    if case == 'member':
        index = next(i for i, v in enumerate(link) if v.endswith('semantics_shell/member.c.o'))
        link[index] = objects[0]
    else:
        index = next(i for i, v in enumerate(link) if v.endswith('libink_apps.a'))
        link[index:index] = objects
    r = subprocess.run(link, cwd=build, capture_output=True, text=True)
    (folder / 'link.log').write_text(r.stdout + r.stderr)
    if r.returncode:
        raise SystemExit(f'{case}: reverse link failed: {r.stderr[-2000:]}')
    if case == 'member':
        command = [str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
                   '--script', str(ROOT / 'tests/semantics_shell/scenes.script'), '--budget', str(ROOT / 'tests/ui_scene.budget.json'), '--assets', str(build / 'assets'),
                   '--content', str(build / 'test-content'), '--out', str(folder / 'device'), '--quiet', '--no-random-outages']
        env = dict(os.environ, INK_SHELL_SCENE='member')
    else:
        command = [sys.executable, str(ROOT / 'tests/small_gaps_3/run.py'), '--build', str(build),
                   '--case', case, '--binary', str(binary)]
        env = dict(os.environ, INK_KEEP_EVIDENCE='1')
    r = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=200)
    (folder / 'test.log').write_text(r.stdout + r.stderr)
    evidence = folder / 'device/metrics.json' if case == 'member' else build / f'test-out/small-gaps-3-{case}-reverse/device/metrics.json'
    data = json.loads(evidence.read_text()) if evidence.exists() else {}
    result = {'case': case, 'exit': r.returncode, 'failures': data.get('metrics', {}).get('test.fail', 0),
              'warnings': data.get('warnings', [])}
    results.append(result)
    print(json.dumps(result, ensure_ascii=False), flush=True)
    if r.returncode == 0 or not result['failures']:
        raise SystemExit(f'{case}: baseline did not fail a real assertion')
(area / 'summary.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')
