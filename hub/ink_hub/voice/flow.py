import asyncio
import re
import logging
import secrets
from contextlib import aclosing
from aiohttp import ClientError

from ink_hub.bus.policy import authorize
from ink_hub.bus.schema import validate
from ink_hub.gateway.wire import ANSWER_BYTES, ANSWER_CUT, ProtocolError, answer as clip, chunks, frame, response
from .stream import FRAME, LEAD, heard_text, recognize, speak, split_speech


# The model accepts at most 128 functions (DeepSeek). The device sends every capability usable right now
# as "actions", so that list cannot rank them; the page snapshot text ends with "动作：" and the page's own
# capability names. Keep those first, then the current app's domain, then the system's everyday domains,
# then the rest in the device's order.
TOOL_LIMIT = 128
CORE_DOMAINS = ("shell", "settings", "system", "notify", "time", "caps", "ai", "text", "member", "media", "audio")
# Apps whose registry id differs from their capability domain (the reader app is "book").
APP_DOMAINS = {"book": "reader"}


def rank_tools(available, snapshot):
    app = snapshot.get("app") if isinstance(snapshot.get("app"), str) else ""
    domain = APP_DOMAINS.get(app.split(".", 1)[0], app.split(".", 1)[0])
    order = list(dict.fromkeys([*snapshot.get("actions", []), *available]))
    names = [name for name in order if name in available]

    text = snapshot.get("text") if isinstance(snapshot.get("text"), str) else ""
    on_page = text.split("动作：", 1)[1] if "动作：" in text else ""

    def rank(item):
        index, name = item
        head = name.split(".", 1)[0]
        if on_page and name in on_page:
            return (0, index)
        return (1 if domain and head == domain else 2 if head in CORE_DOMAINS else 3, index)

    kept = [name for _, name in sorted(enumerate(names), key=rank)[:TOOL_LIMIT]]
    return {name: available[name] for name in kept}



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
    target, turn_timeout = loop.time() - LEAD, None  # target: when the next frame would be sent at 1x
    first, question, completed = True, "", False
    sentences, speaker, END = asyncio.Queue(), None, object()

    async def pause(delay):
        if delay > 0:
            future = loop.create_future()
            timer = loop.call_later(delay, lambda: future.set_result(None) if not future.done() else None)
            try:
                await future
            finally:
                timer.cancel()

    def fetch(sentence):
        # Start a sentence's speech now; its packets wait in a queue until the one before it has been sent.
        if sentence is None:
            return END
        if isinstance(sentence, asyncio.Future):
            return sentence  # a mark (spoken): no speech of its own
        packets = asyncio.Queue()
        async def run():
            try:
                async with aclosing(speak(self.hub.tts, sentence)) as audio:
                    async for packet in audio:
                        packets.put_nowait(packet)
                packets.put_nowait(None)
            except Exception as error:  # raised where the packets are sent, in the turn
                packets.put_nowait(error)
        return sentence, packets, asyncio.create_task(run())

    async def speak_all():
        # One sentence after another, the next one's speech already being fetched while this one is sent.
        nonlocal count, target
        current = ahead = None
        try:
            while (current := ahead if ahead is not None else fetch(await sentences.get())) is not END:
                ahead = None
                if isinstance(current, asyncio.Future):  # all before it has been sent
                    current.done() or current.set_result(None)
                    continue
                sentence, packets, _ = current
                marker = [sentence, count, None]
                session.spoken.append(marker)
                while (packet := await packets.get()) is not None:
                    if isinstance(packet, Exception):
                        raise packet
                    if ahead is None and not sentences.empty():
                        ahead = fetch(sentences.get_nowait())
                    target = max(target, loop.time() - LEAD)
                    await pause(target - loop.time())
                    await self.send(frame(2, sid, count, packet), guard=guard)
                    count += 1
                    session.sent_frames = count
                    session.times.setdefault("first_audio", loop.time())
                    if turn_timeout is not None and not turn_timeout.expired():
                        turn_timeout.reschedule(min(total_deadline, loop.time() + 10))
                    target += FRAME
                    session.plays_until = target + LEAD
                marker[2] = count
        finally:
            tasks = [item[2] for item in (current, ahead) if isinstance(item, tuple)]
            for task in tasks:
                task.cancel()
            await asyncio.gather(*tasks, return_exceptions=True)

    async def spoken():
        # Before the device acts on a tool call, the words before it are heard first, as when the hub sent at 1x: every
        # sentence so far sent, then the lead played out by the pacing. The device must not turn the page or start
        # something that sounds while the speech introducing it is still queued on the device, or before it.
        if not speaker:
            return
        mark = loop.create_future()
        sentences.put_nowait(mark)
        await asyncio.wait((mark, speaker), return_when=asyncio.FIRST_COMPLETED)
        if speaker.done():
            await speaker  # the speech failed: its error ends the turn now
        await pause(session.plays_until - loop.time())

    async def hush():
        # Stop the speech and what was fetched ahead (abort, error, end of the turn).
        nonlocal speaker, sentences
        if speaker:
            speaker.cancel()
            await asyncio.gather(speaker, return_exceptions=True)
        speaker, sentences = None, asyncio.Queue()

    async def emit(value="", final=False):
        nonlocal seq, sent, text, speech, cut, first, speaker
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
            sentences.put_nowait(sentence)
        if final:
            sentences.put_nowait(None)
        if speaker is None and not sentences.empty():
            speaker = asyncio.create_task(speak_all())
        if speaker and (final or speaker.done()):
            await speaker  # all spoken; or the speech failed: its error ends the turn now

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
            # The device's catalogue (hundreds of tools, many pages) is fetched while the user is still talking;
            # waiting for it before recognition cost seconds on every first turn.
            catalogue = asyncio.create_task(self.tools.current(snapshot.get("actions", []), guard=guard))
            catalogue.add_done_callback(lambda task: task.cancelled() or task.exception())
            try:
                member = params.get("member", {})
                if not isinstance(member, dict):
                    raise ProtocolError("家人信息无效")
                child = self.hub.authority.role(scope) == "child" or member.get("role") == "child"
                if session:
                    question, _ = await recognize(self, sid, session, rid)
                    if not guard():
                        return
                else:
                    question = params.get("question", "")
                available = await catalogue
            finally:
                catalogue.cancel()
            if child:
                available = {n: t for n, t in available.items() if t.get("effect") != "system" and t.get("confirm") != "parent"}
            if not isinstance(question, str):
                raise ProtocolError("问题无效")
            lang = params.get("lang", "zh")
            if not isinstance(lang, str) or not lang.isalpha() or len(lang) > 8:
                raise ProtocolError("语言无效")
            if len(available) > 128:
                logging.getLogger(__name__).info("能力目录截断：%d", len(available) - 128)
                available = rank_tools(available, snapshot)
            context = {"role": "child" if child else "adult", "name": member.get("name", "")}
            if question:
                prompt = question
                if session and self.voice_history:
                    previous, heard = self.voice_history
                    prompt = f"上一轮用户：{previous}\n用户已听到的回答：{heard}\n本轮用户：{question}"
                async with aclosing(self.hub.model.stream(prompt, available, lang=lang, member=context)) as model_stream:
                    async for part in model_stream:
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
                        await spoken()
                        receipt = await self.request("tools/call", {"name": part.tool, "arguments": part.arguments,
                                                                   "page_gen": params.get("page_gen", 0), "_meta": meta}, guard=guard)
                        if not isinstance(receipt, dict) or "status" not in receipt:
                            raise ProtocolError("设备回执无效")
                        self.hub.metrics.tool_calls += 1
                        logging.getLogger(__name__).info("设备操作回执：%s", "完成" if receipt["status"] == "done" else "未完成")
                        await emit(receipt.get("say") or ("操作未完成。" if receipt["status"] != "done" else ""))
                        # A new page needs no refetch here: the definitions are cached for the whole catalogue, and
                        # the next turn fetches again only if its page names a tool the cache does not know.
            await emit(final=True)
            completed = True
    except asyncio.CancelledError:
        return
    except (TimeoutError, ProtocolError, PermissionError, ConnectionError, RuntimeError, ClientError, ExceptionGroup, ValueError):
        await hush()
        if guard():
            try:
                await emit("这次没有完成，请重试。", final=True)
            except (ConnectionError, RuntimeError, TimeoutError, ClientError, ExceptionGroup, ValueError):
                pass
    finally:
        await hush()
        if session:
            logging.getLogger(__name__).info("语音阶段时间：%s", session.times)
        if session and question and self.valid_output(scope, epoch):
            self.voice_history = session.history = (question, text if completed else heard_text(session))
            # Up to the lead of it is still to play after "done"; talking over it then cuts it (Connection.speech_cut).
            self.voice_session = session if completed else None
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
