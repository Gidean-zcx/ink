import asyncio
import json
import logging
import secrets
import time
from dataclasses import dataclass, field
from pathlib import Path

from aiohttp import WSMsgType, web

from ink_hub.agent.tools import DeviceTools
from ink_hub.bus.registry import Capability, Registry, object_schema
from ink_hub.content.weather import Weather
from ink_hub.console.setup import Setup
from ink_hub.data.scope import Authority
from ink_hub.data.pairing import PairingStore
from ink_hub.jobs.queue import JobQueue
from ink_hub.jobs.runner import Runner
from ink_hub.models.fake import FakeModel
from ink_hub.remote.input import handle_input
from ink_hub.ops.metrics import Metrics
from ink_hub.speech.fake import FakeASR, FakeTTS
import ink_hub.speech
from ink_hub.voice.flow import run_turn, run_dictation, validate_photos
from ink_hub.voice.stream import heard_text
from .wire import (ProtocolError, chunks, dumps, encode, frame,
                   notification, page, parse_frame, response)
from .tls import loopback, proto_allowed
from .pairing import Pairing


@dataclass
class Session:
    cap: str
    epoch: int
    frames: list = field(default_factory=list)
    size: int = 0
    seq: int = 0
    browser: object = None
    ended: asyncio.Event = field(default_factory=asyncio.Event)
    ready: asyncio.Event = field(default_factory=asyncio.Event)
    interim_ms: int = 800
    times: dict = field(default_factory=dict)
    spoken: list = field(default_factory=list)
    sent_frames: int = 0
    heard_frames: int | None = None
    plays_until: float = 0.  # loop time the last frame sent finishes playing (hub pacing)
    history: tuple | None = None


