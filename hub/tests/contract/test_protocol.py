import asyncio
import json

import pytest
from aiohttp import WSMsgType

from ink_hub.bus.registry import Capability
from ink_hub.gateway.wire import ANSWER_BYTES, ANSWER_CUT, ProtocolError, chunks, encode, frame, page, response, tokens
from ink_hub.models.fake import FakeModel
from ink_hub.speech.fake import FakeASR
from tests.fake_device import FakeDevice, lexical_tokens

TOOL = {"name": "reader.next_page", "title": "下一页", "effect": "navigate", "confirm": "none",
        "inputSchema": {"type": "object", "properties": {}}}


@pytest.mark.parametrize("value", [None, True, 3, "引号\"与\\转义", {}, [], {"x": [1, {"y": "中文"}]}])
def test_token_count_independent(value):
    assert lexical_tokens(encode(value)) == tokens(value)


def test_exact_token_boundary():
    assert tokens({"x": [0] * 125}) == 128
    encode({"x": [0] * 125})
    with pytest.raises(ProtocolError):
        encode({"x": [0] * 126})


@pytest.mark.parametrize("text", ["汉🙂字" * 700, "a\"\\\n" * 800])
def test_utf8_limits(text):
    parts = list(chunks(text, total=1800))
    assert all(len(p.encode()) <= 511 for p in parts)
    assert len("".join(parts).encode()) <= 1800
    assert text.startswith("".join(parts))


async def test_hello_is_development_only(make_hub):
    hub, client, url = await make_hub(dev_allow_proto1=False)
    device = FakeDevice(client, url)
    assert (await device.connect())["error"]["code"] == -32012
    assert not hub.connections


async def test_handshake_and_unauthed_restrictions(make_hub):
    _, client, url = await make_hub()
    device = FakeDevice(client, url)
    device.ws = await client.ws_connect(url + "/v1/device")
    assert (await device.call("caps/list"))["error"]["code"] == -32010
    assert (await device.call("hello", {"proto": 2, "device": "example"}))["error"]["code"] == -32010
    assert "result" in await device.call("hello", {"proto": 1, "device": "example", "member": "demo-adult"})
    caps = (await device.call("caps/list"))["result"]
    assert {c["name"] for c in caps["tools"]} >= {"voice.chat", "asr.stream"}


async def test_voice_order_and_tool(make_hub):
    hub, client, url = await make_hub(model=FakeModel(tool="reader.next_page"))
    device = FakeDevice(client, url, [TOOL])
    await device.connect()
    rid, result = await device.voice()
    assert result["result"]["done"]
    messages = device.messages
    kinds = [m.get("method") for m in messages if isinstance(m, dict)]
    assert kinds.count("ai.stt") == 1 and kinds.count("tools/call") == 1
    end = next(i for i, m in enumerate(messages) if isinstance(m, dict) and m.get("method") == "ai.tts.end")
    audio = [i for i, m in enumerate(messages) if isinstance(m, bytes)]
    assert len(audio) == 5 and max(audio) < end < len(messages) - 1
    assert messages[-1]["id"] == rid
    assert all(m[10] == 0xA5 for m in messages if isinstance(m, bytes))
    assert hub.metrics.tool_calls == 1


async def test_actual_firmware_tools_survive_actions_overflow(make_hub, firmware_caps):
    assert firmware_caps["overflow"] is True
    tools = firmware_caps["tools"]
    assert len(tools) >= 50
    name = tools[-1]["name"]
    hub, client, url = await make_hub(model=FakeModel(tool=name))
    device = FakeDevice(client, url, tools)
    await device.connect()
    await device.voice(actions=[])
    connection = hub.connections["example-device"]
    assert {k[1] for k in connection.tools.definitions} == {t["name"] for t in tools}
    # Whether the final tool is policy-allowed is separate from not losing its definition.
    assert len([m for m in device.calls if m["method"] == "tools/list"]) == (len(tools) + 4) // 5


async def test_caps_pages_count_actual_envelopes(make_hub, firmware_caps):
    hub, client, url = await make_hub()
    for tool in firmware_caps["tools"]:
        hub.registry.register(Capability("x." + tool["name"][:37], tool["title"], hub.ask))
    device = FakeDevice(client, url)
    await device.connect()
    names, cursor = [], None
    while True:
        msg = await device.call("caps/list", {"cursor": cursor} if cursor else {})
        assert lexical_tokens(json.dumps(msg)) <= 110
        result = msg["result"]
        names.extend(t["name"] for t in result["tools"])
        cursor = result.get("nextCursor")
        if cursor is None:
            break
    assert len(names) == len(set(names)) == len(hub.registry.visible("child"))


@pytest.mark.parametrize("cursor", ["-1", "bad", "999999"])
def test_invalid_page_cursor(cursor):
    with pytest.raises(ProtocolError):
        page([{"name": "x"}], 1, cursor)


async def test_query_receipt_is_mcp_shape(make_hub):
    _, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    msg = await device.call("tools/call", {"name": "asr.transcribe", "arguments": {"example": True}})
    assert msg["result"]["structuredContent"]["result"]["text"] == "翻到下一页"
    assert msg["result"]["isError"] is False
    error = await device.call("tools/call", {"name": "confirm.accept", "arguments": {}})
    assert error["error"]["code"] == -32601


