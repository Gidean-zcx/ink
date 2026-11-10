"""Exercise saved Wi-Fi and redo; baseline controls are manual, outside CTest."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = '424fc6b206bbeca53438680d0973c9f2e6f338dc'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--reverse', action='store_true')
a = p.parse_args()
build = a.build.resolve()
area = build / 'settings-small-evidence'
area.mkdir(parents=True, exist_ok=True)


def run(binary, name, expected=None):
    out = area / name
    shutil.rmtree(out, ignore_errors=True)
    out.mkdir()
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--end', '180s',
        '--out', str(out), '--flash-image', str(out / 'user.img'), '--sd-dir', str(out / 'sd'),
        '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')],
        cwd=ROOT, capture_output=True, text=True, timeout=240,
        env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (out / 'run.log').write_text(result.stdout + result.stderr)
    data = json.loads((out / 'metrics.json').read_text())
    metrics = data['metrics']
    passed = result.returncode == 0 and metrics.get('test.finished') == 1 and not metrics.get('test.fail', 0) and not data.get('warnings')
    print(f'{name}: exit={result.returncode}, pass={metrics.get("test.pass", 0)}, '
          f'fail={metrics.get("test.fail", 0)}, finished={metrics.get("test.finished", 0)}', flush=True)
    if expected:
        assert not passed and metrics.get('test.fail', 0) > 0 and expected in (out / 'run.log').read_text(), data.get('warnings')
    else:
        assert passed, (data.get('warnings'), result.stderr[-5000:])
        assert 'SYNTH_ONLY_PASS' not in (out / 'run.log').read_text(), 'credential leaked into log'
        assert all((out / f'clean-{scene}.png').exists() for scene in ('networks-zh', 'networks-en', 'redo-zh', 'redo-en'))


if not a.reverse:
    run(build / 'sim-settings-small', 'fixed')
else:
    db = json.loads((build / 'compile_commands.json').read_text())
    for unit, expected in (
        ('ui/shell/navigation', 'network settings exposes saved networks'),
        ('ui/shell/keyboard', 'redo enablement matches the session'),
        ('services/net/wifi', 'undo forgotten credentials without exposing the password'),
    ):
        path = 'system/' + unit + '.c'
        source = subprocess.run(['git', 'show', BASE + ':' + path], cwd=ROOT, capture_output=True, text=True)
        if source.returncode:
            raise SystemExit('Baseline history is required only for this manual --reverse mode.')
        entry = next(e for e in db if Path(e['file']) == ROOT / path)
        command = shlex.split(entry['command'])
        obj = area / (Path(unit).name + '.o')
        command[command.index('-o') + 1] = str(obj)
        del command[command.index('-c'):command.index('-c') + 2]
        subprocess.run(command + ['-iquote', str((ROOT / path).parent), '-x', 'c', '-c', '-'], input=source.stdout,
                       cwd=build, text=True, check=True)
        link = shlex.split((build / 'CMakeFiles/sim-settings-small.dir/link.txt').read_text())
        binary = area / ('baseline-' + Path(unit).name)
        link[link.index('-o') + 1] = str(binary)
        link.insert(next(i for i, part in enumerate(link) if part.endswith('libink_system.a')), str(obj))
        subprocess.run(link, cwd=build, check=True)
        run(binary, binary.name, expected)
if not os.environ.get('INK_KEEP_EVIDENCE'):
    shutil.rmtree(area)
