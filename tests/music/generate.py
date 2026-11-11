"""Optional fixture regeneration: SoundFile 0.13.1, libsndfile 1.2.2 (not needed by ctest)."""
import array
import math
from pathlib import Path
import soundfile as sf

out = Path(__file__).with_name('fixtures')
out.mkdir(exist_ok=True)

def sync(n):
    return bytes([(n >> 21) & 127, (n >> 14) & 127, (n >> 7) & 127, n & 127])

for rate, channels in [(16000, 1), (44100, 2)]:
    samples = array.array('h', (int(9000 * math.sin(2 * math.pi * (440 + 220 * c) * i / rate))
                              for i in range(rate * 3) for c in range(channels)))
    for fmt, ext, subtype in [('WAV', 'wav', 'PCM_16'), ('FLAC', 'flac', 'PCM_16'),
                              ('OGG', 'ogg', 'VORBIS'), ('MP3', 'mp3', 'MPEG_LAYER_III')]:
        path = out / f'tone-{rate}-{channels}.{ext}'
        with sf.SoundFile(str(path), 'w', samplerate=rate, channels=channels, format=fmt, subtype=subtype) as f:
            f.title, f.artist, f.album = '合成音调', '测试歌手', '原创测试'
            f.buffer_write(samples.tobytes(), dtype='int16')
        with sf.SoundFile(str(path)) as f:
            path.with_name(path.name + '.pcm').write_bytes(bytes(f.buffer_read(f.frames, dtype='int16')))
        if ext == 'mp3':
            version = 3 if rate == 16000 else 4
            frames = b''
            for ident, text in [('TIT2', '合成音调'), ('TPE1', '测试歌手'), ('TALB', '原创测试')]:
                payload = b'\x01\xff\xfe' + text.encode('utf-16le') if version == 3 else b'\x03' + text.encode()
                size = len(payload).to_bytes(4, 'big') if version == 3 else sync(len(payload))
                frames += ident.encode() + size + b'\x00\x00' + payload
            path.write_bytes(b'ID3' + bytes([version, 0, 0]) + sync(len(frames)) + frames + path.read_bytes())
    (out / f'tone-{rate}-{channels}.lrc').write_text('[00:00.00]第一句\n[00:01.00]第二句\n[bad]坏标签\n[00:02.00]第三句\n')
(out / 'bad.mp3').write_bytes(b'ID3\x04\x00\x00\x7f\x7f\x7f\x7fTALB\xff\xff\xff\xff\x00\x00')
print(sf.__version__, sf.__libsndfile_version__)
