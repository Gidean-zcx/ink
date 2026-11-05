"""Check the documented CLI from an independently created locked environment."""
import argparse
import asyncio
import socket
import sys

from aiohttp import ClientSession


async def check(directory):
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    process = await asyncio.create_subprocess_exec(
        sys.executable, "-m", "ink_hub", "serve", "--data", directory, "--port", str(port), "--dev-allow-proto1",
        stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
    try:
        for _ in range(3):
            line = await asyncio.wait_for(process.stdout.readline(), 10)
            if "已就绪".encode() in line:
                break
        else:
            raise AssertionError("CLI never became ready")
        async with ClientSession() as client:
            response = await client.get(f"http://127.0.0.1:{port}/healthz")
            assert await response.json() == {"ok": True, "mode": "example"}
            response = await client.get(f"http://127.0.0.1:{port}/setup")
            assert response.status == 200 and "设置家庭服务器" in await response.text()
        print("fresh CLI: readiness, healthz and setup passed; synthetic data only")
    finally:
        if process.returncode is None:
            process.terminate()
        await asyncio.wait_for(process.communicate(), 10)


parser = argparse.ArgumentParser()
parser.add_argument("--out", required=True)
asyncio.run(check(parser.parse_args().out))