async def test_text_answer_uses_delta_and_caps_bytes(make_hub):
    _, client, url = await make_hub(model=FakeModel(text="中文🙂" * 800))
    device = FakeDevice(client, url)
    await device.connect()
    await device.call("ai.ask", {"question": "示例", "page": {"actions": []}})
    pieces = [m["params"]["text"] for m in device.messages if isinstance(m, dict) and m.get("method") == "ai.delta"]
    assert "".join(pieces) == "中文🙂" * 800  # 8 KB: within the device's 32 KB, nothing dropped
    assert len(pieces) > 1


async def test_over_long_answer_ends_with_the_device_note(make_hub):
    text = "中文🙂" * 4000  # 40 KB
    _, client, url = await make_hub(model=FakeModel(text=text))
    device = FakeDevice(client, url)
    await device.connect()
    await device.call("ai.ask", {"question": "示例", "page": {"actions": []}})
    sent = "".join(m["params"]["text"] for m in device.messages if isinstance(m, dict) and m.get("method") == "ai.delta")
    assert sent.endswith(ANSWER_CUT) and text.startswith(sent[:-len(ANSWER_CUT)])
    # the device keeps it whole: its own cut leaves room for its note and the terminator (INK_AI_ANSWER_MAX)
    assert len(sent.encode()) <= ANSWER_BYTES == 32768 - len(ANSWER_CUT.encode()) - 1
    assert len(sent.encode()) > 32000


@pytest.mark.parametrize("receipt", [{"status": "stale", "page_gen": 13}, {"status": "rejected"}, {"status": "failed"}, {"bad": 1}])
async def test_error_receipts_do_not_reexecute(make_hub, receipt):
    _, client, url = await make_hub(model=FakeModel(tool="reader.next_page"))
    device = FakeDevice(client, url, [TOOL])
    device.receipt = receipt
    await device.connect()
    await device.voice()
    assert len([c for c in device.calls if c["method"] == "tools/call"]) == 1


async def test_device_rpc_error(make_hub):
    _, client, url = await make_hub(model=FakeModel(tool="reader.next_page"))
    device = FakeDevice(client, url, [TOOL])
    await device.connect()
    device.rpc_error = {"code": -32603, "message": "synthetic error"}
    _, result = await device.voice()
    assert result["result"]["done"]
    assert not [c for c in device.calls if c["method"] == "tools/call"]


async def test_reply_ids_out_of_order_and_late(make_hub):
    hub, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    connection = hub.connections["example-device"]
    first = asyncio.create_task(connection.request("tools/list", {}))
    second = asyncio.create_task(connection.request("tools/list", {}))
    a, b = await device.recv(), await device.recv()
    await device.ws.send_json(response(b["id"], {"second": True}))
    assert await second == {"second": True}
    await device.ws.send_json(response(a["id"], {"first": True}))
    assert await first == {"first": True}
    await device.ws.send_json(response(a["id"], {"late": True}))
    assert "result" in await device.call("caps/list")


async def test_request_timeout_cleans_pending(make_hub):
    hub, client, url = await make_hub(request_timeout=0.02)
    device = FakeDevice(client, url)
    await device.connect()
    connection = hub.connections["example-device"]
    task = asyncio.create_task(connection.request("tools/list", {}))
    msg = await device.recv()
    with pytest.raises(TimeoutError):
        await task
    assert not connection.pending
    await device.ws.send_json(response(msg["id"], {"tools": []}))
    assert "result" in await device.call("caps/list")


async def test_concurrent_reply_does_not_swallow_cancellation(make_hub):
    hub, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    connection = hub.connections["example-device"]
    task = asyncio.create_task(connection.request("tools/list", {}))
    request = await device.recv()
    connection.pending[request["id"]].set_result({"tools": []})
    task.cancel()
    with pytest.raises(asyncio.CancelledError):
        await task
    assert not connection.pending


async def test_turn_timeout_does_not_wait_forever(make_hub):
    gate = asyncio.Event()
    hub, client, url = await make_hub(model=FakeModel(gate=gate))
    device = FakeDevice(client, url)
    await device.connect()
    result = await device.call("ai.ask", {"question": "示例", "budget_ms": 3030, "page": {"actions": []}})
    assert result["result"]["done"]
    assert hub.metrics.turns == 1


async def test_reconnect_resets_view_and_closes_old(make_hub):
    hub, client, url = await make_hub()
    first, second = FakeDevice(client, url, [TOOL]), FakeDevice(client, url)
    await first.connect()
    old = hub.connections["example-device"]
    request = asyncio.create_task(old.request("tools/list", {}))
    await first.recv()
    await second.connect()
    with pytest.raises(ConnectionError):
        await request
    assert old.closed and not hub.connections["example-device"].tools.definitions
    assert (await first.ws.receive()).type in (WSMsgType.CLOSE, WSMsgType.CLOSED)


