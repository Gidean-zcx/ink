"""Three isolated UART/mode regressions; no Git history or device access."""
import json
from pathlib import Path
import resource
import shlex
import subprocess
import sys
import tempfile

build = Path(sys.argv[1]).resolve()
port = Path(__file__).resolve().parents[1]
commands = json.loads((build / 'compile_commands.json').read_text())
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
target = 'test-s31-bt' if (build / 'test-s31-bt').exists() else 'test-s31-audio'
mutations = [
    ('connect-without-mode2', 'hal_bt_s31.c',
     'if (ink_mode(2) || ink_send(cmd)', 'if (ink_send(cmd)'),
    ('disconnect-without-mode1', 'hal_bt_s31.c',
     'void hal_bt_disconnect(void) { (void)ink_s31_bh_route(HAL_ROUTE_SPEAKER); }',
     'void hal_bt_disconnect(void) { }'),
    ('second-uart-owner', 'hal_audio_s31.c',
     'if (!ink_s31_bh_ready()) return false;',
     'if (bh1098g_set_mode(hal_uart_open(1, 115200), 1)) return false;'),
]
with tempfile.TemporaryDirectory(prefix='bt-negative-', dir=build) as temp:
    work = Path(temp)
    for name, file, old, new in mutations:
        source = port / file
        body = source.read_text()
        assert body.count(old) == 1, name
        altered = work / file
        altered.write_text(body.replace(old, new))
        entry = next(e for e in commands if Path(e['file']).resolve() == source)
        args = shlex.split(entry['command']) if 'command' in entry else entry['arguments'][:]
        obj = work / 'mutant.o'
        args[args.index('-o') + 1] = str(obj)
        args[args.index(str(source))] = str(altered)
        subprocess.run(args, cwd=entry['directory'], check=True, capture_output=True, text=True)
        link = shlex.split((build / f'CMakeFiles/{target}.dir/link.txt').read_text())
        link = [str(obj) if x.endswith(file + '.o') else x for x in link]
        binary = work / 'test'
        link[link.index('-o') + 1] = str(binary)
        subprocess.run(link, cwd=build, check=True, capture_output=True, text=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)
        assert result.returncode != 0, name
        assert 'assertion' in result.stderr.lower(), result.stderr
        print(f'{name}: compile=0, test={result.returncode}; {result.stderr.strip().splitlines()[-1]}')
