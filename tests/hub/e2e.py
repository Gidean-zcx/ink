"""Real product and loopback hub with the confirmed synthetic child fixture."""
import argparse
import asyncio
import importlib.util
import json
import os
import subprocess
import sys
import time
from pathlib import Path

if any(importlib.util.find_spec(name) is None for name in ("aiohttp", "yaml")):
    print("SKIP: locked hub Python environment is missing (see hub/README.md)")
    sys.exit(77)

from aiohttp import web
from ink_hub.gateway.server import Hub
from ink_hub.models.fake import FakeModel


async def run(args):
    root = Path(__file__).resolve().parents[2]
    output = Path(args.out)
    output.mkdir(parents=True, exist_ok=True)
    if not Path(args.assets, "fonts/sans-44.inkf").exists() or not Path(args.content, "books").exists():
        print("SKIP: generated CJK fonts or test content are missing")
        return 77
    hub = Hub(output / "hub-data", dev_allow_proto1=True, model=FakeModel(tool="reader.next_page"), record_wire=True)
    runner = web.AppRunner(hub.application(), access_log=None)
    await runner.setup()
    site = web.TCPSite(runner, "127.0.0.1", 0)
    await site.start()
    port = site._server.sockets[0].getsockname()[1]
    command = [args.sim, "--profile", str(root / "simulator/profiles/metalio-plus.json"),
               "--script", str(root / "tests/hub/e2e.script"), "--assets", args.assets, "--content", args.content,
               "--out", str(output / "sim"), "--hub-url", f"ws://127.0.0.1:{port}/v1/device",
               "--realtime=1", "--quiet", "--no-random-outages"]
    process = None
    try:
        started = time.perf_counter()
        process = await asyncio.create_subprocess_exec(*command, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.STDOUT)
        stdout, _ = await asyncio.wait_for(process.communicate(), 45)
        (output / "sim.log").write_bytes(stdout)
        assert process.returncode == 0, stdout.decode(errors="replace")[-2500:]
        assert hub.metrics.turns == 1, f"turns={hub.metrics.turns}"
        assert hub.metrics.tool_calls == 1, f"tool_calls={hub.metrics.tool_calls}"
        metrics = json.loads((output / "sim/metrics.json").read_text())
        device = metrics["metrics"]
        assert device.get("test.hub_member_ready", 0) == 1
        assert device["test.hub_member_ready_ms"]["max"] < 3000, "member fixture missed the original first tap"
        assert device.get("ai.answers", 0) == 1
        assert device.get("action.ai.reader.next_page", 0) == 1
        assert device.get("audio.speaker.played_ms", 0) >= 300
        assert device.get("hub.bad_messages", 0) == 0
        assert device.get("bridge.dropped_uplink", 0) == 0
        elapsed = time.perf_counter() - started
        assert elapsed >= metrics["virtual_time_s"] * 0.95, "simulator ran faster than 1x"
        # Keep measurements separate from device performance acceptance (step 2b).
        measurement = hub.metrics.snapshot()
        measurement.update(wall_s=elapsed, virtual_s=metrics["virtual_time_s"])
        assert measurement["loop_samples"] > 0
        (output / "measurement.json").write_text(json.dumps(measurement, indent=2) + "\n")
        samples = output / "messages.jsonl"
        samples.write_text("".join(s + "\n" for s in hub.samples))
        parser = await asyncio.create_subprocess_exec(args.parser, str(samples), stdout=asyncio.subprocess.PIPE)
        parser_out, _ = await parser.communicate()
        assert parser.returncode == 0
        print(parser_out.decode().strip())
        print(json.dumps(measurement, ensure_ascii=False))
        return 0
    finally:
        if process and process.returncode is None:
            process.kill()
            await process.wait()
        await runner.cleanup()


parser = argparse.ArgumentParser()
parser.add_argument("--sim", required=True)
parser.add_argument("--parser", required=True)
parser.add_argument("--assets", required=True)
parser.add_argument("--content", required=True)
parser.add_argument("--out", required=True)
sys.exit(asyncio.run(run(parser.parse_args())))
