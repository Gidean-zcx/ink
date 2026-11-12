"""Manual old-keyboard control for the one-second memo script; CI needs no Git history."""
import json
import os
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
build = root / 'build'
area = build / 'test-out/ime-autotest-baseline'
area.mkdir(parents=True, exist_ok=True)
db = json.loads((build / 'compile_commands.json').read_text())
source = root / 'system/ui/shell/keyboard.c'
old = area / 'keyboard.c'
old.write_text(subprocess.check_output(['git', 'show', '6171c2bf8d59238c95f2d548663f8b2ab3c6f43a:system/ui/shell/keyboard.c'], text=True))
entry = next(e for e in db if Path(e['file']) == source)
cmd = shlex.split(entry['command'])
obj = area / 'keyboard.o'
cmd[cmd.index('-o')+1] = str(obj)
cmd[cmd.index(str(source))] = str(old)
cmd += ['-I' + str(source.parent)]
subprocess.run(cmd, cwd=entry['directory'], check=True)
link = shlex.split((build / 'CMakeFiles/sim-autotest.dir/link.txt').read_text())
binary = area / 'sim-autotest'
link[link.index('-o')+1] = str(binary)
link.insert(link.index('libink_system.a'), str(obj))
subprocess.run(link, cwd=build, check=True)
r = subprocess.run([str(binary), '--profile', str(root / 'simulator/profiles/metalio-plus.json'),
                    '--script', str(root / 'tools/device/autotest/memo.script'), '--assets', str(build / 'assets'),
                    '--content', str(build / 'test-content'), '--out', str(area / 'out'), '--quiet', '--no-random-outages'],
                   env={**os.environ, 'INK_AUTOTEST_SCENE': 'memo'}, capture_output=True, text=True, timeout=120)
(area / 'run.log').write_text(r.stdout+r.stderr)
d=json.loads((area / 'out/metrics.json').read_text())
print('baseline keyboard + 1s memo script:', r.returncode, d['metrics'].get('test.fail',0))
print('\n'.join(d.get('warnings', [])))

matched = d["metrics"].get("test.fail", 0) > 0 and any("keyboard taps type hello" in w or "backspace erases the new draft text" in w for w in d.get("warnings", []))
raise SystemExit(0 if matched else 1)
