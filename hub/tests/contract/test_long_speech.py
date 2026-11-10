import asyncio

import pytest

from ink_hub.gateway.wire import ANSWER_BYTES, ANSWER_CUT, parse_frame
from ink_hub.models.fake import FakeModel
from tests.fake_device import FakeDevice


class RecordingTTS:
    def __init__(self, frames=1):
        self.frames = frames
        self.texts = []

    async def synthesize(self, text):
        self.texts.append(text)
        return [b"synthetic-opus"] * self.frames


async def test_speech_exceeds_450_frames_and_initial_response_budget(make_hub):
    tts = RecordingTTS(751)  # 45.06 s: beyond the hub's old 42 s deadline.
    _, client, url = await make_hub(model=FakeModel(text="合成长朗读。"), tts=tts)
    device = FakeDevice(client, url)
    await device.connect()
    started = asyncio.get_running_loop().time()
    rid, result = await device.voice()
    elapsed = asyncio.get_running_loop().time() - started
    audio = [parse_frame(m) for m in device.messages if isinstance(m, bytes)]
    assert result["result"]["done"] and len(audio) == 751
    assert [m[2] for m in audio] == list(range(751))
    assert [m[3] for m in audio] == [n * 60 for n in range(751)]
    assert elapsed >= (751 - 5) * 0.06 - 0.05
    end = next(m for m in device.messages if isinstance(m, dict) and m.get("method") == "ai.tts.end")
    assert end["params"] == {"id": rid, "stream": 7, "frames": 751}


@pytest.mark.parametrize("text", ["中文🙂" * 4000, "第一句。第二句！Third?\n" + "汉🙂字" * 900],
                         ids=["over-limit", "long-sentences"])
async def test_speech_and_display_keep_identical_utf8_text(make_hub, text):
    tts = RecordingTTS()
    _, client, url = await make_hub(model=FakeModel(text=text), tts=tts)
    device = FakeDevice(client, url)
    await device.connect()
    await device.voice()
    shown = "".join(m["params"]["text"] for m in device.messages
                    if isinstance(m, dict) and m.get("method") == "ai.delta")
    assert "".join(tts.texts) == shown
    assert len(shown.encode()) <= ANSWER_BYTES
    assert all(0 < len(s.encode()) <= 511 for s in tts.texts)
    if len(text.encode()) > ANSWER_BYTES:
        assert shown.endswith(ANSWER_CUT)
        assert text.startswith(shown[:-len(ANSWER_CUT)])
    else:
        assert shown == text
        assert tts.texts[:4] == ["第一句。", "第二句！", "Third?", "\n"]


async def test_next_sentence_is_synthesized_after_previous_audio_was_sent(make_hub):
    tts = RecordingTTS()
    hub, client, url = await make_hub(model=FakeModel(text="第一句。第二句！"), tts=tts)
    device = FakeDevice(client, url)
    await device.connect()
    connection = hub.connections["example-device"]
    sent = []
    original_send = connection.send

    async def send(message, **kwargs):
        await original_send(message, **kwargs)
        if isinstance(message, bytes):
            sent.append(message)

    connection.send = send
    original = tts.synthesize

    async def synthesize(text):
        if tts.texts:
            assert len(sent) == 1
        return await original(text)

    tts.synthesize = synthesize
    await device.voice()
    assert tts.texts == ["第一句。", "第二句！"]


async def test_slow_sentence_does_not_catch_up_in_a_burst(make_hub):
    tts = RecordingTTS(20)
    hub, client, url = await make_hub(model=FakeModel(text="第一句。第二句！"), tts=tts)
    device = FakeDevice(client, url)
    await device.connect()
    connection = hub.connections["example-device"]
    original_send, original_synthesize = connection.send, tts.synthesize
    sent = []
    loop = asyncio.get_running_loop()

    async def send(message, **kwargs):
        await original_send(message, **kwargs)
        if isinstance(message, bytes):
            sent.append(loop.time())

    async def synthesize(text):
        if tts.texts:
            ready = asyncio.Event()
            timer = loop.call_later(0.6, ready.set)  # Vendor delay, not a wait for an expected result.
            try:
                await ready.wait()
            finally:
                timer.cancel()
        return await original_synthesize(text)

    connection.send, tts.synthesize = send, synthesize
    await device.voice()
    assert len(sent) == 40
    assert sent[-1] - sent[20] >= (20 - 5) * 0.06 - 0.03
