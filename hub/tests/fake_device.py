"""Strict Python device, using the device wire protocol rather than hub scripts."""
import asyncio
import json
import re

from aiohttp import WSMsgType

ATOM = re.compile(r'"(?:[^"\\]|\\.)*"|true|false|null|-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?|[\[{]')


def lexical_tokens(raw):
    json.loads(raw)
    return len(ATOM.findall(raw))


class FakeDevice:
    def __init__(self, client, url, tools=(), page_size=5):
        self.client, self.url = client, url
        self.tools, self.page_size = list(tools), page_size
        self.rid = 0
        self.ws = None
        self.messages = []
        self.calls = []
        self.receipt = {"status": "done", "say": "已完成。"}
        self.rpc_error = None

    async def connect(self, device="example-device", proto=1):
        self.ws = await self.client.ws_connect(self.url + "/v1/device")
        return await self.call("hello", {"proto": proto, "device": device, "member": "default"})

    async def send(self, method, params=None, rid=True):
        self.rid += 1
        msg = {"jsonrpc": "2.0", "method": method, "params": params or {}}
        if rid:
            msg["id"] = self.rid
        await self.ws.send_json(msg)
        return self.rid

    async def recv(self):
        msg = await asyncio.wait_for(self.ws.receive(), 5)
        if msg.type == WSMsgType.BINARY:
            assert len(msg.data) >= 10 and msg.data[9] == 0
            self.messages.append(msg.data)
            return msg.data
        assert msg.type == WSMsgType.TEXT, msg.type
        assert lexical_tokens(msg.data) <= 128, "中枢超过设备词法预算"
        data = json.loads(msg.data)
        assert data["jsonrpc"] == "2.0"
        if "method" in data:
            assert len(data["method"].encode()) <= 31
            if "id" in data:
                assert type(data["id"]) is int
            if data["method"] in ("ai.delta", "ai.stt"):
                assert len(data["params"]["text"].encode()) <= 511
        self.messages.append(data)
        return data

    async def answer(self, msg):
        if not isinstance(msg, dict) or "method" not in msg or "id" not in msg:
            return
        self.calls.append(msg)
        method, params = msg["method"], msg["params"]
        if method == "tools/list":
            start = int(params.get("cursor", "0"))
            result = {"tools": self.tools[start:start + self.page_size]}
            if start + self.page_size < len(self.tools):
                result["nextCursor"] = str(start + self.page_size)
        elif method == "tools/call":
            meta = params["_meta"]
            assert len(meta["request_id"].encode()) <= 39
            assert meta["member"] == "default" and meta["epoch"] == 0
            result = self.receipt
        else:
            raise AssertionError(f"协议没有约定这个请求: {method}")
        reply = {"jsonrpc": "2.0", "id": msg["id"], "result": result}
        if self.rpc_error:
            reply.pop("result")
            reply["error"] = self.rpc_error
        await self.ws.send_json(reply)

    async def until(self, rid):
        while True:
            msg = await self.recv()
            if isinstance(msg, dict) and msg.get("id") == rid and "method" not in msg:
                return msg
            await self.answer(msg)

    async def call(self, method, params=None):
        return await self.until(await self.send(method, params))

    async def voice(self, actions=(), page_gen=12):
        await self.send("rt.open", {"sid": 7, "cap": "voice.chat", "params": {}})
        rid = await self.send("ai.listen", {"stream": 7, "format": "opus", "rate": 16000, "frame_ms": 60,
                                            "page_gen": page_gen, "page": {"app": "reader", "text": "示例页面", "actions": list(actions)}})
        # The device only sends audio after rt.open has been acknowledged.
        while True:
            msg = await self.recv()
            await self.answer(msg)
            if isinstance(msg, dict) and msg.get("result") == {"sid": 7}:
                break
        from ink_hub.gateway.wire import frame
        await self.ws.send_bytes(frame(1, 7, 0, b"synthetic-audio"))
        await self.send("ai.listen.end", {"stream": 7, "frames": 1}, rid=False)
        result = await self.until(rid)
        await self.send("rt.close", {"sid": 7}, rid=False)
        return rid, result
