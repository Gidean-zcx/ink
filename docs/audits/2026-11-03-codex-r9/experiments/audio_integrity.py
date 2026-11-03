#!/usr/bin/env python3
"""Compare the actual emitted PCM, not just playback counters."""
import hashlib
import json
import wave
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
names = ["r9_concurrent_control-pqbn", "r9_concurrent-pqbn", "r9_concurrent-pqbn-before",
         "r9_concurrent", "r9_concurrent-before", "r9_concurrent-pqbn-asan"]
rows = []
reference = None
for name in names:
    folder = ROOT / "build-audit/r9-probes" / name
    with wave.open(str(folder / "speaker.wav"), "rb") as w:
        params = (w.getnchannels(), w.getsampwidth(), w.getframerate())
        count = w.getnframes()
        pcm = w.readframes(count)
    assert params == (1, 2, 16000) and count == 352000
    if reference is None:
        reference = pcm
    metrics = json.loads((folder / "metrics.json").read_text())["metrics"]
    assert metrics["audio.speaker.played_ms"] == 18000
    assert metrics["audio.speaker.underruns"] == 0
    assert pcm == reference, name
    rows.append(dict(name=name, params=params, samples=count, played_ms=18000, underruns=0,
                     sha256=hashlib.sha256(pcm).hexdigest(), equal_to_control=True))
result = dict(note="模拟 codec PCM；288000 个语音样本及流间两段各 32000 个静音样本。HAL 将每次关输出后的间隔最多记录为 2 s（hal_audio_sim.c:108–129）。未验证真实声学或 Opus。",
              runs=rows)
(HERE / "audio-integrity.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print("6 WAVs: all 352000 samples identical; 18000 ms played, zero underruns each")
