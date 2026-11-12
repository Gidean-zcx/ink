"""Real say uplink -> resident Whisper/fake cloud -> fake LLM -> say/cloud.

Run manually; requires explicit existing server binary and model. No downloads.
"""
import argparse
import asyncio
import json
import math
import os
import socket
import statistics
import subprocess
import tempfile
from pathlib import Path

from aiohttp import ClientSession, WSMsgType, web

from ink_hub.gateway.server import Hub
from ink_hub.gateway.wire import frame
from ink_hub.models.fake import FakeModel
from ink_hub.speech.local import SayTTS, WhisperASR
from ink_hub.speech.opus import Opus
from tests.fake_device import FakeDevice
from tests.speech.cloud_fixture import Vendors

PHRASES = ["现在几点了。", "请给我讲一个简短的故事。", "今天适合读什么书。"]
REPLY = "这是用于测量延迟的合成回答，我们已经收到了你的问题。"


class Device(FakeDevice):
    async def recv(self):
        msg = await asyncio.wait_for(self.ws.receive(), 90)
        assert msg.type in (WSMsgType.BINARY, WSMsgType.TEXT), msg.type
        data = msg.data if msg.type == WSMsgType.BINARY else json.loads(msg.data)
        self.messages.append(data)
        return data


async def trial(asr, tts, packets, directory):
    hub = Hub(directory, dev_allow_proto1=True, asr=asr, tts=tts, model=FakeModel(text=REPLY))
    runner = web.AppRunner(hub.application(), access_log=None)
    await runner.setup()
    site = web.TCPSite(runner, "127.0.0.1", 0)
    await site.start()
    try:
        async with ClientSession() as client:
            device = Device(client, f"http://127.0.0.1:{site._server.sockets[0].getsockname()[1]}")
            await device.connect()
            await device.call("rt.open", {"sid": 7, "cap": "voice.chat", "params": {"interim_ms": 800}})
            rid = await device.send("ai.listen", {"stream": 7, "budget_ms": 45000, "page": {"actions": []}})
            await device.answer(await device.recv())
            async def upload():
                loop = asyncio.get_running_loop()
                target = loop.time()
                for seq, packet in enumerate(packets):
                    target += .06
                    await asyncio.sleep(max(0, target - loop.time()))
                    await device.ws.send_bytes(frame(1, 7, seq, packet))
                await device.send("ai.listen.end", {"stream": 7}, rid=False)
            task = asyncio.create_task(upload())
            try:
                result = await device.until(rid)
                await task
            finally:
                task.cancel()
                await asyncio.gather(task, return_exceptions=True)
            session = hub.connections["example-device"].sessions[7]
            times = session.times
            assert result["result"]["done"] and all(key in times for key in (
                "speech_end", "asr_final", "model_first_text", "first_sentence", "first_text", "first_audio")), times
            stt = [m["params"]["text"] for m in device.messages if isinstance(m, dict) and m.get("method") == "ai.stt"]
            return {"ms": {key: round((value - times["speech_end"]) * 1000, 2) for key, value in times.items()},
                    "recognized": stt[-1], "uplink_ms": len(packets) * 60}
    finally:
        await runner.cleanup()


def summary(rows):
    result = {}
    for field in ("first_text", "first_audio", "asr_final", "model_first_text", "first_sentence"):
        values = sorted(row["ms"][field] for row in rows)
        result[field] = {"p50_ms": round(statistics.median(values), 2),
                         "p95_ms": values[math.ceil(.95 * len(values)) - 1]}
    return result


async def main(args):
    assert not os.environ.get("DEEPSEEK_API_KEY") and os.environ.get("INK_HUB_MODEL_PROVIDER", "fake") == "fake"
    opus = Opus()
    say = SayTTS(opus=opus)
    recordings = [await say.synthesize(phrase) for phrase in PHRASES]
    server = await Vendors().start()
    process = None
    try:
        with tempfile.TemporaryDirectory(prefix="voice-benchmark-", dir="work") as directory:
            log = Path(directory) / "whisper.log"
            with socket.socket() as sock:
                sock.bind(("127.0.0.1", 0))
                port = sock.getsockname()[1]
            with log.open("wb") as output:
                process = subprocess.Popen([args.whisper_server, "-m", args.model, "--host", "127.0.0.1",
                                            "--port", str(port), "-t", "2", "-l", "zh", "-fa"],
                                           stdin=subprocess.DEVNULL, stdout=output, stderr=output)
            loop = asyncio.get_running_loop()
            deadline = loop.time() + 90
            while True:
                if process.poll() is not None:
                    print(log.read_text(errors="replace")[-2000:], flush=True)
                    raise RuntimeError("常驻 Whisper 启动失败；检查本地编译和 GPU 可用性")
                try:
                    reader, writer = await asyncio.open_connection("127.0.0.1", port)
                    writer.close()
                    await writer.wait_closed()
                    break
                except OSError:
                    if loop.time() >= deadline:
                        raise TimeoutError("Whisper 启动超时")
                    await asyncio.sleep(.05)
            local = WhisperASR(url=f"http://127.0.0.1:{port}/inference", opus=opus)
            cloud_asr, cloud_tts = server.providers("doubao", opus)
            results = {"runs_per_phrase": args.runs, "phrases": PHRASES,
                       "boundary": "最后一个上行采样提交后到中枢文字/音频发出；假 LLM；无设备播放和真实云网络"}
            for name, asr, tts in (("local", local, say), ("fake_cloud", cloud_asr, cloud_tts)):
                rows = []
                for repetition in range(args.runs):
                    for phrase, packets in zip(PHRASES, recordings):
                        row = await trial(asr, tts, packets, Path(directory) / f"{name}-{len(rows)}")
                        row["phrase"] = phrase
                        rows.append(row)
                        print(name, len(rows), row["ms"], flush=True)
                results[name] = {"summary": summary(rows), "samples": rows}
            Path(args.out).write_text(json.dumps(results, ensure_ascii=False, indent=2) + "\n")
            print(json.dumps({key: results[key]["summary"] for key in ("local", "fake_cloud")}, ensure_ascii=False))
    finally:
        await server.close()
        if process and process.poll() is None:
            process.terminate()
            try:
                await asyncio.wait_for(asyncio.to_thread(process.wait), 5)
            except TimeoutError:
                process.kill()
                await asyncio.to_thread(process.wait)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--whisper-server", required=True)
    parser.add_argument("--model", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--runs", type=int, default=5)
    asyncio.run(main(parser.parse_args()))
