"""XiaoZhi Doubao streaming ASR framing and TTS audio_params adaptation.

Copyright (c) 2025 xinnan-tech. MIT; see XIAOZHI-LICENSE.
"""
import base64
import gzip
import json
import uuid
from contextlib import aclosing

from aiohttp import WSMsgType

from .cloud import client, collect, duplex, encode_pcm
from .opus import Opus


def pack(kind, value, final=False):
    body = gzip.compress(value if isinstance(value, bytes) else json.dumps(value).encode())
    return bytes([0x11, kind << 4 | (2 if final else 0), 0x11, 0]) + len(body).to_bytes(4, "big") + body


def unpack(data):
    offset = (data[0] & 15) * 4
    kind, flags = data[1] >> 4, data[1] & 15
    if kind == 15:
        raise RuntimeError("豆包识别失败")
    if flags & 1:
        offset += 4
    size = int.from_bytes(data[offset:offset + 4], "big")
    body = data[offset + 4:offset + 4 + size]
    return json.loads(gzip.decompress(body) if data[2] & 15 == 1 else body), bool(flags & 2)


class DoubaoASR:
    def __init__(self, appid, token, *, url="wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async", opus=None):
        self.appid, self.token, self.url, self.opus = appid, token, url, opus or Opus()

    async def stream(self, audio):
        headers = {"X-Api-App-Key": self.appid, "X-Api-Access-Key": self.token,
                   "X-Api-Resource-Id": "volc.bigasr.sauc.duration", "X-Api-Connect-Id": str(uuid.uuid4())}
        async with client() as http, http.ws_connect(self.url, headers=headers) as ws:
            await ws.send_bytes(pack(1, {"user": {"uid": "ink"}, "audio": {"format": "pcm", "rate": 16000,
                                       "bits": 16, "channel": 1}, "request": {"model_name": "bigmodel",
                                       "enable_punc": True, "show_utterances": True}}))
            async with duplex(ws, audio, lambda pcm: ws.send_bytes(pack(2, pcm)),
                              lambda: ws.send_bytes(pack(2, b"", True))):
                async for msg in ws:
                    if msg.type != WSMsgType.BINARY:
                        raise RuntimeError("豆包识别响应无效")
                    result, final = unpack(msg.data)
                    if result.get("code", 0) not in (0, 1000, 20000000):
                        raise RuntimeError("豆包识别失败")
                    value = result.get("result", {})
                    if value.get("text") or final:
                        yield value.get("text", ""), final
                    if final:
                        return
                raise RuntimeError("豆包识别连接提前结束")


class DoubaoTTS:
    def __init__(self, appid, token, voice, *, url="https://openspeech.bytedance.com/api/v3/tts/unidirectional", opus=None):
        self.appid, self.token, self.voice, self.url, self.opus = appid, token, voice, url, opus or Opus()

    async def stream(self, text):
        async def audio():
            headers = {"X-Api-App-Id": self.appid, "X-Api-Access-Key": self.token,
                       "X-Api-Resource-Id": "seed-tts-2.0", "X-Api-Request-Id": str(uuid.uuid4())}
            body = {"user": {"uid": "ink"}, "req_params": {"text": text, "speaker": self.voice,
                    "audio_params": {"format": "pcm", "sample_rate": 16000}}}
            async with client() as http, http.post(self.url, headers=headers, json=body) as reply:
                reply.raise_for_status()
                async for line in reply.content:
                    value = json.loads(line)
                    if value.get("code") == 20000000:
                        return
                    if value.get("code", 0) != 0:
                        raise RuntimeError("豆包朗读失败")
                    if value.get("data"):
                        yield base64.b64decode(value["data"], validate=True)
                raise RuntimeError("豆包朗读连接提前结束")
        async with aclosing(audio()) as source, aclosing(encode_pcm(self.opus, source)) as packets:
            async for packet in packets:
                yield packet

    async def synthesize(self, text):
        return await collect(self, text)
