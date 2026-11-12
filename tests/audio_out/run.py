"""PCM quality and the real media/focus/output path; no history or extra packages needed."""
import argparse
import array
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import wave

ROOT = Path(__file__).resolve().parents[2]


def write_tone(path, frequency, seconds=5):
    with wave.open(str(path), 'wb') as f:
        f.setparams((2, 2, 44100, 0, 'NONE', 'not compressed'))
        pcm = array.array('h', (int(16000 * math.sin(2 * math.pi * frequency * i / 44100))
                               for i in range(44100) for _ in range(2)))
        f.writeframes(pcm.tobytes() * seconds)


def samples(path):
    with wave.open(str(path), 'rb') as f:
        assert f.getparams()[:3] == (1, 2, 16000)
        return array.array('h', f.readframes(f.getnframes()))


def quality(pcm, freq):
    start = next(i for i, x in enumerate(pcm) if abs(x) > 10) + 8000
    block = pcm[start:start + 16000]
    assert len(block) == 16000, 'one full second of steady output'
    mean = sum(block) / len(block)
    power = sum((x - mean) ** 2 for x in block) / len(block)
    sine = [math.sin(2 * math.pi * freq * i / 16000) for i in range(len(block))]
    cosine = [math.cos(2 * math.pi * freq * i / 16000) for i in range(len(block))]
    a = 2 * sum(x * s for x, s in zip(block, sine)) / len(block)
    b = 2 * sum(x * c for x, c in zip(block, cosine)) / len(block)
    residual = sum((x - mean - a * s - b * c) ** 2 for x, s, c in zip(block, sine, cosine)) / len(block)
    return {'snr_db': 10 * math.log10(max((a * a + b * b) / 2, 1e-20) / max(residual, 1e-20)),
            'attenuation_db': 10 * math.log10((16000 ** 2 / 2) / max(power, 1e-20))}


def run(build, binary, case, area):
    folder = area / case
    sd = folder / 'sd/content/audio'
    sd.mkdir(parents=True, exist_ok=True)
    if case == 'scenario':
        for src in (ROOT / 'tests/audio_out/fixtures').iterdir():
            shutil.copyfile(src, sd / src.name)
        write_tone(sd / '03.wav', 1600)
    else:
        write_tone(sd / f'{case}.wav', {'tone1k': 1000, 'tone10k': 10000, 'music': 440}[case])
        if case == 'music':
            shutil.copyfile(ROOT / 'tests/music/fixtures/tone-44100-2.wav', sd / 'music.wav')
    result = subprocess.run([str(binary), '--profile', str(ROOT / 'simulator/profiles/metalio-plus.json'),
        '--assets', str(build / 'assets'), '--content', str(build / 'test-content'), '--out', str(folder / 'out'),
        '--end', '90s', '--flash-image', str(folder / 'user.img'), '--sd-dir', str(folder / 'sd'),
        '--quiet', '--no-random-outages', '--budget', str(ROOT / 'tests/ui_scene.budget.json')],
        capture_output=True, text=True, timeout=120,
        env={**os.environ, 'INK_AUDIO_OUT_MODE': case, 'UBSAN_OPTIONS': 'halt_on_error=1'})
    (folder / 'run.log').write_text(result.stdout + result.stderr)
    data = json.loads((folder / 'out/metrics.json').read_text())
    m = data['metrics']
    print(f'{case}: exit={result.returncode}, finished={m.get("test.finished", 0)}, '
          f'fail={m.get("test.fail", 0)}, underruns={m.get("audio.speaker.underruns", 0)}', flush=True)
    assert result.returncode == 0 and m.get('test.finished') == 1 and not m.get('test.fail') and not data.get('warnings'), result.stderr[-12000:]
    return samples(folder / 'out/speaker.wav')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', required=True, type=Path)
    p.add_argument('--binary', type=Path)
    p.add_argument('--case', choices=['tone1k', 'tone10k', 'scenario', 'music', 'all'], default='all')
    p.add_argument('--no-thresholds', action='store_true', help='manual baseline measurement only')
    a = p.parse_args()
    build = a.build.resolve()
    binary = a.binary.resolve() if a.binary else build / 'sim-audio-out'
    area = build / 'audio-out-evidence' / binary.name
    shutil.rmtree(area, ignore_errors=True)
    cases = ['tone1k', 'tone10k', 'scenario', 'music'] if a.case == 'all' else [a.case]
    measurements = {}
    for case in cases:
        pcm = run(build, binary, case, area)
        if case.startswith('tone'):
            q = quality(pcm, 1000 if case == 'tone1k' else 6000)
            measurements[case] = q
            print(case, json.dumps(q), flush=True)
            if not a.no_thresholds:
                assert q['snr_db'] >= 60 if case == 'tone1k' else q['attenuation_db'] >= 40, q
    (area / 'quality.json').write_text(json.dumps(measurements, indent=2) + '\n')
    if not os.environ.get('INK_KEEP_EVIDENCE'):
        shutil.rmtree(area)


if __name__ == '__main__':
    main()
