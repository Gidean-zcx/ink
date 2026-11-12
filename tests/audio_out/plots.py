"""Manual export of spectra and numeric evidence; requires numpy and matplotlib."""
import argparse
import json
import shutil
from pathlib import Path
import wave
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.font_manager import FontProperties

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--font', type=Path, required=True)
p.add_argument('--before', default='audio-out-baseline-real')
p.add_argument('--before-title', default='改前：抽点与真实 Opus 重编码')
a = p.parse_args()
a.out.mkdir(parents=True, exist_ok=True)
font = FontProperties(fname=a.font)
measurements = {}
for name, binary, title in [('before', a.before, a.before_title),
                            ('after', 'sim-audio-out', '改后：生产侧 SpeexDSP Q4 与 PCM 输出')]:
    area = a.build / 'audio-out-evidence' / binary
    shutil.copyfile(area / 'music/out/speaker.wav', a.out / (name + '-music.wav'))
    measurements[name] = json.loads((area / 'quality.json').read_text())
    with wave.open(str(area / 'music/out/speaker.wav')) as wav:
        assert wav.getframerate() == 16000 and wav.getnchannels() == 1
        pcm = np.frombuffer(wav.readframes(wav.getnframes()), dtype='<i2').astype(float)
    start = np.flatnonzero(abs(pcm) > 10)[0] + 8000
    block = pcm[start:start + 16000]
    window = np.hanning(len(block))
    spectrum = 20 * np.log10(np.maximum(abs(np.fft.rfft(block * window)) * 2 / window.sum() / 32768, 1e-6))
    fig, ax = plt.subplots(figsize=(8, 3.4), dpi=125)
    ax.plot(np.fft.rfftfreq(len(block), 1 / 16000), spectrum, color='#245c91', linewidth=0.8)
    ax.set(xlim=(0, 8000), ylim=(-120, 0))
    ax.set_xlabel('频率（Hz）', fontproperties=font)
    ax.set_ylabel('幅度（dBFS）', fontproperties=font)
    ax.set_title(title, fontproperties=font)
    ax.grid(alpha=0.2)
    fig.tight_layout()
    target = a.out / (name + '-spectrum.png')
    fig.savefig(target)
    plt.close(fig)
    assert target.stat().st_size < 300000
metrics = json.loads((a.build / 'audio-out-evidence/sim-audio-out/scenario/out/metrics.json').read_text())['metrics']
measurements['scenario'] = {k: v for k, v in metrics.items() if k.startswith(('test.wav.', 'test.pass', 'test.fail', 'test.finished', 'audio.speaker.underrun'))}
measurements['boundary'] = '同一合成双声道音乐的 speaker.wav；改前通路由命令参数指定。模拟器计时不是实机。'
(a.out / 'measurements.json').write_text(json.dumps(measurements, indent=2, ensure_ascii=False) + '\n')
