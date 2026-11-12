import json
import math
import os
import struct
import subprocess
import sys
import asyncio
import io
import wave
from pathlib import Path
from types import SimpleNamespace

import pytest
from aiohttp import web

from ink_hub.speech import local
from ink_hub.speech.opus import Opus


@pytest.fixture(scope="session")
def opus():
    root = Path(__file__).resolve().parents[3]
    suffix = "dylib" if sys.platform == "darwin" else "so"
    library = Path(os.environ.get("INK_HUB_LIBOPUS", root / "work" / "hub-opus" / f"libopus.{suffix}"))
    if not library.is_file() and "INK_HUB_LIBOPUS" not in os.environ:
        deps = Path(os.environ.get("INK_DEPS", root))
        if (deps / "build-s31" / "esp-opus-1.0.5.zip").is_file():
            result = subprocess.run(["sh", str(root / "hub/tools/build_opus.sh")],
                                    env={**os.environ, "INK_DEPS": str(deps)}, capture_output=True)
            assert result.returncode == 0, result.stderr.decode()[-2000:]
        else:
            pytest.skip("真 Opus 测试需要 INK_HUB_LIBOPUS 或 INK_DEPS 中的固定版本压缩包；不下载依赖")
    return Opus(library)


@pytest.fixture
async def speech_tools(tmp_path, monkeypatch, opus):
    bins, audio_root = tmp_path / "bin", tmp_path / "temporary-audio"
    bins.mkdir()
    audio_root.mkdir()
    source = Path(__file__).with_name("fake_speech_tool.py").read_text()
    for name in ("say",):
        tool = bins / name
        tool.write_text(f"#!{sys.executable}\n" + source)
        tool.chmod(0o700)
    model, calls = tmp_path / "fake-model.bin", tmp_path / "calls.jsonl"
    model.write_bytes(b"synthetic model fixture, not a Whisper model")
    monkeypatch.setattr(local.tempfile, "tempdir", str(audio_root))
    monkeypatch.setenv("INK_TEST_CALLS", str(calls))
    monkeypatch.setenv("INK_HUB_SAY_VOICE", "Tingting")
    monkeypatch.setenv("PATH", str(bins) + os.pathsep + os.environ.get("PATH", ""))
    with opus.encoder() as encoder:
        pcm = struct.pack("<960h", *(int(12000 * math.sin(2 * math.pi * 330 * n / 16000)) for n in range(960)))
        frames = [opus.encode(encoder, pcm) for _ in range(25)]
    started, cancelled = asyncio.Event(), asyncio.Event()
    async def inference(request):
        data = await request.post()
        assert data["language"] == "zh" and "简体中文" in data["prompt"]
        with wave.open(io.BytesIO(data["file"].file.read()), "rb") as audio:
            assert (audio.getframerate(), audio.getnchannels(), audio.getsampwidth()) == (16000, 1, 2)
            count = audio.getnframes()
        with calls.open("a") as output:
            output.write(json.dumps({"kind": "whisper", "samples": count}) + "\n")
        started.set()
        mode = os.environ.get("INK_TEST_WHISPER_MODE", "ok")
        if mode == "timeout":
            try:
                await asyncio.Event().wait()
            finally:
                cancelled.set()
        if mode in ("fail", "missing", "forbidden"):
            raise web.HTTPServiceUnavailable()
        return web.json_response({"text": "合成测试识别文字"})
    app = web.Application()
    app.router.add_post("/inference", inference)
    runner = web.AppRunner(app, access_log=None, handler_cancellation=True)
    await runner.setup()
    site = web.TCPSite(runner, "127.0.0.1", 0)
    await site.start()
    monkeypatch.setenv("INK_HUB_WHISPER_URL", f"http://127.0.0.1:{site._server.sockets[0].getsockname()[1]}/inference")
    tools = SimpleNamespace(asr=local.WhisperASR(opus=opus), tts=local.SayTTS(opus=opus),
                            opus=opus, frames=frames, bins=bins, model=model, calls=calls,
                            started=started, cancelled=cancelled,
                            events=lambda: [json.loads(line) for line in calls.read_text().splitlines()] if calls.exists() else [])
    yield tools
    await runner.cleanup()
    assert list(audio_root.iterdir()) == [], "语音临时目录未清理"
