import asyncio

import pytest

from ink_hub.gateway.wire import ANSWER_BYTES, ANSWER_CUT, frame, parse_frame
from ink_hub.models.fake import FakeModel
from ink_hub.voice.stream import FRAME, LEAD
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
    assert elapsed >= (751 - 1 - round(LEAD / FRAME)) * FRAME - 0.05
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


async def test_next_sentence_is_fetched_while_the_previous_one_is_sent(make_hub):
    # Every sentence's speech takes 0.3 s to start (the speech service's first packet). The next sentence's is asked for
    # while this one is still being sent, so the boundary has no gap: its first frame follows at the 1x pace.
    tts = RecordingTTS(40)  # 2.4 s each: more than the lead, so the first sentence is still being paced out
    hub, client, url = await make_hub(model=FakeModel(text="第一句。第二句！"), tts=tts)
    device = FakeDevice(client, url)
    await device.connect()
    connection = hub.connections["example-device"]
    loop = asyncio.get_running_loop()
    sent, asked = [], []
    original_send, original_synthesize = connection.send, tts.synthesize

    async def send(message, **kwargs):
        await original_send(message, **kwargs)
        if isinstance(message, bytes):
            sent.append(loop.time())

    async def synthesize(text):
        asked.append(len(sent))
        ready = asyncio.Event()
        timer = loop.call_later(0.3, ready.set)  # Vendor delay, not a wait for an expected result.
        try:
            await ready.wait()
        finally:
            timer.cancel()
        return await original_synthesize(text)

    connection.send, tts.synthesize = send, synthesize
    rid, result = await device.voice()
    assert result["result"]["done"] and tts.texts == ["第一句。", "第二句！"] and len(sent) == 80
    assert asked[0] == 0 and asked[1] < 40, "the second sentence is asked for while the first one is sent"
    assert sent[40] - sent[39] < FRAME + 0.1, "no gap where the sentences meet"
    audio = [parse_frame(m) for m in device.messages if isinstance(m, bytes)]
    assert [m[2] for m in audio] == list(range(80))


async def test_abort_stops_the_sentence_fetched_ahead(make_hub):
    entered, closed = asyncio.Event(), asyncio.Event()

    class TTS:
        async def stream(self, text):
            if text == "第一句。":
                for _ in range(40):
                    yield b"synthetic-opus"
                return
            entered.set()
            try:
                await asyncio.Event().wait()
                yield b"never"
            finally:
                closed.set()

    hub, client, url = await make_hub(model=FakeModel(text="第一句。第二句！"), tts=TTS())
    device = FakeDevice(client, url)
    await device.connect()
    await device.call("rt.open", {"sid": 7, "cap": "voice.chat", "params": {}})
    rid = await device.send("ai.listen", {"stream": 7, "page": {"actions": []}})
    await device.answer(await device.recv())
    await device.ws.send_bytes(frame(1, 7, 0, b"synthetic-audio"))
    await device.send("ai.listen.end", {"stream": 7}, rid=False)
    while not isinstance(await device.recv(), bytes):
        pass
    await asyncio.wait_for(entered.wait(), 1)  # the second sentence is being fetched while the first plays
    connection = hub.connections["example-device"]
    task = connection.turns[rid]
    await device.send("rt.close", {"sid": 7, "reason": "barge_in", "heard_frames": 3}, rid=False)
    await device.call("caps/list")
    assert task.done() and closed.is_set(), "the abort stops the fetch ahead too"
    sent = len([m for m in device.messages if isinstance(m, bytes)])
    await device.call("caps/list")
    assert len([m for m in device.messages if isinstance(m, bytes)]) == sent < 40


async def test_heard_frames_cut_history_where_the_device_was(make_hub):
    # The hub is up to the lead ahead of the device: what was heard is the device's count, not what was sent.
    class TTS:
        async def stream(self, text):
            for _ in range(40):
                yield b"synthetic-opus"

    hub, client, url = await make_hub(model=FakeModel(text="一二三四五六七八九十。"), tts=TTS())
    device = FakeDevice(client, url)
    await device.connect()
    await device.call("rt.open", {"sid": 7, "cap": "voice.chat", "params": {}})
    rid = await device.send("ai.listen", {"stream": 7, "page": {"actions": []}})
    await device.answer(await device.recv())
    await device.ws.send_bytes(frame(1, 7, 0, b"synthetic-audio"))
    await device.send("ai.listen.end", {"stream": 7}, rid=False)
    connection = hub.connections["example-device"]
    session = connection.sessions[7]
    while session.sent_frames < round(LEAD / FRAME):
        await device.recv()
    await device.send("rt.close", {"sid": 7, "reason": "barge_in", "heard_frames": 10}, rid=False)
    await device.call("caps/list")
    assert connection.turns.get(rid) is None or connection.turns[rid].done()
    assert session.sent_frames >= round(LEAD / FRAME)
    assert connection.voice_history[1] == "一二三", "10 frames of 0.06 s at 5 characters a second"


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
    assert sent[-1] - sent[20] >= (20 - 1 - round(0.6 / FRAME)) * FRAME - 0.03


async def test_lead_is_sent_at_once_then_real_time(make_hub):
    # 1.2 s (20 frames) beyond the first frame go at once and fill the device's jitter buffer; the rest at 1x.
    tts = RecordingTTS(60)
    hub, client, url = await make_hub(model=FakeModel(text="一句。"), tts=tts)
    device = FakeDevice(client, url)
    await device.connect()
    connection = hub.connections["example-device"]
    loop = asyncio.get_running_loop()
    sent, original_send = [], connection.send

    async def send(message, **kwargs):
        await original_send(message, **kwargs)
        if isinstance(message, bytes):
            sent.append(loop.time())

    connection.send = send
    await device.voice()
    assert len(sent) == 60 and LEAD == 1.2
    assert sent[20] - sent[0] < 0.1, "the lead goes out at once"
    assert sent[59] - sent[20] >= 39 * FRAME - 0.05, "then real time"
