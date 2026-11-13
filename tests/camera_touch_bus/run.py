"""HOME presses at many phases of the camera viewfinder all arrive and the viewfinder keeps its pace. Reverse checks:
--reverse rewrite loads the sensor table for every preview frame again (the viewfinder slows down), --reverse yield
keeps the bus through the table load (presses while it opens are lost), --reverse both is the code before the fix
(presses during the viewfinder are lost)."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--build', required=True, type=Path)
p.add_argument('--reverse', choices=('rewrite', 'yield', 'both'))
a = p.parse_args()
build = a.build.resolve()
work = Path(tempfile.mkdtemp(prefix='camera-touch-bus-', dir=build))
binary = build / 'sim-camera-touch-bus'
MUTANTS = {  # reverse checks: what each restores
    'rewrite': ('simulator/models/dev_camera.c', '    bool kept = g_kept;\n', '    bool kept = false; /* reverse: every capture loads the table */\n'),
    'yield': ('drivers/chips/gc2145/gc2145.c', '        osal_task_yield();\n', '        /* reverse: no yield between table writes */\n'),
}
if a.reverse:
    link = shlex.split((build / 'CMakeFiles/sim-camera-touch-bus.dir/link.txt').read_text())
    at = next(i for i, x in enumerate(link) if x.endswith('.a'))  # before the libraries: a mutant replaces its original
    entries = json.loads((build / 'compile_commands.json').read_text())
    for name in (('rewrite', 'yield') if a.reverse == 'both' else (a.reverse,)):
        relative, old, changed = MUTANTS[name]
        model = ROOT / relative
        original = model.read_text()
        assert original.count(old) == 1
        mutant = work / model.name
        mutant.write_text(original.replace(old, changed))
        entry = next(e for e in entries if Path(e['file']).resolve() == model)
        command = shlex.split(entry['command']) if 'command' in entry else entry['arguments'][:]
        obj = work / (model.stem + '.o')
        command[command.index(next(x for x in command if x.endswith(relative)))] = str(mutant)
        command[command.index('-o') + 1] = str(obj)
        subprocess.run(command, cwd=entry['directory'], check=True, capture_output=True, text=True)
        link.insert(at, str(obj))
    binary = work / 'sim-reverse'
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
out = work / 'out'
result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
    '--script', str(ROOT / 'tests/camera_touch_bus/camera.script'), '--assets', str(build / 'assets'),
    '--content', str(build / 'test-content'), '--out', str(out), '--quiet', '--no-random-outages'],
    cwd=ROOT, capture_output=True, text=True, timeout=300)
log = result.stdout + result.stderr
(work / 'run.log').write_text(log)
report = json.loads((out / 'metrics.json').read_text(errors='replace')) if (out / 'metrics.json').exists() else {}
counts = report.get('metrics', {})
evidence = log + json.dumps(report.get('warnings', []), ensure_ascii=False)
warnings = json.dumps(report.get('warnings', []))
lost, lost_preview = warnings.count('reaches home'), warnings.count('after a preview frame reaches home')
frames = counts.get('camera_touch_bus.frames_20s', {}).get('max', '?')
print('exit', result.returncode, 'assertions', counts.get('test.pass'), 'failures', counts.get('test.fail', 0),
      'lost HOME presses', lost, '(during preview frames', lost_preview, ') viewfinder frames in 20 s', frames, flush=True)
if a.reverse:
    slow = 'viewfinder keeps at least 20 frames' in evidence
    expected = slow if a.reverse == 'rewrite' else lost_preview > 0 and slow if a.reverse == 'both' else lost > 0
    assert result.returncode == 0 and counts.get('test.finished') == 1 and expected, evidence[-4000:]
    print('PASS: the reverse check (%s) fails the scenario' % a.reverse, flush=True)
else:
    assert result.returncode == 0 and counts.get('test.finished') == 1 and not counts.get('test.fail', 0), evidence[-4000:]
if os.environ.get('INK_KEEP_EVIDENCE'):
    print('Evidence:', work)
else:
    shutil.rmtree(work)
