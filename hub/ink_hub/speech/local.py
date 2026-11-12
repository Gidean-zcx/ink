"""Local speech tools; audio and text exist only in private temporary files."""
import asyncio
import math
import os
import shutil
import struct
import tempfile
import wave
from pathlib import Path

from .opus import FRAME_BYTES, RATE, Opus

MIN_SAMPLES = RATE * 3 // 10
SILENCE_RMS = 100  # About -50 dBFS; tune against the device microphone.
PROCESS_TIMEOUT = 20


def executable(value, variable):
    path = shutil.which(value) if value else None
    if not path:
        raise RuntimeError(f"找不到语音程序，请检查 {variable}")
    return path


async def run_cli(args, text=None):
    process = None
    try:
        process = await asyncio.create_subprocess_exec(
            *args, stdin=asyncio.subprocess.PIPE if text is not None else asyncio.subprocess.DEVNULL,
            stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
        await asyncio.wait_for(process.communicate(text), PROCESS_TIMEOUT)
        if process.returncode:
            raise RuntimeError("本机语音程序执行失败")
    except (OSError, TimeoutError):
        raise RuntimeError("本机语音程序不可用或已超时，请重试") from None
    finally:
        if process is not None and process.returncode is None:
            process.kill()
            await process.wait()


class WhisperASR:
    def __init__(self, cli=None, model=None, opus=None):
        self.cli = executable(cli or os.environ.get("INK_HUB_WHISPER_CLI"), "INK_HUB_WHISPER_CLI")
        self.model = Path(model or os.environ.get("INK_HUB_WHISPER_MODEL") or "")
        if not self.model.is_file() or not os.access(self.model, os.R_OK):
            raise RuntimeError("找不到识别模型，请检查 INK_HUB_WHISPER_MODEL（不会自动下载）")
        self.opus = opus or Opus()

    async def transcribe(self, frames):
        with self.opus.decoder() as decoder:
            pcm = b"".join(self.opus.decode(decoder, packet) for packet in frames)
        samples = struct.unpack(f"<{len(pcm) // 2}h", pcm)
        if len(samples) < MIN_SAMPLES or math.sqrt(sum(s * s for s in samples) / len(samples)) < SILENCE_RMS:
            return ""
        try:
            with tempfile.TemporaryDirectory(prefix="ink-hub-asr-") as directory:
                wav, output = Path(directory) / "audio.wav", Path(directory) / "result"
                with wave.open(str(wav), "wb") as audio:
                    audio.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
                    audio.writeframes(pcm)
                await run_cli([self.cli, "-m", str(self.model), "-f", str(wav), "-l", "zh",
                               "--prompt", "以下是简体中文的普通话。", "-nt", "-np", "-otxt", "-of", str(output)])
                return output.with_suffix(".txt").read_text(encoding="utf-8").strip()
        except (OSError, UnicodeError, wave.Error):
            raise RuntimeError("无法读取本机识别结果，请重试") from None


class SayTTS:
    def __init__(self, cli="say", voice=None, opus=None):
        self.cli = executable(cli, "PATH（macOS 的 say）")
        self.voice = voice or os.environ.get("INK_HUB_SAY_VOICE") or "Tingting"
        self.opus = opus or Opus()

    async def synthesize(self, text, lang="zh"):
        if not text:
            return []
        voice = "Samantha" if lang == "en" else self.voice
        try:
            with tempfile.TemporaryDirectory(prefix="ink-hub-tts-") as directory:
                wav = Path(directory) / "audio.wav"
                await run_cli([self.cli, "-v", voice, "--file-format=WAVE", "--data-format=LEI16@16000",
                               "--channels=1", "-o", str(wav)], text.encode("utf-8"))
                with wave.open(str(wav), "rb") as audio:
                    if (audio.getnchannels(), audio.getsampwidth(), audio.getframerate()) != (1, 2, RATE):
                        raise RuntimeError("朗读程序没有输出 16 kHz 单声道 PCM")
                    pcm = audio.readframes(audio.getnframes())
                with self.opus.encoder() as encoder:
                    return [self.opus.encode(encoder, pcm[i:i + FRAME_BYTES].ljust(FRAME_BYTES, b"\0"))
                            for i in range(0, len(pcm), FRAME_BYTES)]
        except (OSError, EOFError, wave.Error):
            raise RuntimeError("无法读取本机朗读结果，请重试") from None
