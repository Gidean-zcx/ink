import json
import os
from pathlib import Path

CAPS_PATH = os.environ.get("INK_HUB_CAPS", "../build/test-out/hub/caps.json")
SAMPLE_PATH = os.environ.get("INK_HUB_SAMPLES")
for key in list(os.environ):
    if key == "DEEPSEEK_API_KEY" or key.startswith("INK_HUB_"):
        del os.environ[key]

import pytest
from aiohttp import ClientSession, web

from ink_hub.gateway.server import Hub

SAMPLES = []


@pytest.fixture
async def make_hub(tmp_path):
    runners, clients, hubs = [], [], []

    async def create(**kwargs):
        hub = Hub(tmp_path / str(len(hubs)), dev_allow_proto1=kwargs.pop("dev_allow_proto1", True), record_wire=True, **kwargs)
        runner = web.AppRunner(hub.application(), access_log=None)
        await runner.setup()
        site = web.TCPSite(runner, "127.0.0.1", 0)
        await site.start()
        port = site._server.sockets[0].getsockname()[1]
        client = ClientSession()
        runners.append(runner)
        clients.append(client)
        hubs.append(hub)
        return hub, client, f"http://127.0.0.1:{port}"

    yield create
    for client in clients:
        await client.close()
    for runner in runners:
        await runner.cleanup()
    for hub in hubs:
        SAMPLES.extend(hub.samples)


@pytest.fixture(scope="session")
def firmware_caps():
    path = Path(CAPS_PATH)
    if not path.exists():
        pytest.skip("需要先从当前固件导出能力表：见 hub/README.md")
    return json.loads(path.read_text())


def pytest_sessionfinish(session, exitstatus):
    path = SAMPLE_PATH
    if path and exitstatus == 0:
        output = Path(path)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text("".join(s + "\n" for s in SAMPLES))
