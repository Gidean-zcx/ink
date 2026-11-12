import asyncio
import json

import pytest
from aiohttp import web

from ink_hub.models.fake import FakeModel
from tests.fake_device import FakeDevice


@pytest.fixture(autouse=True)
def clean_model_env(monkeypatch):
    for provider in ("OPENAI", "ANTHROPIC"):
        for field in ("API_KEY", "BASE_URL", "MODEL"):
            monkeypatch.delenv(f"INK_HUB_{provider}_{field}", raising=False)
    monkeypatch.delenv("INK_HUB_MODEL_PROVIDER", raising=False)


@pytest.fixture
async def vendor(monkeypatch):
    runners = []

    async def start(provider, *, mode="normal", gate=None):
        requests = []
        dummy = "test-only-not-a-credential"

        async def handler(request):
            assert request.headers.get("x-api-key" if provider == "anthropic" else "Authorization") == (
                dummy if provider == "anthropic" else "Bearer " + dummy)
            if provider == "anthropic":
                assert request.headers["anthropic-version"] == "2023-06-01"
            body = await request.json()
            requests.append(body)
            assert body["model"] == "test-model" and body["stream"] is True
            if mode == "http-error":
                return web.Response(status=401, text=dummy)
            reply = web.StreamResponse(headers={"Content-Type": "text/event-stream"})
            await reply.prepare(request)

            async def emit(event):
                wire = ("data: " + json.dumps(event, ensure_ascii=False) + "\r\n\r\n").encode()
                # Split a UTF-8 codepoint and an SSE line across network writes.
                cut = wire.find("你".encode()) + 1
                await reply.write(wire[:cut])
                await reply.write(wire[cut:])

            tool = bool(body.get("tools"))
            if provider == "openai":
                await emit({"choices": [{"delta": {"content": "你好"}}]})
                if gate:
                    await gate.wait()
                if tool:
                    assert body["tools"][0]["function"]["description"] == "下一页"
                    for function in ({"name": "ink_0", "arguments": "{"}, {"arguments": "}"}):
                        await emit({"choices": [{"delta": {"tool_calls": [{"index": 0, "function": function}]}}]})
                await emit({"choices": [{"delta": {}, "finish_reason": "tool_calls" if tool else "stop"}]})
                if mode != "truncated":
                    await reply.write(b"data: [DONE]\n\n")
            else:
                await emit({"type": "message_start", "message": {"content": []}})
                await emit({"type": "content_block_delta", "index": 0, "delta": {"type": "text_delta", "text": "你好"}})
                if gate:
                    await gate.wait()
                if tool:
                    assert body["tools"][0]["description"] == "下一页"
                    await emit({"type": "content_block_start", "index": 1,
                                "content_block": {"type": "tool_use", "id": "test-call", "name": "ink_0", "input": {}}})
                    for part in ("{", "}"):
                        await emit({"type": "content_block_delta", "index": 1,
                                    "delta": {"type": "input_json_delta", "partial_json": part}})
                await emit({"type": "message_delta", "delta": {"stop_reason": "tool_use" if tool else "end_turn"}})
                if mode != "truncated":
                    await emit({"type": "message_stop"})
            await reply.write_eof()
            return reply

        app = web.Application()
        app.router.add_post("/v1/" + ("messages" if provider == "anthropic" else "chat/completions"), handler)
        runner = web.AppRunner(app, access_log=None)
        await runner.setup()
        site = web.TCPSite(runner, "127.0.0.1", 0)
        await site.start()
        runners.append(runner)
        port = site._server.sockets[0].getsockname()[1]
        prefix = "INK_HUB_" + provider.upper()
        monkeypatch.setenv("INK_HUB_MODEL_PROVIDER", provider)
        monkeypatch.setenv(prefix + "_API_KEY", dummy)
        monkeypatch.setenv(prefix + "_BASE_URL", f"http://127.0.0.1:{port}/v1")
        monkeypatch.setenv(prefix + "_MODEL", "test-model")
        return requests, dummy

    yield start
    for runner in runners:
        await runner.cleanup()


@pytest.mark.parametrize("provider", ["openai", "anthropic"])
async def test_models_dialogue_and_device_tool(provider, vendor, make_hub, caplog):
    requests, dummy = await vendor(provider)
    hub, client, url = await make_hub()
    assert (await hub.model.answer("问候", [])).text == "你好"
    tool = {"name": "reader.next_page", "title": "下一页", "effect": "navigate", "confirm": "none",
            "inputSchema": {"type": "object", "properties": {}}}
    device = FakeDevice(client, url, [tool])
    await device.connect()
    _, result = await device.voice()
    assert result["result"]["done"] and hub.metrics.tool_calls == 1
    call = next(c for c in device.calls if c["method"] == "tools/call")
    assert call["params"]["name"] == tool["name"] and call["params"]["arguments"] == {}
    assert dummy not in caplog.text and dummy not in "".join(hub.samples)
    assert len(requests) == 2


@pytest.mark.parametrize("provider", ["openai", "anthropic"])
async def test_incremental_text_before_response_ends(provider, vendor):
    gate = asyncio.Event()
    await vendor(provider, gate=gate)
    stream = FakeModel().provider.stream("问候", [])
    try:
        first = await asyncio.wait_for(anext(stream), 2)
        assert first.text == "你好" and not gate.is_set()
        gate.set()
        assert [part async for part in stream] == []
    finally:
        gate.set()
        await stream.aclose()


@pytest.mark.parametrize("provider", ["openai", "anthropic"])
async def test_no_key_keeps_fake(provider, monkeypatch):
    monkeypatch.setenv("INK_HUB_MODEL_PROVIDER", provider)
    model = FakeModel()
    assert model.provider is None
    assert (await model.answer("问候", [])).text == "这是示例回答。"


@pytest.mark.parametrize("provider", ["openai", "anthropic"])
async def test_vendor_error_never_discloses_credentials(provider, vendor, caplog):
    _, dummy = await vendor(provider, mode="http-error")
    with pytest.raises(RuntimeError) as caught:
        await FakeModel().answer("问候", [])
    assert dummy not in str(caught.value) + caplog.text


async def test_incomplete_anthropic_stream_is_rejected(vendor):
    await vendor("anthropic", mode="truncated")
    with pytest.raises(RuntimeError, match="未完成"):
        await FakeModel().answer("问候", [])
