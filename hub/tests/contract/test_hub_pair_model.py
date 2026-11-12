import asyncio
import json
import logging
import os
import subprocess
from pathlib import Path

import pytest
from aiohttp import web

from ink_hub.models import from_environment
from tests.fake_device import FakeDevice

TOOL = {"name": "display.set_brightness", "title": "设置亮度（0 到 100）", "effect": "setting",
        "inputSchema": {"type": "object", "properties": {"value": {"type": "integer", "minimum": 0, "maximum": 100}},
                        "required": ["value"], "additionalProperties": False}}


@pytest.fixture
async def streaming_vendor(monkeypatch):
    runners = []

    async def start(parts=("第一段。", "第二段。", "第三段。"), *, arguments=None, fail=False):
        requests, ended = [], asyncio.Event()

        async def handler(request):
            body = await request.json()
            requests.append(body)
            reply = web.StreamResponse(headers={"Content-Type": "text/event-stream"})
            await reply.prepare(request)

            async def emit(delta, finish=None):
                await reply.write(("data: " + json.dumps({"choices": [{"delta": delta, "finish_reason": finish}]},
                                                       ensure_ascii=False) + "\n\n").encode())

            for text in parts:
                await emit({"content": text})
                delayed = asyncio.Event()
                timer = asyncio.get_running_loop().call_later(0.3, delayed.set)  # Deliberate vendor latency.
                try:
                    await delayed.wait()
                finally:
                    timer.cancel()
                if fail:
                    ended.set()
                    await reply.write_eof()
                    return reply
            if arguments is not None:
                name = body["tools"][0]["function"]["name"]
                await emit({"tool_calls": [{"index": 0, "function": {"name": name, "arguments": json.dumps(arguments)}}]})
            await emit({}, "tool_calls" if arguments is not None else "stop")
            await reply.write(b"data: [DONE]\n\n")
            ended.set()
            await reply.write_eof()
            return reply

        app = web.Application()
        app.router.add_post("/chat/completions", handler)
        runner = web.AppRunner(app, access_log=None)
        await runner.setup()
        site = web.TCPSite(runner, "127.0.0.1", 0)
        await site.start()
        runners.append(runner)
        monkeypatch.setenv("INK_HUB_MODEL_PROVIDER", "openai")
        monkeypatch.setenv("INK_HUB_OPENAI_API_KEY", "sk-test-0000")
        monkeypatch.setenv("INK_HUB_OPENAI_MODEL", "test-model")
        monkeypatch.setenv("INK_HUB_OPENAI_BASE_URL", f"http://127.0.0.1:{site._server.sockets[0].getsockname()[1]}")
        return requests, ended

    yield start
    for runner in runners:
        await runner.cleanup()


def shown(device):
    return "".join(m["params"]["text"] for m in device.messages if isinstance(m, dict) and m.get("method") == "ai.delta")


@pytest.mark.parametrize("arguments,called", [({"value": 30}, True), ({}, False), ({"value": 300}, False)])
async def test_real_schema_and_parameters_reach_device(streaming_vendor, make_hub, arguments, called):
    requests, _ = await streaming_vendor(parts=("我来调整。",), arguments=arguments)
    _, client, url = await make_hub()
    device = FakeDevice(client, url, [TOOL])
    device.receipt = {"status": "done", "say": "亮度已调到 30。"}
    await device.connect()
    await device.call("ai.ask", {"question": "把亮度调到 30"})
    function = requests[0]["tools"][0]["function"]
    assert function["description"] == TOOL["title"]
    assert function["parameters"] == TOOL["inputSchema"]
    assert function["name"] == "ink_0"
    calls = [c for c in device.calls if c["method"] == "tools/call"]
    assert bool(calls) is called
    if called:
        assert calls[0]["params"]["name"] == TOOL["name"] and calls[0]["params"]["arguments"] == {"value": 30}
        assert shown(device) == "我来调整。亮度已调到 30。"
    else:
        assert shown(device) == "我来调整。这次没有完成，请重试。"


@pytest.mark.parametrize("share,member", [(False, {"role": "child", "name": "合成称呼青禾"}),
                                        (True, {"role": "child", "name": "合成称呼青禾"}), (True, {})])
async def test_system_prompt_and_private_member(streaming_vendor, make_hub, monkeypatch, share, member):
    requests, _ = await streaming_vendor(parts=("你好。",))
    if share:
        monkeypatch.setenv("INK_HUB_SHARE_MEMBER_NAME", "1")
    _, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    await device.call("ai.ask", {"question": "你是谁", "member": member})
    raw = json.dumps(requests[0], ensure_ascii=False)
    system = requests[0]["messages"][0]["content"]
    assert ("合成称呼青禾" in raw) is (share and bool(member))
    assert all(value not in raw for value in ("Z-Alpha", "default", "demo-adult", "<称呼>"))
    assert all(value in system for value in ("墨水屏", "三句以内", "设备回执", "孩子", "lang=zh"))


