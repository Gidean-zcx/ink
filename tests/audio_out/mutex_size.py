"""Compile a sizeof probe with the S31 flags; inspect its symbol, never run on a device."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
a = p.parse_args()
build = a.build.resolve()
db = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in db if e['file'].endswith('/ports/esp32s31/osal.c'))
cmd = list(entry.get('arguments') or shlex.split(entry['command']))
with tempfile.TemporaryDirectory(prefix='audio-mutex-size-', dir=build) as tmp:
    folder = Path(tmp)
    source, obj = folder / 'probe.c', folder / 'probe.o'
    source.write_text('#include "freertos/FreeRTOS.h"\n#include "freertos/semphr.h"\n'
                      'char ink_pcm_mutex_request_bytes[sizeof(StaticSemaphore_t)];\n')
    cmd[cmd.index('-o') + 1] = str(obj)
    cmd[cmd.index(entry['file'])] = str(source)
    for flag in ['-MF', '-MT', '-MQ']:
        if flag in cmd:
            cmd[cmd.index(flag) + 1] = str(folder / ('probe.d' if flag == '-MF' else 'probe.o'))
    cmd.append('-fno-lto')
    subprocess.run(cmd, cwd=entry['directory'], check=True)
    compiler = Path(cmd[0])
    nm = compiler.with_name(compiler.name.replace('gcc', 'nm'))
    lines = subprocess.check_output([str(nm), '-S', str(obj)], text=True).splitlines()
    line = next(line for line in lines if line.endswith(' ink_pcm_mutex_request_bytes'))
    print('StaticSemaphore_t / Queue_t request bytes:', int(line.split()[1], 16))
