import asyncio
import re
import logging
import secrets
from contextlib import aclosing
from aiohttp import ClientError

from ink_hub.bus.policy import authorize
from ink_hub.bus.schema import validate
from ink_hub.gateway.wire import ANSWER_BYTES, ANSWER_CUT, ProtocolError, answer as clip, chunks, frame, response
from .stream import heard_text, recognize, speak, split_speech


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
    first, question, completed = True, "", False

    async def emit(value="", final=False):
        nonlocal count, seq, sent, text, speech, cut, target, first
        if not cut:
            combined = text + value
            text = clip(combined)
            cut = text != combined
        safe = text if final or cut else "".join(chunks(text, total=ANSWER_BYTES - len(ANSWER_CUT.encode())))
        addition = safe[sent:]
        for part in chunks(addition):
            await self.notify("ai.delta", {"id": rid, "text": part, "seq": seq}, guard)
            if session:
                session.times.setdefault("first_text", loop.time())
            seq += 1
        sent += len(addition)
        if not session:
            return
        speech += addition
        ready = list(split_speech(speech, first, final))
        speech = speech[sum(map(len, ready)):]
        for sentence in (part for value in ready for part in chunks(value)):
            first = False
            session.times.setdefault("first_sentence", loop.time())
            marker = [sentence, count, None]
            session.spoken.append(marker)
            async with aclosing(speak(self.hub.tts, sentence)) as audio:
                async for packet in audio:
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
                    session.sent_frames = count
                    session.times.setdefault("first_audio", loop.time())
                    if turn_timeout is not None and not turn_timeout.expired():
                        turn_timeout.reschedule(min(total_deadline, loop.time() + 10))
                    target += 0.06
            marker[2] = count

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
                question, _ = await recognize(self, sid, session, rid)
                if not guard():
                    return
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
                prompt = question
                if session and self.voice_history:
                    previous, heard = self.voice_history
                    prompt = f"上一轮用户：{previous}\n用户已听到的回答：{heard}\n本轮用户：{question}"
                async for part in self.hub.model.stream(prompt, available, lang=lang, member=context):
                    if not guard():
                        return
                    if session and part.text:
                        session.times.setdefault("model_first_text", loop.time())
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
                    logging.getLogger(__name__).info("设备操作回执：%s", "完成" if receipt["status"] == "done" else "未完成")
                    await emit(receipt.get("say") or ("操作未完成。" if receipt["status"] != "done" else ""))
                    if receipt.get("page_gen", params.get("page_gen")) != params.get("page_gen"):
                        await self.tools.current([], guard=guard)
            await emit(final=True)
            completed = True
    except asyncio.CancelledError:
        return
    except (TimeoutError, ProtocolError, PermissionError, ConnectionError, RuntimeError, ClientError, ExceptionGroup, ValueError):
        if guard():
            try:
                await emit("这次没有完成，请重试。", final=True)
            except (ConnectionError, RuntimeError, TimeoutError):
                pass
    finally:
        if session:
            logging.getLogger(__name__).info("语音阶段时间：%s", session.times)
        if session and question and self.valid_output(scope, epoch):
            self.voice_history = (question, text if completed else heard_text(session))
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
        text, segs = await recognize(self, sid, session)
        if not guard():
            return
        await self.notify("rt.asr.done", {"sid": sid, "segs": segs, "ms": session.seq * 60}, guard)
    except (asyncio.CancelledError, ConnectionError):
        return
    except (RuntimeError, TimeoutError, ClientError, ExceptionGroup, ValueError):
        if guard():
            await self.notify("rt.asr.error", {"sid": sid, "code": "vendor", "message": "语音识别没有完成，请重试。"}, guard)
            await self.notify("rt.close", {"sid": sid}, guard)
