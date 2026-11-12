import asyncio
from contextlib import aclosing

import pytest

from ink_hub.gateway.wire import frame
from ink_hub.models.fake import FakeModel, ModelResult
from ink_hub.voice.stream import split_speech, heard_text
from ink_hub.gateway.server import Session
from tests.fake_device import FakeDevice
from tests.speech.cloud_fixture import Vendors, ANSWER, PCM, TEXT


@pytest.fixture
async def vendors():
    server = await Vendors().start()
    yield server
    await server.close()


async def start_voice(device, frames, end=True):
    await device.call("rt.open", {"sid": 7, "cap": "voice.chat", "params": {}})
    rid = await device.send("ai.listen", {"stream": 7, "page": {"actions": []}})
    # Answer tools/list before sending PCM; no polling or arbitrary delay.
    await device.answer(await device.recv())
    for seq, packet in enumerate(frames):
        await device.ws.send_bytes(frame(1, 7, seq, packet))
    if end:
        await device.send("ai.listen.end", {"stream": 7}, rid=False)
    return rid


def encode(opus, count, quiet=0):
    with opus.encoder() as encoder:
        return [opus.encode(encoder, PCM) for _ in range(count)] + [opus.encode(encoder, bytes(1920)) for _ in range(quiet)]


@pytest.mark.parametrize("name", ["doubao", "aliyun"])
async def test_vendor_complete_flow_and_vad(make_hub, vendors, opus, name):
    asr, tts = vendors.providers(name, opus)
    hub, client, url = await make_hub(asr=asr, tts=tts, model=FakeModel(text=ANSWER))
    device = FakeDevice(client, url)
    await device.connect()
    rid = await start_voice(device, encode(opus, 6, quiet=18), end=False)
    # No listen.end: server VAD must finalize this turn.
    assert (await device.until(rid))["result"]["done"]
    session = hub.connections["example-device"].sessions[7]
    times = session.times
    assert times["speech_end"] <= times["asr_final"] <= times["model_first_text"] <= times["first_sentence"] <= times["first_audio"]
    assert vendors.texts == [ANSWER[:ANSWER.index("，") + 1], ANSWER[ANSWER.index("，") + 1:]]
    assert any(isinstance(m, bytes) for m in device.messages)
    assert next(m for m in device.messages if isinstance(m, dict) and m.get("method") == "ai.tts.end")["params"]["frames"] == 2 * vendors.frames


@pytest.mark.parametrize("name", ["doubao", "aliyun"])
@pytest.mark.parametrize("interim", [0, 800])
async def test_dictation_interim_throttle(make_hub, vendors, opus, name, interim):
    asr, _ = vendors.providers(name, opus)
    hub, client, url = await make_hub(asr=asr)
    device = FakeDevice(client, url)
    await device.connect()
    await device.call("rt.open", {"sid": 9, "cap": "asr.stream", "params": {"interim_ms": interim}})
    for seq, packet in enumerate(encode(opus, 6)):
        await device.ws.send_bytes(frame(1, 9, seq, packet))
    await device.send("rt.turn", {"sid": 9, "op": "end"}, rid=False)
    values = []
    while True:
        value = await device.recv()
        if value.get("method") == "rt.asr.done":
            break
        values.append(value["params"])
    assert len([v for v in values if not v["final"]]) == (1 if interim else 0)
    assert [v["text"] for v in values if v["final"]] == [TEXT]
    assert hub.connections["example-device"].sessions[9].times["asr_final"]


@pytest.mark.parametrize("name", ["doubao", "aliyun"])
async def test_first_packet_before_vendor_finishes_and_abort(make_hub, vendors, opus, name):
    vendors.gate = asyncio.Event()
    asr, tts = vendors.providers(name, opus)
    hub, client, url = await make_hub(asr=asr, tts=tts, model=FakeModel(text=ANSWER))
    device = FakeDevice(client, url)
    await device.connect()
    rid = await start_voice(device, encode(opus, 5))
    while not isinstance(await device.recv(), bytes):
        pass
    connection = hub.connections["example-device"]
    task = connection.turns[rid]
    assert not vendors.gate.is_set(), "首包必须在厂商完成整句前发出"
    loop = asyncio.get_running_loop()
    before = loop.time()
    await device.send("rt.close", {"sid": 7, "reason": "barge_in", "heard_frames": 0}, rid=False)
    await device.call("caps/list")  # Confirms the gateway processed the abort.
    assert task.done() and loop.time() - before < .1
    await asyncio.wait_for(vendors.closed.wait(), .1)
    assert connection.voice_history == (TEXT, "")
    vendors.gate.set()
    await device.call("caps/list")
    # The first packet is the only packet in the connection, even after provider release.
    assert len([m for m in device.messages if isinstance(m, bytes)]) == 1


