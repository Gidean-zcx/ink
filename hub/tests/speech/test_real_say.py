"""Native smoke test; the default evidence stays inside pytest's directory."""
import math
import os
import shutil
import struct
import sys
import wave
from pathlib import Path

import pytest

from ink_hub.speech import local


@pytest.mark.skipif(sys.platform != "darwin", reason="原生 say 仅在 macOS 可用；其它语音契约用假程序验证")
async def test_real_say(opus, tmp_path, monkeypatch):
    if not shutil.which("say"):
        pytest.skip("这台 Mac 没有 say，不能验证原生朗读")
    source = {}
    original = local.run_cli

    async def measure(args, text=None):
        await original(args, text)
        with wave.open(args[args.index("-o") + 1], "rb") as audio:
            source["samples"] = audio.getnframes()

    monkeypatch.setattr(local, "run_cli", measure)
    packets = await local.SayTTS(voice="Tingting", opus=opus).synthesize("今天天气怎么样？")
    assert packets and len(packets) == math.ceil(source["samples"] / 960)
    with opus.decoder() as decoder:
        pcm = b"".join(opus.decode(decoder, packet) for packet in packets)
    assert max(abs(s) for s in struct.unpack(f"<{len(pcm) // 2}h", pcm)) > 100
    output = Path(os.environ.get("INK_HUB_SAY_EVIDENCE", tmp_path / "say-roundtrip.wav"))
    output.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(output), "wb") as audio:
        audio.setparams((1, 2, 16000, 0, "NONE", "not compressed"))
        audio.writeframes(pcm)
    assert output.stat().st_size <= 300000
    print(f"原生 say: voice=Tingting, 源音频={source['samples']} 采样/{source['samples'] / 16000:.4f} 秒, "
          f"Opus={len(packets)} 帧/{len(packets) * 0.06:.2f} 秒, WAV={output.stat().st_size} 字节")
