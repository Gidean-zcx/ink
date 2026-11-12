import asyncio
import math
import os
import struct

import pytest

from ink_hub.gateway.wire import frame, parse_frame
from ink_hub.models.fake import FakeModel
from ink_hub.speech import from_environment, local
from tests.fake_device import FakeDevice

TRANSCRIPT = "合成测试识别文字"
SPOKEN = "合成测试朗读内容。"


async def voice(device, frames):
    assert (await device.call("rt.open", {"sid": 7, "cap": "voice.chat", "params": {}}))["result"] == {"sid": 7}
    rid = await device.send("ai.listen", {"stream": 7, "format": "opus", "rate": 16000,
                                          "frame_ms": 60, "page": {"actions": []}})
    for seq, packet in enumerate(frames):
        await device.ws.send_bytes(frame(1, 7, seq, packet))
    await device.send("ai.listen.end", {"stream": 7, "frames": len(frames)}, rid=False)
    result = await device.until(rid)
    await device.send("rt.close", {"sid": 7}, rid=False)
    return rid, result


async def test_voice_protocol(make_hub, speech_tools, caplog, capfd):
    tools = speech_tools
    _, client, url = await make_hub(asr=tools.asr, tts=tools.tts, model=FakeModel(text=SPOKEN))
    device = FakeDevice(client, url)
    await device.connect()
    rid, result = await voice(device, tools.frames)
    assert result == {"jsonrpc": "2.0", "id": rid, "result": {"done": True}}
    messages = device.messages
    stt = next(i for i, m in enumerate(messages) if isinstance(m, dict) and m.get("method") == "ai.stt")
    delta = next(i for i, m in enumerate(messages) if isinstance(m, dict) and m.get("method") == "ai.delta")
    end = next(i for i, m in enumerate(messages) if isinstance(m, dict) and m.get("method") == "ai.tts.end")
    audio = [(i, parse_frame(m)) for i, m in enumerate(messages) if isinstance(m, bytes)]
    assert messages[stt]["params"]["text"] == TRANSCRIPT
    assert messages[delta]["params"]["text"] == SPOKEN
    assert stt < delta < min(i for i, _ in audio) <= max(i for i, _ in audio) < end < len(messages) - 1
    assert messages[end]["params"]["frames"] == len(audio) == math.ceil(2111 / 960)
    with tools.opus.decoder() as decoder:
        for seq, (_, (kind, sid, number, ms, flags, packet)) in enumerate(audio):
            assert (kind, sid, number, ms, flags) == (2, 7, seq, seq * 60, 0)
            assert len(tools.opus.decode(decoder, packet)) == 1920
    assert tools.events()[0]["samples"] == 25 * 960
    assert tools.events()[1]["text"] == SPOKEN
    logs = caplog.text + "".join(capfd.readouterr())
    assert TRANSCRIPT not in logs and SPOKEN not in logs


async def test_dictation(make_hub, speech_tools):
    tools = speech_tools
    _, client, url = await make_hub(asr=tools.asr, tts=tools.tts)
    device = FakeDevice(client, url)
    await device.connect()
    await device.call("rt.open", {"sid": 9, "cap": "asr.stream", "params": {"kind": "text"}})
    for seq, packet in enumerate(tools.frames):
        await device.ws.send_bytes(frame(1, 9, seq, packet))
    await device.send("rt.turn", {"sid": 9, "op": "end"}, rid=False)
    text, done = await device.recv(), await device.recv()
    assert text["method"] == "rt.asr" and text["params"] == {"sid": 9, "seg": 0, "final": True, "text": TRANSCRIPT}
    assert done["method"] == "rt.asr.done" and done["params"] == {"sid": 9, "segs": 1, "ms": 1500}
    await device.send("rt.close", {"sid": 9}, rid=False)


@pytest.mark.parametrize("amplitude,count", [(0, 25), (20, 25), (12000, 4), (0, 0)])
async def test_silence_and_short_skip_model(speech_tools, monkeypatch, amplitude, count):
    tools = speech_tools
    monkeypatch.setenv("INK_TEST_WHISPER_MODE", "forbidden")
    pcm = struct.pack("<960h", *(int(amplitude * math.sin(2 * math.pi * 440 * n / 16000)) for n in range(960)))
    with tools.opus.encoder() as encoder:
        frames = [tools.opus.encode(encoder, pcm) for _ in range(count)]
    assert await tools.asr.transcribe(frames) == ""
    assert not tools.calls.exists()


async def test_minimum_duration_is_accepted(speech_tools, monkeypatch):
    monkeypatch.setenv("INK_TEST_INPUT_FRAMES", "5")
    assert await speech_tools.asr.transcribe(speech_tools.frames[:5]) == TRANSCRIPT


