"""Manual baseline controls; compile temporary objects and leave tracked sources untouched."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', required=True, type=Path)
p.add_argument('--baseline', required=True)
a = p.parse_args()
build = a.build.resolve()
area = build / 'audit14-reverse'
area.mkdir(exist_ok=True)
cases = [
    ('page-gate', 'system/policy/cap.c', 'sim-audit14-locked', 'tests/time_settings.py', [], 'declared action works without opening the private page'),
    ('page-input', 'system/services/input/input_service.c', 'sim-audit14-locked', 'tests/time_settings.py', [], 'declared page closes via capability and physical HOME'),
    ('page-home', 'system/policy/system_actions.c', 'sim-audit14-locked', 'tests/time_settings.py', [], 'declared page closes via capability and physical HOME'),
    ('ownership', 'system/policy/members.c', 'sim-kid-apps', 'tests/kid_apps.py', ['--phase', 'ownership'], 'list denies audit14.alias'),
    ('batch', 'system/services/file/file_caps.c', 'sim-files-ops', 'tests/files_ops/run.py', [], 'AI partial collision summarizes failed files'),
    ('redraw', 'system/ui/shell/time_settings.c', 'sim-time-settings', 'tests/time_settings.py', [], 'alarm changes keep the time page controls alive'),
]
summary = []
for name, file, target, runner, extra, expected in cases:
    path = ROOT / file
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    baseline = subprocess.run(['git', 'show', f'{a.baseline}:{file}'], cwd=ROOT, capture_output=True, text=True, check=True).stdout
    mutant = area / f'{name}.c'
    mutant.write_text(baseline)
    flags = {}
    for line in (build / 'CMakeFiles/ink_system.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
    obj, binary = area / f'{name}.o', area / name
    subprocess.run([link[0], *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS'], '-iquote', str(path.parent), '-c', str(mutant), '-o', str(obj)], cwd=build, check=True, capture_output=True, text=True)
    link[link.index('-o') + 1] = str(binary)
    link.insert(next(i for i, item in enumerate(link) if item.endswith('libink_system.a')), str(obj))
    subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
    result = subprocess.run([os.sys.executable, str(ROOT / runner), '--build', str(build), '--binary', str(binary), *extra], cwd=ROOT,
        capture_output=True, text=True, timeout=350, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    output = result.stdout + result.stderr
    (area / f'{name}.log').write_text(output)
    match = re.search(r'Evidence: (.+)', output)
    evidence = ROOT / match[1].strip() if match else build / 'time-settings-evidence' / name
    metrics = [json.loads(file.read_text(errors='replace')) for file in evidence.rglob('metrics.json')]
    failures = '\n'.join(warning for data in metrics for warning in data.get('warnings', []))
    assert result.returncode != 0 and expected in output + failures, (output + failures)[-8000:]
    assert any(data.get('metrics', {}).get('test.finished') == 1 and data.get('metrics', {}).get('test.fail', 0) > 0 for data in metrics), output
    assert hashlib.sha256(path.read_bytes()).hexdigest() == digest
    summary.append({'case': name, 'exit': result.returncode, 'expected_failure': expected})
    print(json.dumps(summary[-1], ensure_ascii=False), flush=True)

path = ROOT / 'system/services/hub/hub_link.c'
source = path.read_text()
baseline = subprocess.run(['git', 'show', f'{a.baseline}:system/services/hub/hub_link.c'], cwd=ROOT, capture_output=True, text=True, check=True).stdout
for name, declaration in [('clock-reader', 'bool ink_hub_server_time'), ('clock-send', 'static void send_hello'), ('clock-hello', 'static void on_text')]:
    pattern = re.escape(declaration) + r'\([^\n]*\) \{[\s\S]*?\n\}'
    old = re.search(pattern, baseline)[0]
    current = re.search(pattern, source)[0]
    mutant = area / f'{name}.c'
    mutant.write_text(source.replace(current, old, 1))
    result = subprocess.run([os.sys.executable, str(ROOT / 'tests/audit14_locks.py'), '--source', str(mutant)], capture_output=True, text=True)
    output = result.stdout + result.stderr
    (area / f'{name}.log').write_text(output)
    assert result.returncode != 0 and 'shared clock access without id_lock' in output, output
    summary.append({'case': name, 'exit': result.returncode, 'expected_failure': 'shared clock access without id_lock'})
    print(json.dumps(summary[-1], ensure_ascii=False), flush=True)
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
