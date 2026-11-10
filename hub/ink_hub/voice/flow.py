import asyncio
import secrets

from ink_hub.bus.policy import authorize
from ink_hub.bus.schema import validate
from ink_hub.gateway.wire import ANSWER_BYTES, ProtocolError, chunks, frame, response


async def run_turn(self, rid, method, params):
    epoch, scope = self.epoch, self.scope
    sid = params.get("stream", 0)
    session = self.sessions.get(sid)
    guard = lambda: self.valid_output(scope, epoch, sid, session)
    count = 0
    try:
        if not guard():
            return
        budget = params.get("budget_ms", 30000)
        if type(budget) is not int or budget < 0:
            raise ProtocolError("剩余时间无效")
        deadline = asyncio.get_running_loop().time() + max(0, min(budget, 45000) - 3000) / 1000
        async with asyncio.timeout_at(deadline):
            snapshot = params.get("page", {})
            if not isinstance(snapshot, dict) or not isinstance(snapshot.get("actions", []), list):
                raise ProtocolError("页面快照无效")
            if any(not isinstance(n, str) or len(n.encode()) > 47 for n in snapshot.get("actions", [])):
                raise ProtocolError("能力名无效")
            available = await self.tools.current(snapshot.get("actions", []), guard=guard)
            if self.hub.authority.role(scope) == "child":
                available = {n: t for n, t in available.items() if t.get("effect") != "system" and t.get("confirm") != "parent"}
            if session:
                await session.ended.wait()
                question = await self.hub.asr.transcribe(session.frames)
                session.frames.clear()
                if not guard():
                    return
                transcript = next(chunks(question), "")
                await self.notify("ai.stt", {"id": rid, "text": transcript}, guard)
            else:
                question = params.get("question", "")
            if not isinstance(question, str):
                raise ProtocolError("问题无效")
            lang = params.get("lang", "zh")
            if not isinstance(lang, str) or not lang.isalpha() or len(lang) > 8:
                raise ProtocolError("语言无效")
            # Chinese is the default; another language is passed so the model answers in it.
            extra = {"lang": lang} if lang != "zh" else {}
            answer = await self.hub.model.answer(question, list(available), **extra) if question else None
            if not guard():
                return
            text = answer.text if answer else ""
            if answer and answer.tool:
                descriptor = available[answer.tool]
                if not isinstance(answer.arguments, dict) or "_meta" in answer.arguments:
                    raise ProtocolError("工具参数无效")
                validate(answer.arguments, descriptor.get("inputSchema", {"type": "object"}))
                meta = authorize(self.hub.authority, scope, epoch=epoch, connection=self.id,
                                 action=answer.tool, arguments=answer.arguments, now=asyncio.get_running_loop().time(),
                                 effect=descriptor.get("effect", "read"))
                meta["request_id"] = secrets.token_hex(16)
                receipt = await self.request("tools/call", {"name": answer.tool, "arguments": answer.arguments,
                                                           "page_gen": params.get("page_gen", 0), "_meta": meta}, guard=guard)
                if not isinstance(receipt, dict) or "status" not in receipt:
                    raise ProtocolError("设备回执无效")
                self.hub.metrics.tool_calls += 1
                text = receipt.get("say") or ("操作未完成。" if receipt["status"] != "done" else text)
                if receipt.get("page_gen", params.get("page_gen")) != params.get("page_gen"):
                    await self.tools.current([], guard=guard)
            for seq, part in enumerate(chunks(text, total=ANSWER_BYTES)):
                await self.notify("ai.delta", {"id": rid, "text": part, "seq": seq}, guard)
            if session:
                audio = await self.hub.tts.synthesize("".join(chunks(text, total=ANSWER_BYTES)))
                if len(audio) > 450:
                    raise ProtocolError("朗读超过一轮预算")
                start = asyncio.get_running_loop().time()
                for seq, packet in enumerate(audio):
                    # Five-frame prefill, then real-time pacing via the loop timer.
                    target = start + max(0, seq - 4) * 0.06
                    delay = target - asyncio.get_running_loop().time()
                    if delay > 0:
                        future = asyncio.get_running_loop().create_future()
                        timer = asyncio.get_running_loop().call_later(delay, lambda: future.set_result(None) if not future.done() else None)
                        try:
                            await future
                        finally:
                            timer.cancel()
                    await self.send(frame(2, sid, seq, packet), guard=guard)
                    count += 1
    except asyncio.CancelledError:
        return
    except (TimeoutError, ProtocolError, PermissionError, ConnectionError, RuntimeError):
        if guard():
            try:
                await self.notify("ai.delta", {"id": rid, "text": "这次没有完成，请重试。"}, guard)
            except ConnectionError:
                pass
    finally:
        if guard() and not asyncio.current_task().cancelling():
            try:
                if session:
                    await self.notify("ai.tts.end", {"id": rid, "stream": sid, "frames": count}, guard)
                await self.send(response(rid, {"done": True}), guard=guard)
                self.hub.metrics.turns += 1
            except ConnectionError:
                pass

async def run_dictation(self, sid, session):
    scope = self.scope
    guard = lambda: self.valid_output(scope, session.epoch, sid, session)
    if not guard():
        return
    try:
        text = await self.hub.asr.transcribe(session.frames)
        session.frames.clear()
        if not guard():
            return
        parts = list(chunks(text, limit=480))
        for seg, part in enumerate(parts):
            await self.notify("rt.asr", {"sid": sid, "seg": seg, "final": True, "text": part}, guard)
        await self.notify("rt.asr.done", {"sid": sid, "segs": len(parts), "ms": session.seq * 60}, guard)
    except (asyncio.CancelledError, ConnectionError):
        return
