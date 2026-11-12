"""XiaoZhi NLS transcription and FlowingSpeechSynthesizer adaptation.

Copyright (c) 2025 xinnan-tech. MIT; see XIAOZHI-LICENSE.
"""
import uuid
from contextlib import aclosing

from aiohttp import WSMsgType

from .cloud import client, collect, duplex, encode_pcm
from .opus import Opus


def command(appkey, task, namespace, name, payload=None):
    return {"header": {"appkey": appkey, "task_id": task, "message_id": uuid.uuid4().hex,
                       "namespace": namespace, "name": name}, "payload": payload or {}}


def event(msg):
    if msg.type != WSMsgType.TEXT:
        raise RuntimeError("阿里语音响应无效")
    value = msg.json()
    if value.get("header", {}).get("status", 20000000) != 20000000:
        raise RuntimeError("阿里语音请求失败")
    return value.get("header", {}).get("name"), value.get("payload", {})


class AliyunASR:
    def __init__(self, appkey, token, *, url="wss://nls-gateway-cn-shanghai.aliyuncs.com/ws/v1", opus=None):
        self.appkey, self.token, self.url, self.opus = appkey, token, url, opus or Opus()

    async def stream(self, audio):
        task = uuid.uuid4().hex
        request = lambda name, payload=None: command(self.appkey, task, "SpeechTranscriber", name, payload)
        async with client() as http, http.ws_connect(self.url, headers={"X-NLS-Token": self.token}) as ws:
            await ws.send_json(request("StartTranscription", {"format": "pcm", "sample_rate": 16000,
                                     "enable_intermediate_result": True, "enable_punctuation_prediction": True,
                                     "enable_inverse_text_normalization": True}))
            name, _ = event(await ws.receive())
            if name != "TranscriptionStarted":
                raise RuntimeError("阿里识别未就绪")
            async with duplex(ws, audio, ws.send_bytes, lambda: ws.send_json(request("StopTranscription"))):
                async for msg in ws:
                    name, payload = event(msg)
                    if name in ("TranscriptionResultChanged", "SentenceEnd"):
                        yield payload.get("result", ""), name == "SentenceEnd"
                    if name == "TranscriptionCompleted":
                        return
                raise RuntimeError("阿里识别连接提前结束")


class AliyunTTS:
    def __init__(self, appkey, token, voice="longxiaochun", *, url="wss://nls-gateway-cn-beijing.aliyuncs.com/ws/v1", opus=None):
        self.appkey, self.token, self.voice, self.url, self.opus = appkey, token, voice, url, opus or Opus()

    async def stream(self, text):
        async def audio():
            task = uuid.uuid4().hex
            request = lambda name, payload=None: command(self.appkey, task, "FlowingSpeechSynthesizer", name, payload)
            async with client() as http, http.ws_connect(self.url, headers={"X-NLS-Token": self.token}) as ws:
                await ws.send_json(request("StartSynthesis", {"voice": self.voice, "format": "pcm", "sample_rate": 16000}))
                async for msg in ws:
                    if msg.type == WSMsgType.BINARY:
                        yield msg.data
                        continue
                    name, _ = event(msg)
                    if name == "SynthesisStarted":
                        await ws.send_json(request("RunSynthesis", {"text": text}))
                        await ws.send_json(request("StopSynthesis"))
                    elif name == "SynthesisCompleted":
                        return
                raise RuntimeError("阿里朗读连接提前结束")
        async with aclosing(audio()) as source, aclosing(encode_pcm(self.opus, source)) as packets:
            async for packet in packets:
                yield packet

    async def synthesize(self, text):
        return await collect(self, text)
