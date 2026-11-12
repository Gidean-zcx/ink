"""Loopback-only vendor protocol fixtures; all audio is a synthetic tone."""
import asyncio
import base64
import gzip
import json
import math
import struct

from aiohttp import web, WSMsgType

from ink_hub.speech.doubao import unpack

TEXT = "这是本机合成的测试文字。"
ANSWER = "这是一句超过八个字的回答，后面的文字还没有凑齐。"
PCM = struct.pack("<960h", *(int(8000 * math.sin(2 * math.pi * 330 * n / 16000)) for n in range(960)))


def doubao_result(text, final=False):
    payload = gzip.compress(json.dumps({"result": {"text": text}}).encode())
    return bytes([0x11, 0x93 if final else 0x91, 0x11, 0]) + struct.pack(">iI", -1 if final else 1, len(payload)) + payload


class Vendors:
    def __init__(self):
        self.texts, self.started, self.closed = [], asyncio.Event(), asyncio.Event()
        self.gate = None
        self.frames = 8

    async def doubao_asr(self, request):
        ws = web.WebSocketResponse()
        await ws.prepare(request)
        init = await ws.receive_bytes()
        assert init[:2] == b"\x11\x10"
        count = 0
        async for msg in ws:
            assert msg.type == WSMsgType.BINARY
            assert msg.data[1] >> 4 == 2
            if msg.data[1] & 2:
                await ws.send_bytes(doubao_result(TEXT, True))
                break
            count += 1
            await ws.send_bytes(doubao_result(TEXT[:min(count, len(TEXT))]))
        await ws.close()
        return ws

    async def aliyun_asr(self, request):
        ws = web.WebSocketResponse()
        await ws.prepare(request)
        init = await ws.receive_json()
        assert init["header"]["namespace"] == "SpeechTranscriber"
        await ws.send_json(self.event("TranscriptionStarted"))
        count = 0
        async for msg in ws:
            if msg.type == WSMsgType.TEXT:
                assert msg.json()["header"]["name"] == "StopTranscription"
                await ws.send_json(self.event("SentenceEnd", TEXT))
                await ws.send_json(self.event("TranscriptionCompleted"))
                break
            assert msg.type == WSMsgType.BINARY and len(msg.data) == 1920
            count += 1
            await ws.send_json(self.event("TranscriptionResultChanged", TEXT[:min(count, len(TEXT))]))
        await ws.close()
        return ws

    @staticmethod
    def event(name, text=""):
        return {"header": {"name": name, "status": 20000000}, "payload": {"result": text}}

    async def doubao_tts(self, request):
        body = await request.json()
        assert body["req_params"]["audio_params"] == {"format": "pcm", "sample_rate": 16000}
        self.texts.append(body["req_params"]["text"])
        reply = web.StreamResponse(headers={"Content-Type": "application/json"})
        await reply.prepare(request)
        try:
            for i in range(self.frames):
                await reply.write((json.dumps({"code": 0, "data": base64.b64encode(PCM).decode()}) + "\n").encode())
                if i == 0:
                    self.started.set()
                    if self.gate:
                        await self.gate.wait()
            await reply.write(b'{"code":20000000}\n')
            await reply.write_eof()
        finally:
            self.closed.set()
        return reply

    async def aliyun_tts(self, request):
        ws = web.WebSocketResponse()
        await ws.prepare(request)
        init = await ws.receive_json()
        assert init["header"]["namespace"] == "FlowingSpeechSynthesizer"
        await ws.send_json(self.event("SynthesisStarted"))
        run, stop = await ws.receive_json(), await ws.receive_json()
        assert run["header"]["name"] == "RunSynthesis" and stop["header"]["name"] == "StopSynthesis"
        self.texts.append(run["payload"]["text"])
        try:
            for i in range(self.frames):
                # Deliberately split PCM across transport frames.
                await ws.send_bytes(PCM[:1001])
                await ws.send_bytes(PCM[1001:])
                if i == 0:
                    self.started.set()
                    if self.gate:
                        waiters = [asyncio.create_task(self.gate.wait()), asyncio.create_task(ws.receive())]
                        try:
                            await asyncio.wait(waiters, return_when=asyncio.FIRST_COMPLETED)
                            if ws.closed:
                                return ws
                        finally:
                            for waiter in waiters:
                                waiter.cancel()
                            await asyncio.gather(*waiters, return_exceptions=True)
            await ws.send_json(self.event("SynthesisCompleted"))
            await ws.close()
        finally:
            self.closed.set()
        return ws

    async def start(self):
        app = web.Application()
        for path, handler in (("doubao-asr", self.doubao_asr), ("aliyun-asr", self.aliyun_asr),
                              ("doubao-tts", self.doubao_tts), ("aliyun-tts", self.aliyun_tts)):
            app.router.add_route("*", "/" + path, handler)
        self.runner = web.AppRunner(app, access_log=None, handler_cancellation=True)
        await self.runner.setup()
        site = web.TCPSite(self.runner, "127.0.0.1", 0)
        await site.start()
        self.url = f"http://127.0.0.1:{site._server.sockets[0].getsockname()[1]}"
        return self

    async def close(self):
        if self.gate:
            self.gate.set()
        await self.runner.cleanup()

    def providers(self, name, opus):
        from ink_hub.speech.doubao import DoubaoASR, DoubaoTTS
        from ink_hub.speech.aliyun import AliyunASR, AliyunTTS
        asr, tts = (DoubaoASR, DoubaoTTS) if name == "doubao" else (AliyunASR, AliyunTTS)
        # Empty fixture credentials; never pretend to be a real key.
        return (asr("", "", opus=opus, url=self.url + "/" + name + "-asr"),
                tts("", "", "synthetic-voice", opus=opus, url=self.url + "/" + name + "-tts"))