@pytest.mark.parametrize("parts,fail", [(("第一段。", "第二段。", "第三段。"), False),
                                      (("汉🙂字" * 300,), False), (("已经收到的字。",), True)])
async def test_stream_reaches_device_before_finish(streaming_vendor, make_hub, parts, fail):
    _, ended = await streaming_vendor(parts=parts, fail=fail)
    _, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    rid = await device.send("ai.ask", {"question": "示例"})
    while True:
        message = await device.recv()
        await device.answer(message)
        if message.get("method") == "ai.delta":
            assert not ended.is_set()
            break
    assert (await device.until(rid))["result"] == {"done": True}
    expected = "".join(parts) + ("这次没有完成，请重试。" if fail else "")
    assert shown(device) == expected
    deltas = [m["params"] for m in device.messages if m.get("method") == "ai.delta"]
    assert all(len(p["text"].encode()) <= 511 for p in deltas)
    assert [p["seq"] for p in deltas] == list(range(len(deltas)))


async def test_first_speech_frame_precedes_model_end(streaming_vendor, make_hub):
    _, ended = await streaming_vendor()

    class TTS:
        async def synthesize(self, text):
            return [b"synthetic-opus"]

    hub, client, url = await make_hub(tts=TTS())
    device = FakeDevice(client, url)
    await device.connect()
    connection = hub.connections["example-device"]
    send = connection.send
    observed = []

    async def observe(message, **kwargs):
        await send(message, **kwargs)
        if isinstance(message, bytes):
            observed.append(ended.is_set())

    connection.send = observe
    await device.voice()
    assert observed and observed[0] is False


async def test_tool_limit_and_page_order(streaming_vendor, make_hub, caplog):
    requests, _ = await streaming_vendor(parts=("你好。",))
    hub, client, url = await make_hub()
    tools = [{**TOOL, "name": f"test.action_{i}", "title": f"合成能力 {i}"} for i in range(130)]
    device = FakeDevice(client, url, tools)
    await device.connect()
    # A page can contain more than 128 actions; its own order wins over directory order.
    actions = [tool["name"] for tool in reversed(tools)]
    with caplog.at_level(logging.INFO):
        await device.call("ai.ask", {"question": "示例", "page": {"actions": actions}})
    descriptors = requests[0]["tools"]
    assert len(descriptors) == 128 and "能力目录截断：2" in caplog.text
    assert descriptors[0]["function"]["description"] == "合成能力 129"
    assert descriptors[-1]["function"]["description"] == "合成能力 2"
    # Confirm the actual descriptors were selected, without logging arguments.
    connection = hub.connections["example-device"]
    assert len(connection.tools.definitions) == 130


def test_deepseek_default_and_safe_start_script(tmp_path, monkeypatch, caplog):
    for key in list(os.environ):
        if key.startswith("INK_HUB_") or key == "DEEPSEEK_API_KEY":
            monkeypatch.delenv(key)
    monkeypatch.setenv("DEEPSEEK_API_KEY", "sk-test-0000")
    model = from_environment()
    assert (model.provider, model.base, model.model) == ("openai", "https://api.deepseek.com", "deepseek-chat")
    monkeypatch.setenv("INK_HUB_MODEL_PROVIDER", "fake")
    assert from_environment() is None
    monkeypatch.delenv("INK_HUB_MODEL_PROVIDER")
    fake = tmp_path / "synthetic.env"
    marker = tmp_path / "must-not-exist"
    fake.write_text(f"UNRELATED=$(touch {marker})\nDEEPSEEK_API_KEY=sk-test-0000\nOTHER=private-test-data\n")
    fake.chmod(0o644)
    import sys
    env = {**os.environ, "INK_HUB_ENV_FILE": str(fake), "INK_HUB_PYTHON": sys.executable}
    script = Path(__file__).resolve().parents[2] / "deploy/start-mac.sh"
    reply = subprocess.run([str(script), "--check"], capture_output=True, text=True, env=env)
    assert reply.returncode == 0 and json.loads(reply.stdout)["model"] == "deepseek-chat"
    assert "权限" in reply.stderr and not marker.exists()
    assert "sk-test-0000" not in reply.stdout + reply.stderr + caplog.text
    assert "private-test-data" not in reply.stdout + reply.stderr


async def test_speech_environment_extension_is_used(make_hub, monkeypatch):
    import ink_hub.speech
    from ink_hub.speech.fake import FakeASR, FakeTTS
    asr, tts = FakeASR("合成识别"), FakeTTS()
    monkeypatch.setattr(ink_hub.speech, "from_environment", lambda: (asr, tts), raising=False)
    hub, _, _ = await make_hub()
    assert hub.asr is asr and hub.tts is tts
    monkeypatch.setattr(ink_hub.speech, "from_environment", lambda: None)
    hub, _, _ = await make_hub()
    assert isinstance(hub.asr, FakeASR) and isinstance(hub.tts, FakeTTS)
