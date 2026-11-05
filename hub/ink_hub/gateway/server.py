import asyncio
import json
import secrets
import time
from dataclasses import dataclass, field
from pathlib import Path

from aiohttp import WSMsgType, web

from ink_hub.agent.tools import DeviceTools
from ink_hub.bus.registry import Capability, Registry
from ink_hub.console.setup import Setup
from ink_hub.data.scope import Authority
from ink_hub.jobs.queue import JobQueue
from ink_hub.models.fake import FakeModel
from ink_hub.ops.metrics import Metrics
from ink_hub.speech.fake import FakeASR, FakeTTS
from ink_hub.voice.flow import run_turn, run_dictation
from .wire import (ANSWER_BYTES, ProtocolError, chunks, dumps, encode, frame,
                   notification, page, parse_frame, response)


@dataclass
class Session:
    cap: str
    epoch: int
    frames: list = field(default_factory=list)
    size: int = 0
    seq: int = 0
    ended: asyncio.Event = field(default_factory=asyncio.Event)


class Connection:
    def __init__(self, hub, ws):
        self.hub, self.ws = hub, ws
        self.id = secrets.token_hex(8)
        self.device = None
        self.scope = None
        self.epoch = 0
        self.authed = False
        self.closed = False
        self.sessions = {}
        self.turns = {}
        self.pending = {}
        self.rid = 1000
        self.queue = asyncio.PriorityQueue(maxsize=128)
        self.sequence = 0
        self.tools = DeviceTools(self.request)
        self.sender = asyncio.create_task(self._sender())

    async def _sender(self):
        try:
            while True:
                _, _, data, guard, done = await self.queue.get()
                try:
                    if guard is None or guard():
                        if isinstance(data, bytes):
                            await self.ws.send_bytes(data)
                        else:
                            await self.ws.send_str(data)
                            if self.hub.samples is not None:
                                self.hub.samples.append(data)
                    if not done.done():
                        done.set_result(None)
                except Exception:
                    if not done.done():
                        done.set_exception(ConnectionError("连接已关闭"))
                    raise
                finally:
                    self.queue.task_done()
        except (ConnectionError, RuntimeError):
            await self.ws.close()

    async def send(self, message, *, priority=3, guard=None):
        if self.closed:
            raise ConnectionError("连接已关闭")
        data = message if isinstance(message, bytes) else encode(message)
        if isinstance(message, bytes):
            priority = {1: 0, 2: 0, 4: 1, 3: 2, 6: 4, 7: 4}.get(message[0], 3)
        done = asyncio.get_running_loop().create_future()
        self.sequence += 1
        await self.queue.put((priority, self.sequence, data, guard, done))
        await done

    async def notify(self, method, params, guard=None):
        await self.send(notification(method, params), guard=guard)

    async def request(self, method, params, timeout=None):
        self.rid += 1
        rid = self.rid
        future = asyncio.get_running_loop().create_future()
        self.pending[rid] = future
        try:
            await self.send({"jsonrpc": "2.0", "id": rid, "method": method, "params": params})
            result = await asyncio.wait_for(future, timeout if timeout is not None else self.hub.request_timeout)
            # wait_for may return a concurrently completed future despite task cancellation.
            if asyncio.current_task().cancelling():
                raise asyncio.CancelledError
            return result
        finally:
            self.pending.pop(rid, None)

    async def error(self, rid, code, message):
        await self.send({"jsonrpc": "2.0", "id": rid, "error": {"code": code, "message": message}})

    async def cancel_work(self):
        tasks = list(self.turns.values())
        self.turns.clear()
        self.sessions.clear()
        for task in tasks:
            task.cancel()
        for future in self.pending.values():
            if not future.done():
                future.set_exception(ConnectionError("连接或家人已变化"))
        await asyncio.gather(*tasks, return_exceptions=True)

    async def close(self):
        self.closed = True
        await self.cancel_work()
        self.sender.cancel()
        await asyncio.gather(self.sender, return_exceptions=True)
        while not self.queue.empty():
            *_, done = self.queue.get_nowait()
            if not done.done():
                done.cancel()
            self.queue.task_done()
        await self.ws.close()

    async def receive(self, raw):
        rid = None
        try:
            if isinstance(raw, bytes):
                if not self.scope:
                    raise ProtocolError("请先连接", -32010)
                kind, sid, seq, _, _, payload = parse_frame(raw)
                session = self.sessions.get(sid)
                if not session or session.epoch != self.epoch:
                    return
                if kind != 1 or seq != session.seq or session.size + len(payload) > 128 * 1024:
                    await self.end_session(sid)
                    await self.notify("rt.close", {"sid": sid})
                    return
                session.seq = (seq + 1) & 65535
                session.frames.append(payload)
                session.size += len(payload)
                return
            if not self.scope and len(raw.encode()) > 4096:
                await self.ws.close(code=1009)
                return
            msg = json.loads(raw, parse_constant=lambda _: (_ for _ in ()).throw(ValueError()))
            if not isinstance(msg, dict) or msg.get("jsonrpc") != "2.0":
                raise ProtocolError("消息格式无效")
            rid = msg.get("id")
            if rid is not None and (type(rid) is not int or rid < 0 or rid > 2147483647):
                rid = None
                raise ProtocolError("请求编号必须是整数")
            if "method" not in msg:
                future = self.pending.get(rid)
                if future and not future.done():
                    if "error" in msg:
                        future.set_exception(ProtocolError("设备拒绝了请求", -32603))
                    elif "result" in msg:
                        future.set_result(msg["result"])
                    else:
                        future.set_exception(ProtocolError("设备回复无效"))
                return
            method, params = msg["method"], msg.get("params", {})
            if not isinstance(method, str) or len(method.encode()) > 31 or not isinstance(params, dict):
                raise ProtocolError("方法或参数无效")
            if not self.scope:
                if method != "hello":
                    raise ProtocolError("请先连接", -32010)
                await self.hello(rid, params)
                return
            self.hub.authority.check(self.scope)
            if method == "hello":
                raise ProtocolError("已经连接")
            if method == "caps/list":
                entries = [c.directory() for c in self.hub.registry.visible(self.hub.authority.role(self.scope))]
                await self.send(response(rid, page(entries, rid, params.get("cursor"))))
            elif method == "tools/list":
                entries = [c.tool() for c in self.hub.registry.visible(self.hub.authority.role(self.scope), "ai", "call")]
                await self.send(response(rid, page(entries, rid, params.get("cursor"))))
            elif method == "tools/call":
                receipt = await self.hub.registry.call(self.scope, params.get("name"), params.get("arguments", {}), self.hub.authority)
                await self.send(response(rid, {"content": [{"type": "text", "text": receipt.get("say", "")}],
                                               "structuredContent": receipt, "isError": receipt["status"] != "done"}))
            elif method == "rt.open":
                sid, cap = params.get("sid"), params.get("cap")
                if type(sid) is not int or sid not in range(1, 256) or sid in self.sessions:
                    raise ProtocolError("会话编号无效")
                allowed = {c.name for c in self.hub.registry.visible(self.hub.authority.role(self.scope), form="session")}
                if cap not in allowed:
                    raise ProtocolError("未提供这个会话", -32021)
                pp = params.get("params", {})
                if not isinstance(pp, dict) or len(dumps(pp).encode()) > 239:
                    raise ProtocolError("会话参数无效")
                if cap == "asr.stream" and pp.get("kind", "text") not in ("text", "number"):
                    raise ProtocolError("密码不能语音输入", -32020)
                if len(self.sessions) >= 4:
                    raise ProtocolError("会话已满", -32022)
                self.sessions[sid] = Session(cap, self.epoch)
                await self.send(response(rid, {"sid": sid}))
            elif method == "rt.close":
                await self.end_session(params.get("sid"))
            elif method in ("ai.listen", "ai.ask"):
                if rid is None or rid in self.turns:
                    raise ProtocolError("这一轮编号无效")
                sid = params.get("stream", 0)
                if method == "ai.listen" and (sid not in self.sessions or self.sessions[sid].cap != "voice.chat"):
                    raise ProtocolError("语音会话未打开")
                if len(self.turns) >= 4:
                    raise ProtocolError("正在处理的请求已满", -32022)
                task = asyncio.create_task(run_turn(self, rid, method, params))
                self.turns[rid] = task
                task.add_done_callback(lambda t, key=rid: self.turns.pop(key, None))
            elif method == "ai.listen.end":
                session = self.sessions.get(params.get("stream"))
                if session:
                    session.ended.set()
            elif method == "rt.turn":
                sid = params.get("sid")
                session = self.sessions.get(sid)
                if session and session.cap == "asr.stream" and params.get("op") == "end":
                    task = asyncio.create_task(run_dictation(self, sid, session))
                    self.turns[f"asr:{sid}"] = task
                    task.add_done_callback(lambda t: self.turns.pop(f"asr:{sid}", None))
            elif method == "ink/member":
                # Only synthetic configured identities exist in proto1 development mode.
                member, epoch = params.get("member"), params.get("epoch")
                if type(epoch) is not int or epoch <= self.epoch or member not in ("default", "demo-adult", "demo-other"):
                    raise ProtocolError("家人或代次无效")
                scope = self.hub.authority.issue("demo", member)
                # Proto1 cannot prove adult unlock; it never grants an adult space.
                if self.hub.authority.role(scope) == "adult":
                    raise ProtocolError("开发握手不能验证大人密码", -32020)
                await self.cancel_work()
                self.scope, self.epoch = scope, epoch
                self.authed = False
                await self.notify("caps/changed", {})
            elif method == "device/event":
                pass  # No persistent event body or activity data in step 1.
            else:
                raise ProtocolError("未提供这个方法", -32601)
        except (ProtocolError, PermissionError, ValueError, TypeError, KeyError) as exc:
            if rid is not None:
                await self.error(rid, exc.code if isinstance(exc, ProtocolError) else -32602,
                                 str(exc) if isinstance(exc, (ProtocolError, PermissionError)) else "参数无效")

    async def hello(self, rid, params):
        if rid is None or params.get("proto") != 1 or not self.hub.dev_allow_proto1:
            raise ProtocolError("只开放显式启用的开发握手", -32012)
        device = params.get("device")
        if not isinstance(device, str) or not 1 <= len(device.encode()) <= 64:
            raise ProtocolError("设备名称无效")
        old = self.hub.connections.get(device)
        if old and old is not self:
            await old.close()
        self.device = device
        self.hub.connections[device] = self
        self.scope = self.hub.authority.issue("demo", "default")
        self.tools = DeviceTools(self.request, params.get("fw", "development"), params.get("manifest_hash", ""))
        await self.send(response(rid, {"session": self.id, "proto": 1, "now_ms": int(time.time() * 1000),
                                       "hub_instance": self.hub.instance}))

    async def end_session(self, sid):
        session = self.sessions.pop(sid, None)
        if session:
            session.ended.set()
        # Closing any voice session invalidates its active turns before more output is sent.
        tasks = list(self.turns.values())
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)