async def test_member_change_cancels_late_output(make_hub):
    gate = asyncio.Event()
    hub, client, url = await make_hub(model=FakeModel(gate=gate))
    device = FakeDevice(client, url)
    await device.connect()
    await device.send("ai.ask", {"question": "示例", "page": {"actions": []}})
    await device.answer(await device.recv())
    await device.send("ink/member", {"member": "default", "epoch": 1}, rid=False)
    assert (await device.recv())["method"] == "caps/changed"
    gate.set()
    await device.call("caps/list")  # Ordering barrier, not a timed sleep.
    assert not [m for m in device.messages if isinstance(m, dict) and m.get("method") == "ai.delta"]
    assert hub.connections["example-device"].epoch == 1


async def test_password_asr_refused_and_synthetic_dictation(make_hub):
    hub, client, url = await make_hub(asr=FakeASR("示例输入。"))
    device = FakeDevice(client, url)
    await device.connect()
    assert (await device.call("rt.open", {"sid": 4, "cap": "asr.stream", "params": {"kind": "password"}}))["error"]["code"] == -32020
    await device.call("rt.open", {"sid": 4, "cap": "asr.stream", "params": {"kind": "text"}})
    await device.ws.send_bytes(frame(1, 4, 0, b"example"))
    await device.send("rt.turn", {"sid": 4, "op": "end"}, rid=False)
    assert (await device.recv())["method"] == "rt.asr"
    assert (await device.recv())["method"] == "rt.asr.done"
    assert hub.model.calls == 0


async def test_bad_binary_sequence_and_frame_limit(make_hub):
    _, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    await device.call("rt.open", {"sid": 4, "cap": "voice.chat", "params": {}})
    await device.ws.send_bytes(frame(1, 4, 2, b"out-of-order"))
    assert (await device.recv())["method"] == "rt.close"


async def test_unauthenticated_size_limit(make_hub):
    _, client, url = await make_hub()
    async with client.ws_connect(url + "/v1/device") as ws:
        await ws.send_str(" " * 4097)
        msg = await ws.receive()
        assert msg.type == WSMsgType.CLOSE and msg.data == 1009


async def test_vendor_failure_returns_control(make_hub):
    _, client, url = await make_hub(model=FakeModel(fail=True))
    device = FakeDevice(client, url)
    await device.connect()
    assert (await device.call("ai.ask", {"question": "示例", "page": {"actions": []}}))["result"]["done"]


async def test_device_epoch_survives_hello_and_bounds_tool_delivery(make_hub):
    hub, client, url = await make_hub(model=FakeModel(tool=TOOL['name']))
    device = FakeDevice(client, url, [TOOL])
    device.epoch = 17
    await device.connect()
    await device.voice(actions=[TOOL['name']])
    delivered = next(c for c in device.calls if c['method'] == 'tools/call')
    assert delivered['params']['_meta']['epoch'] == 17
    await device.send('ink/member', {'member': 'default', 'authed': False, 'epoch': 18}, rid=False)
    assert (await device.recv())['method'] == 'caps/changed'
    device.epoch = 18
    assert hub.connections['example-device'].epoch == 18
    old = asyncio.create_task(hub.connections['example-device'].request('tools/call', delivered['params']))
    await device.answer(await device.recv())
    with pytest.raises(ProtocolError):
        await old


async def test_event_ack_reconnect_duplicate_and_gap(make_hub):
    hub, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    event = {'event': 'fixture.event', 'args': {'example': True}, 'boot': 3, 'seq': 1, 'member': 'default'}
    for seq in (1, 1, 3):
        event['seq'] = seq
        await device.send('device/event', event, rid=False)
        ack = await device.recv()
        assert ack['method'] == 'device/event_ack' and ack['params'] == {'boot': 3, 'upto': seq}
    assert hub.metrics.event_gaps == 1
    await device.ws.close()
    device = FakeDevice(client, url)
    await device.connect()
    await device.send('device/event', event, rid=False)
    assert (await device.recv())['params'] == {'boot': 3, 'upto': 3}
    assert hub.metrics.event_gaps == 1
    event['boot'], event['seq'] = 4, 1
    await device.send('device/event', event, rid=False)
    assert (await device.recv())['params'] == {'boot': 4, 'upto': 1}
    assert hub.metrics.event_gaps == 2
    assert hub.event_positions == {'example-device': (4, 1)}


@pytest.mark.parametrize('change', [{'member': 'demo-adult'}, {'boot': True}, {'seq': 0}, {'seq': '1'}, {'args': []}])
async def test_invalid_event_is_not_acknowledged(make_hub, change):
    hub, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    event = {'event': 'fixture.event', 'args': {}, 'boot': 1, 'seq': 1, 'member': 'default', **change}
    assert (await device.call('device/event', event))['error']['code'] == -32602
    assert not hub.event_positions


@pytest.mark.parametrize('epoch', [True, -1, '3', 2**32])
async def test_invalid_hello_epoch_does_not_register_connection(make_hub, epoch):
    hub, client, url = await make_hub()
    device = FakeDevice(client, url)
    device.epoch = epoch
    assert (await device.connect())['error']['code'] == -32602
    assert not hub.connections