@pytest.mark.parametrize("stage", ["asr", "model", "tts"])
async def test_cancel_all_stages_within_100ms(make_hub, stage):
    entered, cancelled = asyncio.Event(), asyncio.Event()
    class Block:
        async def stream(self, *args, **kwargs):
            entered.set()
            try:
                await asyncio.Event().wait()
                yield b"never" if stage == "tts" else ModelResult("never")
            finally:
                cancelled.set()
        async def transcribe(self, frames):
            entered.set()
            try:
                await asyncio.Event().wait()
            finally:
                cancelled.set()
    block = Block()
    if stage == "asr":
        # Legacy ASR is also cancellable; streaming vendors are exercised above.
        class ASR:
            transcribe = block.transcribe
        block = ASR()
    hub, client, url = await make_hub(**{stage: block} if stage != "model" else {"model": block})
    device = FakeDevice(client, url)
    await device.connect()
    rid = await start_voice(device, [b"synthetic-audio"])
    await asyncio.wait_for(entered.wait(), 1)
    connection = hub.connections["example-device"]
    task = connection.turns[rid]
    try:
        before = asyncio.get_running_loop().time()
        await device.send("rt.close", {"sid": 7}, rid=False)
        await device.call("caps/list")
        assert cancelled.is_set() and task.done()
        assert asyncio.get_running_loop().time() - before < .1
        assert not [m for m in device.messages if isinstance(m, bytes)]
    finally:
        task.cancel()
        await asyncio.gather(task, return_exceptions=True)



def test_split_and_history():
    assert list(split_speech("太短，12345678，后面。末尾")) == ["太短，12345678，", "后面。"]
    assert list(split_speech("12345678，尾巴", first=False, final=True)) == ["12345678，尾巴"]
    session = Session("voice.chat", 0, sent_frames=20, spoken=[["一二三四五", 0, 5], ["甲乙丙丁戊己", 5, None]])
    session.heard_frames = 7
    assert heard_text(session) == "一二三四五"
    session.heard_frames = None
    assert heard_text(session) == "一二三四五甲乙丙"


async def test_first_clause_is_spoken_while_model_still_streams(make_hub, vendors, opus):
    release = asyncio.Event()
    class Model:
        async def stream(self, *args, **kwargs):
            yield ModelResult("这是一句超过八个字的回答，")
            await release.wait()
            yield ModelResult("这是后半句。")
    asr, tts = vendors.providers("doubao", opus)
    _, client, url = await make_hub(asr=asr, tts=tts, model=Model())
    device = FakeDevice(client, url)
    await device.connect()
    rid = await start_voice(device, encode(opus, 5))
    while not isinstance(await device.recv(), bytes):
        pass
    assert not release.is_set() and vendors.texts == ["这是一句超过八个字的回答，"]
    release.set()
    assert (await device.until(rid))["result"]["done"]


async def test_interim_interval_allows_next_update_and_final_bypasses_it(monkeypatch):
    from types import SimpleNamespace
    from ink_hub.voice import stream
    clock = SimpleNamespace(now=0.)
    monkeypatch.setattr(stream, "asyncio", SimpleNamespace(get_running_loop=lambda: SimpleNamespace(time=lambda: clock.now)))
    class ASR:
        async def stream(self, audio):
            for now, text, final in ((0., "甲", False), (.799, "乙", False), (.8, "丙", False), (.801, "丁", True)):
                clock.now = now
                yield text, final
    sent = []
    async def notify(method, params, guard):
        sent.append(params)
    connection = SimpleNamespace(hub=SimpleNamespace(asr=ASR()), scope="synthetic", notify=notify,
                                 valid_output=lambda *args: True)
    session = Session("asr.stream", 0, interim_ms=800)
    assert await stream.recognize(connection, 9, session) == ("丁", 1)
    assert [value["text"] for value in sent] == ["甲", "丙", "丁"]
    assert sent[-1]["final"]