@pytest.mark.parametrize("provider", ["whisper", "say"])
@pytest.mark.parametrize("mode", ["timeout", "fail", "missing"])
async def test_failure_keeps_connection(make_hub, speech_tools, monkeypatch, caplog, capfd, provider, mode):
    tools = speech_tools
    monkeypatch.setenv("INK_TEST_" + provider.upper() + "_MODE", mode)
    monkeypatch.setattr(local, "PROCESS_TIMEOUT", 1)
    if mode == "missing":
        (tools.bins / provider).unlink()
    _, client, url = await make_hub(asr=tools.asr, tts=tools.tts, model=FakeModel(text=SPOKEN))
    device = FakeDevice(client, url)
    await device.connect()
    _, result = await voice(device, tools.frames)
    assert result["result"] == {"done": True}
    assert any(isinstance(m, dict) and m.get("method") == "ai.delta"
               and m["params"]["text"] == "这次没有完成，请重试。" for m in device.messages)
    assert "result" in await device.call("caps/list")
    for event in tools.events():
        with pytest.raises(ProcessLookupError):
            os.kill(event["pid"], 0)
    logs = caplog.text + "".join(capfd.readouterr())
    assert TRANSCRIPT not in logs and SPOKEN not in logs


async def test_say_padding_language_and_input(speech_tools, monkeypatch):
    tools = speech_tools
    captured = []
    encode = tools.opus.encode

    def capture(handle, pcm):
        captured.append(pcm)
        return encode(handle, pcm)

    monkeypatch.setattr(tools.opus, "encode", capture)
    assert await tools.tts.synthesize("") == []
    assert not tools.calls.exists()
    monkeypatch.setenv("INK_TEST_VOICE", "Samantha")
    text = "--voice=unsafe $(synthetic) 合成测试"
    assert len(await tools.tts.synthesize(text, lang="en")) == 3
    assert tools.events()[0]["text"] == text
    assert all(len(pcm) == 1920 for pcm in captured)
    assert captured[-1][(2111 % 960) * 2:] == bytes((960 - 2111 % 960) * 2)


async def test_cancellation_cleans_process(speech_tools, monkeypatch):
    tools = speech_tools
    started = asyncio.Event()
    original = asyncio.create_subprocess_exec

    async def spawn(*args, **kwargs):
        process = await original(*args, **kwargs)
        started.process = process
        started.set()
        return process

    monkeypatch.setattr(asyncio, "create_subprocess_exec", spawn)
    monkeypatch.setenv("INK_TEST_WHISPER_MODE", "timeout")
    task = asyncio.create_task(tools.asr.transcribe(tools.frames))
    await asyncio.wait_for(started.wait(), 3)
    task.cancel()
    with pytest.raises(asyncio.CancelledError):
        await task
    assert started.process.returncode is not None


def test_environment_defaults(monkeypatch):
    monkeypatch.delenv("INK_HUB_ASR", raising=False)
    monkeypatch.delenv("INK_HUB_TTS", raising=False)
    assert from_environment() == (None, None)
    monkeypatch.setenv("INK_HUB_ASR", "fake")
    monkeypatch.setenv("INK_HUB_TTS", "fake")
    assert from_environment() == (None, None)


def test_environment_real(speech_tools, monkeypatch):
    monkeypatch.setenv("INK_HUB_ASR", "whisper")
    monkeypatch.setenv("INK_HUB_TTS", "say")
    asr, tts = from_environment()
    assert isinstance(asr, local.WhisperASR) and isinstance(tts, local.SayTTS)


@pytest.mark.parametrize("variable,value,message", [
    ("INK_HUB_ASR", "unknown", "INK_HUB_ASR"), ("INK_HUB_TTS", "unknown", "INK_HUB_TTS"),
    ("INK_HUB_WHISPER_CLI", "missing-synthetic-whisper", "INK_HUB_WHISPER_CLI"),
    ("INK_HUB_WHISPER_MODEL", "missing-synthetic-model", "INK_HUB_WHISPER_MODEL"),
    ("INK_HUB_LIBOPUS", "missing-synthetic-library", "INK_HUB_LIBOPUS"), ("PATH", "", "say"),
])
def test_environment_errors(speech_tools, monkeypatch, variable, value, message):
    monkeypatch.setenv("INK_HUB_ASR", "whisper")
    monkeypatch.setenv("INK_HUB_TTS", "say")
    monkeypatch.setenv(variable, value)
    with pytest.raises(RuntimeError, match=message):
        from_environment()