class Hub:
    def __init__(self, directory, *, dev_allow_proto1=False, model=None, asr=None, tts=None, request_timeout=3, record_wire=False):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        self.dev_allow_proto1 = dev_allow_proto1
        self.model, self.asr, self.tts = model or FakeModel(), asr or FakeASR(), tts or FakeTTS()
        self.request_timeout = request_timeout
        self.authority = Authority()
        self.authority.add("demo", "demo-adult")
        self.authority.add("demo", "demo-other")
        self.authority.add("demo", "default", "child", ("demo-adult", "demo-other"))
        self.instance = secrets.token_hex(16)
        self.connections = {}
        self.samples = [] if record_wire else None
        self.registry = Registry()
        self.registry.register(Capability("voice.chat", "语音对话", form="session"))
        self.registry.register(Capability("asr.stream", "听写", form="session", expose=frozenset({"app"})))
        self.registry.register(Capability("llm.ask", "示例问答", self.ask))
        self.registry.register(Capability("tts.speak", "示例朗读", self.speak))
        self.registry.register(Capability("asr.transcribe", "示例识别", self.transcribe))
        self.registry.register(Capability("job.status", "任务进度", self.job_status))
        self.jobs = JobQueue(self.directory / "hub.db", self.authority)
        self.metrics = Metrics()
        self.setup = Setup(self.directory)

    async def ask(self, scope, arguments):
        if set(arguments) - {"question"} or not isinstance(arguments.get("question"), str):
            raise ProtocolError("需要问题文字")
        result = await self.model.answer(arguments["question"], [])
        return {"status": "done", "say": next(chunks(result.text), "")}

    async def speak(self, scope, arguments):
        if set(arguments) != {"text"} or not isinstance(arguments["text"], str):
            raise ProtocolError("需要朗读文字")
        packets = await self.tts.synthesize(arguments["text"])
        return {"status": "done", "say": "已生成示例声音。", "result": {"frames": len(packets)}}

    async def transcribe(self, scope, arguments):
        if arguments != {"example": True}:
            raise ProtocolError("此步骤仅接受示例声音")
        return {"status": "done", "result": {"text": await self.asr.transcribe([b"example"])}}

    async def job_status(self, scope, arguments):
        if set(arguments) != {"id"} or not isinstance(arguments["id"], str):
            raise ProtocolError("需要任务编号")
        row = await asyncio.to_thread(self.jobs.get, scope, arguments["id"])
        return {"status": "done", "result": {"state": row["state"] if row else "unknown"}}

    async def device(self, request):
        if request.remote not in ("127.0.0.1", "::1"):
            raise web.HTTPForbidden(text="开发设备只能从本机连接")
        ws = web.WebSocketResponse(max_msg_size=32768, heartbeat=20)
        await ws.prepare(request)
        connection = Connection(self, ws)
        timer = asyncio.get_running_loop().call_later(10, lambda: asyncio.create_task(ws.close()) if not connection.scope else None)
        try:
            async for msg in ws:
                if msg.type in (WSMsgType.TEXT, WSMsgType.BINARY):
                    await connection.receive(msg.data)
        finally:
            timer.cancel()
            await connection.close()
            if self.connections.get(connection.device) is connection:
                self.connections.pop(connection.device)
        return ws

    def application(self):
        app = web.Application(client_max_size=4096)
        app.router.add_get("/v1/device", self.device)
        app.router.add_get("/ping", self.ping)
        app.router.add_get("/healthz", self.ping)
        app.router.add_get("/metrics", self.metric_page)
        app.router.add_get("/setup", self.setup.page)
        app.router.add_post("/setup", self.setup.save)

        async def lifespan(application):
            await asyncio.to_thread(self.jobs.recover)
            monitor = asyncio.create_task(self.metrics.monitor())
            yield
            monitor.cancel()
            await asyncio.gather(monitor, return_exceptions=True)
            await asyncio.gather(*(c.close() for c in list(self.connections.values())))

        app.cleanup_ctx.append(lifespan)
        return app

    async def ping(self, request):
        return web.json_response({"ok": True, "mode": "example"})

    async def metric_page(self, request):
        return web.json_response(self.metrics.snapshot())
