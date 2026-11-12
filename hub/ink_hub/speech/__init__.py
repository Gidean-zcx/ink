"""Explicit speech selection; missing settings preserve the gateway's fakes."""
import os

from .fake import FakeASR, FakeTTS


def from_environment():
    asr = os.environ.get("INK_HUB_ASR", "fake")
    tts = os.environ.get("INK_HUB_TTS", "fake")
    if asr not in ("fake", "whisper"):
        raise RuntimeError("INK_HUB_ASR 只能是 fake 或 whisper")
    if tts not in ("fake", "say"):
        raise RuntimeError("INK_HUB_TTS 只能是 fake 或 say")
    from .local import SayTTS, WhisperASR
    return (WhisperASR() if asr == "whisper" else None,
            SayTTS() if tts == "say" else None)