class Connection:
    def __init__(self, hub, ws, remote="127.0.0.1", secure=False):
        self.hub, self.ws = hub, ws
        self.remote, self.secure = remote, secure
        self.proto, self.pair_code = 1, None
        self.id = secrets.token_hex(8)
        self.device = None
        self.scope = None
        self.epoch = 0
        self.authed = False
        self.closed = False
        self.sessions = {}
        self.turns = {}
        self.calls = {}
        self.turn_sessions = {}
        self.voice_history = None
        self.voice_session = None  # the last answered voice turn, for what of it was heard (speech_cut)
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
                    if done.cancelled():
                        continue
                    if guard is not None and not guard():
                        done.set_exception(ConnectionError("连接或家人已变化"))
                        continue
                    if isinstance(data, bytes):
                        await self.ws.send_bytes(data)
                    else:
                        await self.ws.send_str(data)
                        if self.hub.samples is not None:
                            self.hub.samples.append(data)
                    if not done.done():
                        done.set_result(None)
                except asyncio.CancelledError:
                    if not done.done():
                        done.set_exception(ConnectionError("连接已关闭"))
                    raise
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
        scope, epoch = self.scope, self.epoch
        valid = lambda: (not self.closed and (scope is None or self.valid_output(scope, epoch))
                         and (guard is None or guard()))
        self.sequence += 1
        try:
            await self.queue.put((priority, self.sequence, data, valid, done))
            await done
        finally:
            # Cancellation invalidates queued output, even before awaiting its receipt.
            if not done.done():
                done.cancel()
            elif not done.cancelled():
                done.exception()

    async def notify(self, method, params, guard=None):
        await self.send(notification(method, params), guard=guard)

    async def request(self, method, params, timeout=None, *, guard=None):
        scope, epoch = self.scope, self.epoch
        valid = lambda: self.valid_output(scope, epoch) and (guard is None or guard())
        if scope is None or not valid():
            raise ConnectionError("连接或家人已变化")
        self.rid += 1
        rid = self.rid
        future = asyncio.get_running_loop().create_future()
        self.pending[rid] = future
        try:
            await self.send({"jsonrpc": "2.0", "id": rid, "method": method, "params": params}, guard=valid)
            result = await asyncio.wait_for(future, timeout if timeout is not None else self.hub.request_timeout)
            # wait_for may return a concurrently completed future despite task cancellation.
            if asyncio.current_task().cancelling():
                raise asyncio.CancelledError
            if not valid():
                raise ConnectionError("连接或家人已变化")
            return result
        finally:
            self.pending.pop(rid, None)
            if not future.done():
                future.cancel()
            elif not future.cancelled():
                future.exception()

    async def error(self, rid, code, message, *, guard=None):
        await self.send({"jsonrpc": "2.0", "id": rid, "error": {"code": code, "message": message}}, guard=guard)

    async def call_tool(self, rid, params, scope, epoch):
        guard = lambda: self.valid_output(scope, epoch)
        try:
            if not guard():
                return
            receipt = await self.hub.registry.call(scope, params.get("name"), params.get("arguments", {}), self.hub.authority)
            await self.send(response(rid, {"content": [{"type": "text", "text": receipt.get("say", "")}],
                                           "structuredContent": receipt, "isError": receipt["status"] != "done"}), guard=guard)
        except (ProtocolError, PermissionError, ValueError, TypeError, KeyError,
                TimeoutError, ConnectionError, RuntimeError) as exc:
            if not guard() or isinstance(exc, ConnectionError):
                return
            code = exc.code if isinstance(exc, ProtocolError) else -32602
            message = str(exc) if isinstance(exc, (ProtocolError, PermissionError)) else "参数无效"
            if isinstance(exc, (TimeoutError, RuntimeError)):
                code, message = -32603, "这次没有完成，请重试。"
            try:
                await self.error(rid, code, message, guard=guard)
            except ConnectionError:
                pass

    async def weather(self, rid, method, params, scope, epoch):
        guard = lambda: self.valid_output(scope, epoch)
        try:
            handler = self.hub.weather.find if method == "weather/find" else self.hub.weather.get
            reply = response(rid, await handler(params))
        except ProtocolError as exc:
            reply = {"jsonrpc": "2.0", "id": rid, "error": {"code": exc.code, "message": str(exc)}}
        try:
            await self.send(reply, guard=guard)
        except ConnectionError:
            pass

    async def cancel_work(self):
        tasks = list(self.turns.values()) + list(self.calls.values())
        self.turns.clear()
        self.calls.clear()
        self.turn_sessions.clear()
        self.voice_history = self.voice_session = None
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
                done.set_exception(ConnectionError("连接已关闭"))
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
                if session.cap == "browser.session" and kind == 4 and session.browser is not None:
                    self.hub.authority.check(self.scope)
                    if self.hub.authority.role(self.scope) == "child":
                        await self.end_session(sid)
                        await self.notify("rt.nav", {"sid": sid, "blocked": True})
                        return
                    await handle_input(session.browser, payload)
                    return
                if session.ended.is_set():
                    return
                if kind != 1 or seq != session.seq or session.size + len(payload) > 128 * 1024:
                    await self.end_session(sid)
                    await self.notify("rt.close", {"sid": sid})
                    return
                session.seq = (seq + 1) & 65535
                session.frames.append(payload)
                session.size += len(payload)
                session.ready.set()
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
                if method.startswith("pair/"):
                    await self.hub.pairing.receive(self, rid, method, params)
                    return
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
                if rid is None or rid in self.calls or rid in self.turns:
                    raise ProtocolError("这一轮编号无效")
                if len(self.calls) + len(self.turns) >= 4:
                    raise ProtocolError("正在处理的请求已满", -32022)
                task = asyncio.create_task(self.call_tool(rid, params, self.scope, self.epoch))
                self.calls[rid] = task
                task.add_done_callback(lambda t, key=rid: self.calls.pop(key, None) if self.calls.get(key) is t else None)
            elif method == "text/suggest":
                if set(params) != {"type", "prefix"} or params["type"] != "plain" or not isinstance(params["prefix"], str) or not 1 <= len(params["prefix"]) <= 500:
                    raise ProtocolError("接话参数无效")
                await self.send(response(rid, {"available": False, "suggestions": []}))
            elif method in ("weather/find", "weather/get"):
                if rid is None or rid in self.calls or len(self.calls) + len(self.turns) >= 4:
                    raise ProtocolError("正在处理的请求已满", -32022)
                task = asyncio.create_task(self.weather(rid, method, params, self.scope, self.epoch))
                self.calls[rid] = task
                task.add_done_callback(lambda t, key=rid: self.calls.pop(key, None) if self.calls.get(key) is t else None)
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
                interim = pp.get("interim_ms", 800)
                if type(interim) is not int or not 0 <= interim <= 10000:
                    raise ProtocolError("中间结果间隔无效")
                if cap == "voice.chat":
                    self.speech_cut()
                self.sessions[sid] = Session(cap, self.epoch, interim_ms=interim)
                await self.send(response(rid, {"sid": sid}))
                if cap == "asr.stream":
                    task = asyncio.create_task(run_dictation(self, sid, self.sessions[sid]))
                    self.turns[f"asr:{sid}"] = task
                    self.turn_sessions[f"asr:{sid}"] = sid
                    task.add_done_callback(lambda t, key=f"asr:{sid}": self.forget_turn(key))
            elif method == "rt.close":
                session = self.sessions.get(params.get("sid"))
                heard = params.get("heard_frames")
                if session and type(heard) is int and heard >= 0:
                    session.heard_frames = heard
                await self.end_session(params.get("sid"), params.get("reason") in ("barge_in", "dismiss", "cancel"))
            elif method == "ai.replay":
                raise ProtocolError("unavailable", -32021)
            elif method in ("ai.listen", "ai.ask"):
                validate_photos(params.get("attachments", []))
                if rid is None or rid in self.turns or rid in self.calls:
                    raise ProtocolError("这一轮编号无效")
                sid = params.get("stream", 0)
                if method == "ai.listen" and (sid not in self.sessions or self.sessions[sid].cap != "voice.chat"):
                    raise ProtocolError("语音会话未打开")
                if len(self.turns) + len(self.calls) >= 4:
                    raise ProtocolError("正在处理的请求已满", -32022)
                task = asyncio.create_task(run_turn(self, rid, method, params))
                self.turns[rid] = task
                self.turn_sessions[rid] = sid
                task.add_done_callback(lambda t, key=rid: self.forget_turn(key))
            elif method == "ai.listen.end":
                session = self.sessions.get(params.get("stream"))
                if session:
                    session.times.setdefault("speech_end", asyncio.get_running_loop().time())
                    session.ended.set()
                    session.ready.set()
            elif method == "rt.turn":
                sid = params.get("sid")
                session = self.sessions.get(sid)
                if session and session.cap == "asr.stream" and params.get("op") == "end":
                    session.times.setdefault("speech_end", asyncio.get_running_loop().time())
                    session.ended.set()
                    session.ready.set()
            elif method == "ink/member":
                if self.proto == 2:
                    if type(params.get("epoch")) is not int or params["epoch"] <= self.epoch:
                        raise ProtocolError("家人代次无效")
                    scope, authed, epoch = self.member_scope(params)
                    self.scope, self.authed, self.epoch = scope, authed, epoch
                    await self.cancel_work()
                    await self.notify("caps/changed", {})
                    return
                # Only synthetic configured identities exist in proto1 development mode.
                member, epoch = params.get("member"), params.get("epoch")
                if (type(epoch) is not int or not self.epoch < epoch <= 0xffffffff or
                        type(params.get("authed", False)) is not bool or member not in ("default", "demo-adult", "demo-other")):
                    raise ProtocolError("家人或代次无效")
                scope = self.hub.authority.issue("demo", member)
                # Proto1 cannot prove adult unlock; it never grants an adult space.
                if self.hub.authority.role(scope) == "adult":
                    raise ProtocolError("开发握手不能验证大人密码", -32020)
                self.scope, self.epoch = scope, epoch
                self.authed = False
                await self.cancel_work()
                await self.notify("caps/changed", {})
            elif method == "device/event":
                boot, seq, member = params.get("boot"), params.get("seq"), params.get("member")
                if (type(boot) is not int or not 0 < boot <= 0xffffffff or
                        type(seq) is not int or not 0 < seq <= 0xffffffff or member != self.scope.member or
                        not isinstance(params.get("event"), str) or not isinstance(params.get("args"), dict)):
                    raise ProtocolError("事件标识无效")
                # Only transport position is retained here; event bodies are synthetic and transient.
                old_boot, old_seq = self.hub.event_positions.get(self.device, (boot, 0))
                if boot < old_boot:
                    raise ProtocolError("事件启动序号已过期")
                if boot != old_boot or seq > old_seq + 1:
                    self.hub.metrics.event_gaps += 1
                upto = max(seq, old_seq) if boot == old_boot else seq
                self.hub.event_positions[self.device] = boot, upto
                await self.notify("device/event_ack", {"boot": boot, "upto": upto})
            else:
                raise ProtocolError("未提供这个方法", -32601)
        except (ProtocolError, PermissionError, ValueError, TypeError, KeyError) as exc:
            if rid is not None:
                await self.error(rid, exc.code if isinstance(exc, ProtocolError) else -32602,
                                 str(exc) if isinstance(exc, (ProtocolError, PermissionError)) else "参数无效")

    async def hello(self, rid, params):
        proto = params.get("proto")
        if rid is None or type(proto) is not int or not proto_allowed(proto, self.remote, self.secure, self.hub.dev_allow_proto1):
            raise ProtocolError("只开放显式启用的开发握手", -32012)
        device = params.get("device")
        if not isinstance(device, str) or not 1 <= len(device.encode()) <= 64:
            raise ProtocolError("设备名称无效")
        epoch = params.get("epoch", 0)
        if type(epoch) is not int or not 0 <= epoch <= 0xffffffff or type(params.get("authed", False)) is not bool:
            raise ProtocolError("家人代次无效")
        if proto == 2:
            token = params.get("token")
            if not isinstance(token, str):
                raise ProtocolError("auth_invalid", -32010)
            self.hub.tokens.authenticate(device, token)
            scope, authed, epoch = self.member_scope(params)
        else:
            scope, authed = self.hub.authority.issue("demo", "default"), False
        old = self.hub.connections.get(device)
        if old and old is not self:
            await old.close()
        self.device = device
        self.hub.connections[device] = self
        self.scope, self.authed, self.epoch, self.proto = scope, authed, epoch, proto
        entry = self.hub.pairing.codes.pop(self.pair_code, None)
        if entry:
            entry["timer"].cancel()
        self.tools = DeviceTools(self.request, params.get("fw", "development"), params.get("manifest_hash", ""))
        await self.send(response(rid, {"session": self.id, "proto": proto, "now_ms": int(self.hub.clock() * 1000),
                                       "hub_instance": self.hub.instance}))
        logging.getLogger(__name__).info("设备已连接：协议 %d", proto)

    def member_scope(self, params):
        member, authed, epoch = params.get("member"), params.get("authed", False), params.get("epoch", 0)
        if (not isinstance(member, str) or not 1 <= len(member.encode()) <= 15 or type(authed) is not bool
                or type(epoch) is not int or not 0 <= epoch <= 0xffffffff):
            raise ProtocolError("家人或代次无效")
        authority, role = self.hub.authority, "adult" if authed else "child"
        old = authority._members.get(("home", member))
        if not old or old[0] != role:
            authority.add("home", member, role)
        return authority.issue("home", member), authed, epoch

    async def end_session(self, sid, interrupted=False):
        session = self.sessions.pop(sid, None)
        if session:
            session.ended.set()
        tasks = [task for key, task in self.turns.items() if self.turn_sessions.get(key) == sid]
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        if (session and session.history is not None and self.voice_history is session.history
                and (interrupted or session.heard_frames is not None)):
            self.voice_history = (session.history[0], heard_text(session))

    def speech_cut(self):
        # A new voice turn stops the answer still playing on the device (talking over it, ai_service's
        # ink_audio_stop_all). The hub sends up to the lead ahead, so after its last frame and "done" (when the device
        # closes the session, without a reason or a count) part of the answer may not have played yet: what was heard
        # is what the pacing says had played when the new turn began, not the whole answer.
        last, self.voice_session = self.voice_session, None
        now = asyncio.get_running_loop().time()
        if last and last.history is not None and self.voice_history is last.history and now < last.plays_until:
            self.voice_history = last.history = (last.history[0], heard_text(last, now))

    def forget_turn(self, key):
        self.turns.pop(key, None)
        self.turn_sessions.pop(key, None)

    def valid_output(self, scope, epoch, sid=0, session=None):
        if scope is None:
            return False
        try:
            self.hub.authority.check(scope)
        except PermissionError:
            return False
        return (not self.closed and self.epoch == epoch and self.scope == scope
                and (session is None or self.sessions.get(sid) is session))



