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
p.add_argument('--opus-build', type=Path, help='manual baseline: real fixed-point codec from the S31 host build')
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
            old = 'int err = speex_resampler_process_int(AU.resampler, 0, f->data ? (const int16_t *)f->data + AU.source_off : NULL, &in, AU.pcm, &out);'
            assert old in text
            text = text.replace(old, '''out = (spx_uint32_t)((uint64_t)in * HAL_AUDIO_RATE / f->rate);
for (spx_uint32_t i = 0; i < out; i++) AU.pcm[i] = ((const int16_t *)f->data)[(uint64_t)i * f->rate / HAL_AUDIO_RATE];
int err = 0;''', 1)
            old = 'osal_cost_cycles((uint64_t)out * 3200 * (f->rate > 44100 ? f->rate : 44100) / 44100);'
            assert old in text
            text = text.replace(old, 'osal_cost_cycles((uint64_t)out * 6);', 1)
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
    if a.opus_build:
        assert a.baseline, '--opus-build is only for the manual baseline'
        opus = a.opus_build.resolve()
        source = ROOT / 'ports/sim/hal/hal_codec_sim.c'
        entry = next(e for e in db if Path(e['file']) == source and 'ink_port_sim.dir' in e['command'])
        cmd = shlex.split(entry['command'])
        # Keep the simulator's fake packet generator for unrelated fixture services.
        obj = folder / 'hal_codec_sim.c.o'
        cmd[cmd.index('-o') + 1] = str(obj)
        cmd += ['-Dhal_opus_' + name + '=ink_unused_opus_' + name for name in
                ['enc_new', 'enc_free', 'encode', 'dec_new', 'dec_free', 'decode']]
        subprocess.run(cmd, cwd=build, check=True)
        port = folder / 'libink_port_sim.a'
        shutil.copyfile(build / port.name, port)
        subprocess.run(['ar', 'r', str(port), str(obj)], check=True)
        link = [str(port) if Path(x).name == port.name else x for x in link]
        real = folder / 'hal_opus_s31.c.o'
        cmd = shlex.split(entry['command'])
        cmd[cmd.index('-o') + 1] = str(real)
        cmd[cmd.index(str(source))] = str(ROOT / 'ports/esp32s31/hal_opus_s31.c')
        cmd += ['-I' + str(opus / 'esp-opus/include')]
        subprocess.run(cmd, cwd=build, check=True)
        link += [str(real), str(opus / 'opus-build/libs31-opus.a'), '-lm']
    binary = build / ('audio-out-baseline-real' if a.opus_build else 'audio-out-baseline' if a.baseline else 'audio-out-point-sampling')
    link[link.index('-o') + 1] = str(binary)
    subprocess.run(link, cwd=build, check=True)
    if a.baseline:
        from run import run, samples, quality
        area = build / 'audio-out-evidence' / binary.name
        shutil.rmtree(area, ignore_errors=True)
        measurements = {}
        for case in ['tone1k', 'tone10k', 'music']:
            try:
                pcm = run(build, binary, case, area)
            except AssertionError:
                data = json.loads((area / case / 'out/metrics.json').read_text())
                warnings = data.get('warnings', [])
                # Preserve the real-codec baseline's failure; never relax the ctest budget.
                assert a.opus_build and data['metrics'].get('test.finished') == 1 and not data['metrics'].get('test.fail')
                assert len(warnings) == 1 and "task 'exec.media' may overflow its 8192-byte stack" in warnings[0], warnings
                print('BASELINE BUDGET FAILURE (comparison audio only):', warnings[0])
                pcm = samples(area / case / 'out/speaker.wav')
            if case.startswith('tone'):
                measurements[case] = quality(pcm, 1000 if case == 'tone1k' else 6000)
                print(case, json.dumps(measurements[case]))
        (area / 'quality.json').write_text(json.dumps(measurements, indent=2) + '\n')
    else:
        cmd = [sys.executable, '-B', str(ROOT / 'tests/audio_out/run.py'), '--build', str(build), '--binary', str(binary), '--case', 'tone10k']
        result = subprocess.run(cmd, capture_output=True, text=True, env={**os.environ, 'INK_KEEP_EVIDENCE': '1'})
        print(result.stdout)
        expected = 'attenuation_db'
        assert result.returncode != 0 and expected in result.stderr, result.stdout + result.stderr[-12000:]
        print('reverse control failed at the required assertion:', expected)
        if not os.environ.get('INK_KEEP_EVIDENCE'):
            shutil.rmtree(build / 'audio-out-evidence' / binary.name)
            binary.unlink()
