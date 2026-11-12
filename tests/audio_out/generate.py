"""Manual synthetic fixture regeneration: numpy 2.0.2, SoundFile 0.13.1/libsndfile 1.2.2."""
import array
import math
from pathlib import Path
import numpy as np
import soundfile as sf

out = Path(__file__).with_name('fixtures')
out.mkdir(exist_ok=True)
t = np.arange(70 * 44100) / 44100
left, right = np.where(t < 60, 440, 880), np.where(t < 60, 660, 1320)
pcm = np.column_stack((12000 * np.sin(2 * np.pi * left * t), 12000 * np.sin(2 * np.pi * right * t))).astype('int16')
sf.write(out / '01.flac', pcm, 44100, subtype='PCM_16')
mono = array.array('h', (int(12000 * math.sin(2 * math.pi * 330 * i / 44100)) for i in range(12 * 44100)))
with sf.SoundFile(str(out / '02.mp3'), 'w', samplerate=44100, channels=1, format='MP3', subtype='MPEG_LAYER_III') as f:
    f.buffer_write(mono.tobytes(), dtype='int16')