class Hub:
    def __init__(self, directory, *, dev_allow_proto1=False, model=None, asr=None, tts=None, request_timeout=3, record_wire=False, weather_source=None, lan=None, clock=time.time):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        self.dev_allow_proto1 = dev_allow_proto1
        self.lan, self.clock = lan, clock
        self.tokens = PairingStore(self.directory / "hub.db", clock)
        self.pairing = Pairing(self)
        factory = getattr(ink_hub.speech, "from_environment", None)
        providers = factory() if factory else None
        if providers:
            asr, tts = asr or providers[0], tts or providers[1]
        self.model, self.asr, self.tts = model or FakeModel(), asr or FakeASR(), tts or FakeTTS()
        self.request_timeout = request_timeout
        self.weather = Weather(weather_source)
        self.authority = Authority()
        self.authority.add("demo", "demo-adult")
        self.authority.add("demo", "demo-other")
        self.authority.add("demo", "default", "child", ("demo-adult", "demo-other"))
        self.instance = secrets.token_hex(16)
        self.connections = {}
        self.event_positions = {}
        self.samples = [] if record_wire else None
        self.registry = Registry()
        self.registry.register(Capability("voice.chat", "语音对话", form="session"))
        self.registry.register(Capability("asr.stream", "听写", form="session", expose=frozenset({"app"})))
        self.registry.register(Capability("llm.ask", "示例问答", self.ask,
                                           input_schema=object_schema({"question": {"type": "string"}})))
        self.registry.register(Capability("tts.speak", "示例朗读", self.speak,
                                           input_schema=object_schema({"text": {"type": "string"}})))
        self.registry.register(Capability("asr.transcribe", "示例识别", self.transcribe,
                                           input_schema=object_schema({"example": {"type": "boolean", "enum": [True]}})))
        self.registry.register(Capability("job.status", "任务进度", self.job_status,
                                           input_schema=object_schema({"id": {"type": "string"}})))
        self.jobs = JobQueue(self.directory / "hub.db", self.authority)
        self.runner = Runner(self.jobs)
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
        if not request.secure and not loopback(request.remote):
            raise web.HTTPForbidden(text="开发设备只能从本机连接")
        ws = web.WebSocketResponse(max_msg_size=32768, heartbeat=20)
        await ws.prepare(request)
        connection = Connection(self, ws, request.remote, request.secure)
        timer = asyncio.get_running_loop().call_later(10, lambda: asyncio.create_task(ws.close()) if not connection.scope and not connection.pair_code else None)
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

    def tls_application(self):
        app = web.Application(client_max_size=4096)
        app.router.add_get("/v1/device", self.device)
        return app

    def application(self):
        app = web.Application(client_max_size=4096)
        app.router.add_get("/v1/device", self.device)
        app.router.add_get("/ping", self.ping)
        app.router.add_get("/healthz", self.ping)
        app.router.add_get("/metrics", self.metric_page)
        app.router.add_get("/setup", self.setup.page)
        app.router.add_post("/setup", self.setup.save)
        app.router.add_post("/claim", self.pairing.claim)

        async def lifespan(application):
            await asyncio.to_thread(self.jobs.recover)
            monitor = asyncio.create_task(self.metrics.monitor())
            jobs = asyncio.create_task(self.runner.run())
            yield
            monitor.cancel()
            jobs.cancel()
            await asyncio.gather(monitor, jobs, return_exceptions=True)
            await asyncio.gather(*(c.close() for c in list(self.connections.values())))
            for entry in self.pairing.codes.values():
                entry["timer"].cancel()

        app.cleanup_ctx.append(lifespan)
        return app

    async def ping(self, request):
        return web.json_response({"ok": True, "mode": "example"})

    async def metric_page(self, request):
        return web.json_response(self.metrics.snapshot())
