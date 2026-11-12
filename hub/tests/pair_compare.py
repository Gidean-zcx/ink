"""Manual before/after comparison; the baseline is optional Git history, never a CTest dependency."""
import asyncio
import json
import os
import subprocess
import sys
import types
from pathlib import Path

for key in list(os.environ):
    if key == "DEEPSEEK_API_KEY" or key.startswith("INK_HUB_"):
        del os.environ[key]

from aiohttp import ClientSession, web
from ink_hub.gateway.server import Hub
import ink_hub.gateway.server as server
from tests.fake_device import FakeDevice
from tests.contract.test_hub_pair_model import TOOL


def baseline_module(path, package):
    result = subprocess.run(["git", "show", sys.argv[1] + ":" + path], capture_output=True, text=True)
    if result.returncode:
        raise SystemExit("baseline unavailable; this manual comparison needs the requested Git history")
    module = types.ModuleType("comparison")
    module.__package__ = package
    exec(compile(result.stdout, path, "exec"), module.__dict__)
    return module


async def run():
    original_turn = server.run_turn
    old_flow = baseline_module("hub/ink_hub/voice/flow.py", "ink_hub.voice")
    old_model = baseline_module("hub/ink_hub/models/remote.py", "ink_hub.models")
    from ink_hub.models.remote import RemoteModel
    for phase in ("before", "after"):
        requests = []

        async def vendor(request):
            body = await request.json()
            requests.append(body)
            function = body["tools"][0]["function"]
            arguments = {"value": 30} if "value" in function["parameters"].get("properties", {}) else {}
            events = [{"choices": [{"delta": {"tool_calls": [{"index": 0, "function": {
                "name": function["name"], "arguments": json.dumps(arguments)}}]}}]},
                {"choices": [{"delta": {}, "finish_reason": "tool_calls"}]}]
            payload = "".join("data: " + json.dumps(e) + "\n\n" for e in events) + "data: [DONE]\n\n"
            return web.Response(text=payload, content_type="text/event-stream")

        vendor_app = web.Application()
        vendor_app.router.add_post("/chat/completions", vendor)
        vendor_runner = web.AppRunner(vendor_app, access_log=None)
        await vendor_runner.setup()
        site = web.TCPSite(vendor_runner, "127.0.0.1", 0)
        await site.start()
        os.environ.update(INK_HUB_MODEL_PROVIDER="openai", INK_HUB_OPENAI_API_KEY="sk-test-0000",
            INK_HUB_OPENAI_MODEL="test-model", INK_HUB_OPENAI_BASE_URL=f"http://127.0.0.1:{site._server.sockets[0].getsockname()[1]}")
        server.run_turn = old_flow.run_turn if phase == "before" else original_turn
        directory = Path("work/hub-pair-1/compare") / phase
        hub = Hub(directory, dev_allow_proto1=True)
        hub.model.provider = (old_model.RemoteModel if phase == "before" else RemoteModel)("openai", "INK_HUB_OPENAI")
        runner = web.AppRunner(hub.application(), access_log=None)
        await runner.setup()
        site = web.TCPSite(runner, "127.0.0.1", 0)
        await site.start()
        try:
            async with ClientSession() as client:
                device = FakeDevice(client, f"http://127.0.0.1:{site._server.sockets[0].getsockname()[1]}", [TOOL])
                await device.connect()
                await device.call("ai.ask", {"question": "把亮度调到 30"})
                calls = [c["params"] for c in device.calls if c["method"] == "tools/call"]
                print(json.dumps({"phase": phase, "question": "把亮度调到 30",
                    "system": requests[0]["messages"][0]["content"], "function": requests[0]["tools"][0]["function"],
                    "device_call": [{"name": c["name"], "arguments": c["arguments"]} for c in calls]}, ensure_ascii=False))
        finally:
            await runner.cleanup()
            await vendor_runner.cleanup()
    server.run_turn = original_turn


asyncio.run(run())
