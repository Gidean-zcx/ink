import struct
from typing import Protocol


class Recognizer(Protocol):
    async def transcribe(self, frames: list[bytes]) -> str: ...


class Speaker(Protocol):
    async def synthesize(self, text: str) -> list[bytes]: ...


class FakeASR:
    def __init__(self, text="翻到下一页"):
        self.text = text

    async def transcribe(self, frames):
        return self.text if frames else ""


class FakeTTS:
    async def synthesize(self, text):
        # Same magic, level and pitch as hal_codec_sim.c; not Opus.
        return [struct.pack("<BHH", 0xA5, 5000, 1800) + bytes([i]) * 175 for i in range(5)] if text else []
