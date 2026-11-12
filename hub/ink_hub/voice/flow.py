import asyncio
import re
import logging
import secrets

from ink_hub.bus.policy import authorize
from ink_hub.bus.schema import validate
from ink_hub.gateway.wire import ANSWER_BYTES, ANSWER_CUT, ProtocolError, answer as clip, chunks, frame, response


def sentences(text):
    for match in re.finditer(r"[^。！？.!?；;\n]*[。！？.!?；;\n]|[^。！？.!?；;\n]+$", text):
        yield from chunks(match.group())


def validate_photos(attachments):
    if not isinstance(attachments, list) or len(attachments) > 1:
        raise ProtocolError("照片附件无效")
    for photo in attachments:
        if not isinstance(photo, dict):
            raise ProtocolError("照片附件无效")
        width, height, gray = photo.get("width"), photo.get("height"), photo.get("gray4")
        if (type(width) is not int or type(height) is not int or not 0 < width <= 512 or not 0 < height <= 512
                or not isinstance(gray, str) or len(gray) > 65536 or len(gray) != width * height
                or re.fullmatch("[0-9a-f]+", gray) is None):
            raise ProtocolError("照片附件无效")


async def run_turn(self, rid, method, params):
    epoch, scope = self.epoch, self.scope
    sid = params.get("stream", 0)
    session = self.sessions.get(sid)
    guard = lambda: self.valid_output(scope, epoch, sid, session)
    count, seq, sent, text, speech, cut = 0, 0, 0, "", "", False
    loop = asyncio.get_running_loop()
    target, turn_timeout = loop.time() - 0.24, None

    async def emit(value="", final=False):
        nonlocal count, seq, sent, text, speech, cut, target
        if not cut:
            combined = text + value
            text = clip(combined)
            cut = text != combined
        safe = text if final or cut else "".join(chunks(text, total=ANSWER_BYTES - len(ANSWER_CUT.encode())))
        addition = safe[sent:]
        for part in chunks(addition):
            await self.notify("ai.delta", {"id": rid, "text": part, "seq": seq}, guard)
            seq += 1
        sent += len(addition)
        if not session:
            return
        speech += addition
        ready = list(sentences(speech))
        speech = ""
        if ready and not final and not re.search(r"[。！？.!?；;\n]$", ready[-1]):
            speech = ready.pop()
        for sentence in ready:
            audio = await self.hub.tts.synthesize(sentence)
            for packet in audio:
                target = max(target, loop.time() - 0.24)
                delay = target - loop.time()
                if delay > 0:
                    future = loop.create_future()
                    timer = loop.call_later(delay, lambda: future.set_result(None) if not future.done() else None)
                    try:
                        await future
                    finally:
                        timer.cancel()
                await self.send(frame(2, sid, count, packet), guard=guard)
                count += 1
                if turn_timeout is not None and not turn_timeout.expired():
                    turn_timeout.reschedule(min(total_deadline, loop.time() + 10))
                target += 0.06

    try:
        if not guard():
            return
        budget = params.get("budget_ms", 30000)
        if type(budget) is not int or budget < 0:
            raise ProtocolError("剩余时间无效")
        loop = asyncio.get_running_loop()
        deadline = loop.time() + max(0, min(budget, 45000) - 3000) / 1000
        total_deadline = loop.time() + 597  # device's ten-minute limit, less transport margin
        async with asyncio.timeout_at(deadline) as turn_timeout:
            snapshot = params.get("page", {})
            if not isinstance(snapshot, dict) or not isinstance(snapshot.get("actions", []), list):
                raise ProtocolError("页面快照无效")
            if any(not isinstance(n, str) or len(n.encode()) > 47 for n in snapshot.get("actions", [])):
                raise ProtocolError("能力名无效")
            available = await self.tools.current(snapshot.get("actions", []), guard=guard)
            member = params.get("member", {})
            if not isinstance(member, dict):
                raise ProtocolError("家人信息无效")
            child = self.hub.authority.role(scope) == "child" or member.get("role") == "child"
            if child:
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
            if len(available) > 128:
                logging.getLogger(__name__).info("能力目录截断：%d", len(available) - 128)
                order = list(dict.fromkeys([*snapshot.get("actions", []), *available]))
                available = {name: available[name] for name in order if name in available}
                available = dict(list(available.items())[:128])
            context = {"role": "child" if child else "adult", "name": member.get("name", "")}
            if question:
                async for part in self.hub.model.stream(question, available, lang=lang, member=context):
                    if not guard():
                        return
                    await emit(part.text)
                    if not part.tool:
                        continue
                    descriptor = available.get(part.tool)
                    if not descriptor or not isinstance(part.arguments, dict) or "_meta" in part.arguments:
                        raise ProtocolError("工具参数无效")
                    validate(part.arguments, descriptor.get("inputSchema", {"type": "object"}))
                    meta = authorize(self.hub.authority, scope, epoch=epoch, connection=self.id,
                                     action=part.tool, arguments=part.arguments, now=loop.time(),
                                     effect=descriptor.get("effect", "read"))
                    meta["request_id"] = secrets.token_hex(16)
                    receipt = await self.request("tools/call", {"name": part.tool, "arguments": part.arguments,
                                                               "page_gen": params.get("page_gen", 0), "_meta": meta}, guard=guard)
                    if not isinstance(receipt, dict) or "status" not in receipt:
                        raise ProtocolError("设备回执无效")
                    self.hub.metrics.tool_calls += 1
                    await emit(receipt.get("say") or ("操作未完成。" if receipt["status"] != "done" else ""))
                    if receipt.get("page_gen", params.get("page_gen")) != params.get("page_gen"):
                        await self.tools.current([], guard=guard)
            await emit(final=True)
    except asyncio.CancelledError:
        return
    except (TimeoutError, ProtocolError, PermissionError, ConnectionError, RuntimeError):
        if guard():
            try:
                await emit("这次没有完成，请重试。", final=True)
            except (ConnectionError, RuntimeError, TimeoutError):
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
