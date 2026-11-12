"""Shared cancellation and PCM framing for the XiaoZhi vendor adapters."""
import asyncio
from contextlib import asynccontextmanager

from aiohttp import ClientSession, ClientTimeout

from .opus import FRAME_BYTES


@asynccontextmanager
async def duplex(ws, audio, send, finish):
    async def upload():
        async for pcm in audio:
            await send(pcm)
        await finish()
    async with asyncio.TaskGroup() as group:
        task = group.create_task(upload())
        try:
            yield ws
        finally:
            task.cancel()


def client():
    return ClientSession(timeout=ClientTimeout(total=20, sock_connect=3))


async def encode_pcm(opus, audio):
    pending = bytearray()
    with opus.encoder() as encoder:
        async for pcm in audio:
            pending.extend(pcm)
            while len(pending) >= FRAME_BYTES:
                yield opus.encode(encoder, bytes(pending[:FRAME_BYTES]))
                del pending[:FRAME_BYTES]
        if pending:
            yield opus.encode(encoder, bytes(pending).ljust(FRAME_BYTES, b"\0"))


async def collect(speaker, text):
    return [packet async for packet in speaker.stream(text)]
