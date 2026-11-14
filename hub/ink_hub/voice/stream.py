"""XiaoZhi receive/abort/first-sentence flow, adapted to the existing gateway.

Copyright (c) 2025 xinnan-tech. MIT; see speech/XIAOZHI-LICENSE.
"""
import asyncio
import os
import time
from contextlib import aclosing

from ink_hub.gateway.wire import chunks
from ink_hub.speech.vad import VoiceActivity

FRAME = .06  # one Opus frame of speech
# Speech kept ahead of the device's playback: it starts at its own prebuffer, so the first sound is no later, and what
# arrives early fills its jitter buffer to this depth; a downlink stall shorter than this is not heard (4G retransmits
# hold up to about 0.6 s).
LEAD = 1.2


def split_speech(text, first=True, final=False):
    start = 0
    for i, char in enumerate(text):
        if char in "。！？.!?；;\n" or (first and char in "，," and i - start >= 8):
            yield text[start:i + 1]
            start, first = i + 1, False
    if final and start < len(text):
        yield text[start:]


async def packets(session):
    while True:
        session.ready.clear()
        while session.frames:
            yield session.frames.pop(0)
        if session.ended.is_set():
            return
        await session.ready.wait()


async def recognize(connection, sid, session, rid=None):
    asr, text, seg, last = connection.hub.asr, "", 0, None
    loop = asyncio.get_running_loop()
    guard = lambda: connection.valid_output(connection.scope, session.epoch, sid, session)

    async def audio():
        vad = VoiceActivity(os.environ.get("INK_HUB_VAD_MODEL"))
        with asr.opus.decoder() as decoder:
            async for packet in packets(session):
                pcm = asr.opus.decode(decoder, packet)
                stop = vad.feed(pcm)
                if stop:
                    session.ended.set()
                    session.times.setdefault("speech_end", loop.time())
                yield pcm
                if stop:
                    return

    if not hasattr(asr, "stream"):
        await session.ended.wait()
        value = await asr.transcribe(session.frames)
        session.frames.clear()
        results = [(value, True)]
        async def legacy():
            for result in results:
                yield result
        source = legacy()
    else:
        source = asr.stream(audio())
    async with aclosing(source):
        async for value, final in source:
            if not guard():
                raise ConnectionError("连接或家人已变化")
            now = loop.time()
            if not final and (not session.interim_ms or last is not None and now - last < session.interim_ms / 1000):
                continue
            last = now
            if rid is not None:
                if final:
                    text += value
                await connection.notify("ai.stt", {"id": rid, "text": next(chunks(text if final else text + value), "")}, guard)
            else:
                parts = list(chunks(value, limit=480))
                for offset, part in enumerate(parts):
                    await connection.notify("rt.asr", {"sid": sid, "seg": seg + offset, "final": final, "text": part}, guard)
                if final:
                    seg += len(parts)
                    text += value
            if final:
                session.times["asr_final"] = now
    return text, seg


async def speak(tts, text):
    if hasattr(tts, "stream"):
        async with aclosing(tts.stream(text)) as source:
            async for packet in source:
                yield packet
    else:
        for packet in await tts.synthesize(text):
            yield packet


def heard_text(session, now=None):
    # The device's count of played frames when it sends one; otherwise the frames due by now (the hub sent them LEAD
    # ahead of their playing time, so up to LEAD of them may still wait in the device's buffer).
    heard = session.heard_frames
    if heard is None:
        ahead = max(0., session.plays_until - (time.monotonic() if now is None else now))
        heard = max(0, session.sent_frames - round(ahead / FRAME))
    heard = min(heard, session.sent_frames)
    answer = ""
    for text, start, end in session.spoken:
        count = max(0, heard - start)
        size = len(text) if end is not None and heard >= end else int(count * .06 * 5)
        answer += text[:size]
    return answer
