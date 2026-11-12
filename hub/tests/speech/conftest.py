import json
import math
import os
import struct
import subprocess
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

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
def speech_tools(tmp_path, monkeypatch, opus):
    bins, audio_root = tmp_path / "bin", tmp_path / "temporary-audio"
    bins.mkdir()
    audio_root.mkdir()
    source = Path(__file__).with_name("fake_speech_tool.py").read_text()
    for name in ("whisper", "say"):
        tool = bins / name
        tool.write_text(f"#!{sys.executable}\n" + source)
        tool.chmod(0o700)
    model, calls = tmp_path / "fake-model.bin", tmp_path / "calls.jsonl"
    model.write_bytes(b"synthetic model fixture, not a Whisper model")
    monkeypatch.setattr(local.tempfile, "tempdir", str(audio_root))
    monkeypatch.setenv("INK_TEST_CALLS", str(calls))
    monkeypatch.setenv("INK_HUB_WHISPER_CLI", str(bins / "whisper"))
    monkeypatch.setenv("INK_HUB_WHISPER_MODEL", str(model))
    monkeypatch.setenv("INK_HUB_SAY_VOICE", "Tingting")
    monkeypatch.setenv("PATH", str(bins) + os.pathsep + os.environ.get("PATH", ""))
    with opus.encoder() as encoder:
        pcm = struct.pack("<960h", *(int(12000 * math.sin(2 * math.pi * 330 * n / 16000)) for n in range(960)))
        frames = [opus.encode(encoder, pcm) for _ in range(25)]
    tools = SimpleNamespace(asr=local.WhisperASR(opus=opus), tts=local.SayTTS(opus=opus),
                            opus=opus, frames=frames, bins=bins, model=model, calls=calls,
                            events=lambda: [json.loads(line) for line in calls.read_text().splitlines()] if calls.exists() else [])
    yield tools
    assert list(audio_root.iterdir()) == [], "语音临时目录未清理"
