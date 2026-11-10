"""Manual mutation checks; compile copies in the build directory without editing product sources."""
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
a = p.parse_args()
build = a.build.resolve()
area = build / 'mobile-network-reverse'
area.mkdir(exist_ok=True)
db = {Path(e['file']).resolve(): e for e in json.loads((build / 'compile_commands.json').read_text())}
cases = (
    ('apn-command', 'drivers/chips/nt26/nt26.c',
     '"AT+CGDCONT=1,\\\"IP\\\",\\\"%.100s\\\"", name);',
     '"AT+CGDCONT=1,\\\"IP\\\",\\\"%.100s\\\"", "");', 'model received the expected APN command'),
    ('password-gate', 'system/services/comm/comm_service.c',
     'if (password && c->src != INK_SRC_TOUCH && c->src != INK_SRC_KEY) { c->rc.status = INK_RC_REJECTED; return; }',
     '(void)password;', 'AI cannot supply a password'),
    ('roaming-gate', 'system/services/net/net_service.c',
     '(!roaming || ink_setting_get("net.roaming"))', '(roaming >= 0)', 'roaming registration disables mobile data'),
    ('byte-count', 'system/services/comm/comm_service.c',
     'CM.usage.current += rx + tx - CM.sampled_bytes;', 'CM.usage.current += 0;', 'month counts exact receive and transmit interface bytes'),
    ('daily-checkpoint', 'system/services/comm/comm_service.c',
     'ink_db_t *db = ink_db_open(&MOBILE, NULL); uint32_t ver = 0;',
     'if (id == 2) return true;\n    ink_db_t *db = ink_db_open(&MOBILE, NULL); uint32_t ver = 0;',
     'restart restores both months'),
)
summary = []
for name, file, old, new, expected in cases:
    path = ROOT / file
    source = path.read_text()
    assert source.count(old) == 1, (name, source.count(old))
    copy, obj, binary = area / (name + '.c'), area / (name + '.o'), area / name
    copy.write_text(source.replace(old, new))
    entry = db[path.resolve()]
    compile_cmd = shlex.split(entry['command']) if 'command' in entry else list(entry['arguments'])
    compile_cmd[compile_cmd.index(entry['file'])] = str(copy)
    compile_cmd[compile_cmd.index('-o') + 1] = str(obj)
    compile_cmd += ['-iquote', str(path.parent)]
    result = subprocess.run(compile_cmd, cwd=entry.get('directory', build), capture_output=True, text=True)
    (area / (name + '-compile.log')).write_text(result.stdout + result.stderr)
    assert result.returncode == 0, (name, result.stderr[-1500:])
    link = shlex.split((build / 'CMakeFiles/sim-mobile-network.dir/link.txt').read_text())
    link[link.index('-o') + 1] = str(binary)
    library = 'libink_drivers.a' if file.startswith('drivers/') else 'libink_system.a'
    link.insert(next(i for i, s in enumerate(link) if s.endswith(library)), str(obj))
    subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
    result = subprocess.run([sys.executable, '-B', str(ROOT / 'tests/mobile_network/run.py'), '--build', str(build), '--binary', str(binary)],
        cwd=ROOT, capture_output=True, text=True, timeout=300, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    (area / (name + '.log')).write_text(result.stdout + result.stderr)
    assert result.returncode != 0 and expected in result.stdout + result.stderr, (name, result.stderr[-3500:])
    summary.append({'case': name, 'exit': result.returncode, 'expected_failure': expected})
    print(f'{name}: expected failure ({expected})', flush=True)
(area / 'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
