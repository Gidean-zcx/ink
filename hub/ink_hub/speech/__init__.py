"""Explicit speech selection; missing settings preserve the gateway's fakes."""
import os

from .fake import FakeASR, FakeTTS


def from_environment():
    asr = os.environ.get("INK_HUB_ASR", "fake")
    tts = os.environ.get("INK_HUB_TTS", "fake")
    if asr not in ("fake", "whisper", "sherpa", "doubao", "aliyun"):
        raise RuntimeError("INK_HUB_ASR 接法无效")
    if tts not in ("fake", "say", "doubao", "aliyun"):
        raise RuntimeError("INK_HUB_TTS 接法无效")
    from .local import SayTTS, WhisperASR
    from .sherpa import SherpaASR
    from .doubao import DoubaoASR, DoubaoTTS
    from .aliyun import AliyunASR, AliyunTTS
    def provider(kind, name):
        if name == "fake":
            return None
        if name in ("whisper", "sherpa", "say"):
            return {"whisper": WhisperASR, "sherpa": SherpaASR, "say": SayTTS}[name]()
        def required(key):
            value = os.environ.get("INK_HUB_" + name.upper() + "_" + key)
            if not value:
                raise RuntimeError("语音凭据不完整：" + name + " " + key)
            return value
        cls = {("asr", "doubao"): DoubaoASR, ("tts", "doubao"): DoubaoTTS,
               ("asr", "aliyun"): AliyunASR, ("tts", "aliyun"): AliyunTTS}[kind, name]
        args = [required("APPID" if name == "doubao" else "APPKEY"), required("TOKEN")]
        if kind == "tts":
            args.append(required("VOICE"))
        return cls(*args)
    return provider("asr", asr), provider("tts", tts)
