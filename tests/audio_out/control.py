"""Relink one temporary archive: point-sampling reverse control, or manual git baseline."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', required=True, type=Path)
p.add_argument('--baseline', help='manual mode only; never required by ctest')
a = p.parse_args()
build = a.build.resolve()
db = json.loads((build / 'compile_commands.json').read_text())
with tempfile.TemporaryDirectory(prefix='audio-out-control-', dir=build) as tmp:
    folder = Path(tmp)
    archive = folder / 'libink_system.a'
    shutil.copyfile(build / 'libink_system.a', archive)
    files = ['audio_service.c', 'media_service.c', 'codec_formats.c'] if a.baseline else ['audio_service.c']
    for name in files:
        source = ROOT / 'system/services/audio' / name
        if a.baseline:
            text = subprocess.check_output(['git', 'show', f'{a.baseline}:{source.relative_to(ROOT)}'], cwd=ROOT, text=True)
        else:
            text = source.read_text()
            old = 'int err = speex_resampler_process_int(AU.resampler, 0, (const int16_t *)f->data, &in, AU.pcm, &out);'
            assert old in text
            text = text.replace(old, '''out = (spx_uint32_t)((uint64_t)in * HAL_AUDIO_RATE / f->rate);
    for (spx_uint32_t i = 0; i < out; i++) AU.pcm[i] = ((const int16_t *)f->data)[(uint64_t)i * f->rate / HAL_AUDIO_RATE];
    int err = 0;''', 1)
        copy = folder / name
        copy.write_text(text)
        obj = folder / (name + '.o')
        entry = next(e for e in db if Path(e['file']) == source and 'ink_system.dir' in e['command'])
        cmd = shlex.split(entry['command'])
        cmd[cmd.index('-o') + 1] = str(obj)
        cmd[cmd.index(str(source))] = str(copy)
        cmd += ['-iquote', str(source.parent)]
        subprocess.run(cmd, cwd=build, check=True)
        subprocess.run(['ar', 'r', str(archive), str(obj)], check=True)
    link = shlex.split((build / 'CMakeFiles/sim-audio-out.dir/link.txt').read_text())
    link = [str(archive) if Path(x).name == 'libink_system.a' else x for x in link]
    binary = build / ('audio-out-baseline' if a.baseline else 'audio-out-point-sampling')
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    cmd = [sys.executable, '-B', str(ROOT / 'tests/audio_out/run.py'), '--build', str(build), '--binary', str(binary)]
    cmd += ['--no-thresholds'] if a.baseline else ['--case', 'tone10k']
    result = subprocess.run(cmd, capture_output=True, text=True, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
    print(result.stdout)
    if a.baseline:
        assert result.returncode == 0, result.stderr[-12000:]
    else:
        assert result.returncode != 0 and 'attenuation_db' in result.stderr, result.stdout + result.stderr[-12000:]
        print('point-sampling reverse: the 40 dB attenuation assertion failed as required')
        if not os.environ.get('INK_KEEP_EVIDENCE'):
            shutil.rmtree(build / 'audio-out-evidence' / binary.name)
            binary.unlink()
