import hashlib
import asyncio
import json
import os
import socket
import ssl
import subprocess
import sys
from pathlib import Path

import pytest
from aiohttp import ClientError, ClientSession, web

from ink_hub.gateway.server import Hub
from ink_hub.gateway.tls import identity, proto_allowed, verification_code
from ink_hub.gateway.wire import ProtocolError
from tests.fake_device import FakeDevice

LAN = {"url": "wss://example.com:8444/v1/device", "pin": "1" * 64, "code": verification_code("1" * 64)}


async def begin(client, url, nonce="synthetic-nonce", **params):
    device = FakeDevice(client, url)
    device.ws = await client.ws_connect(url + "/v1/device")
    reply = await device.call("pair/begin", {"proto": 2, "nonce": nonce, **params})
    return device, reply


async def complete(device):
    message = await device.recv()
    assert message["method"] == "pair/complete"
    await device.answer(message)
    return message["params"]


async def hello(device, credentials, **params):
    return await device.call("hello", {"proto": 2, "device": credentials["device"],
        "token": credentials["token"], "member": "kid", "authed": False, "epoch": 17, **params})


async def test_claim_and_restart_keep_hashed_token(make_hub):
    hub, client, url = await make_hub(lan=LAN)
    device, reply = await begin(client, url)
    code = reply["result"]["code"]
    assert len(code) == 8 and reply["result"]["expires_s"] == 600
    denied = await client.post(url + "/claim", json={"code": code, "check": "WRONG"})
    assert denied.status == 403
    claimed = await client.post(url + "/claim", json={"code": code, "check": LAN["code"], "name": "合成设备"})
    assert claimed.status == 200
    offer = await device.recv()
    assert offer["method"] == "pair/offer" and offer["params"]["name"] == "合成设备"
    assert {"family", "by", "txid"} <= offer["params"].keys()
    await device.answer(offer)
    assert (await device.call("pair/accept", {"txid": offer["params"]["txid"]}))["result"] == {}
    credentials = await complete(device)
    assert credentials["urls"] == [LAN["url"]] and credentials["pins"] == [LAN["pin"]]
    assert (await hello(device, credentials))["result"]["proto"] == 2
    await device.ws.close()
    restarted, second_client, second_url = await make_hub(directory=hub.directory, lan=LAN)
    second = FakeDevice(second_client, second_url)
    second.ws = await second_client.ws_connect(second_url + "/v1/device")
    assert {"session", "proto", "now_ms", "hub_instance"} <= (await hello(second, credentials))["result"].keys()
    for path in restarted.directory.rglob("*"):
        if path.is_file():
            assert credentials["token"].encode() not in path.read_bytes()


async def test_nonce_resume_expiry_and_limits(make_hub):
    now = [1000.0]
    hub, client, url = await make_hub(lan=LAN, clock=lambda: now[0])
    first, reply = await begin(client, url)
    second, again = await begin(client, url)
    code = reply["result"]["code"]
    assert again == reply
    assert first.ws.closed or await first.ws.receive()
    now[0] += 601
    await hub.pairing.expire(code)
    assert (await second.recv())["method"] == "pair/expired"
    assert code not in hub.pairing.codes
    third, reply = await begin(client, url, "five-attempts")
    code = reply["result"]["code"]
    for check in ("不对", "WRONG", "WRONG", "WRONG", "WRONG"):
        assert (await client.post(url + "/claim", json={"code": code, "check": check})).status == 403
    assert (await third.recv())["method"] == "pair/expired"
    assert (await client.post(url + "/claim", json={"code": code, "check": LAN["code"]})).status == 400
    for i in range(32):
        _, result = await begin(client, url, f"capacity-{i}")
        assert "result" in result
    _, result = await begin(client, url, "capacity-full")
    assert result["error"]["code"] == -32022


async def test_preauth_resume_rotation_and_errors(make_hub):
    now = [1000.0]
    hub, client, url = await make_hub(lan=LAN, clock=lambda: now[0])
    preauth = hub.tokens.preauth()
    first, reply = await begin(client, url, preauth=preauth)
    assert "result" in reply
    old = await complete(first)
    await first.ws.close()
    hub, client, url = await make_hub(directory=hub.directory, lan=LAN, clock=lambda: now[0])
    second, reply = await begin(client, url, txid=old["txid"])
    new = await complete(second)
    assert new["device"] == old["device"] and new["token"] != old["token"]
    assert (await hello(second, old))["error"]["code"] == -32010
    assert (await hello(second, new, token="synthetic-wrong-token"))["error"]["code"] == -32010
    _, reused = await begin(client, url, "another-nonce", preauth=preauth)
    assert reused["error"]["code"] == -32030
    _, reused = await begin(client, url, preauth=preauth)
    assert reused["error"]["code"] == -32030
    expired = hub.tokens.preauth()
    now[0] += 86401
    _, invalid = await begin(client, url, "expired-file", preauth=expired)
    assert invalid["error"]["code"] == -32030
    now[0] += 7 * 86400
    assert (await hello(second, new))["error"]["code"] == -32013
    with pytest.raises(ProtocolError) as caught:
        hub.tokens.authenticate("another-device", new["token"])
    assert caught.value.code == -32010


async def test_formal_member_scope_and_unauthenticated_gate(make_hub):
    hub, client, url = await make_hub(lan=LAN)
    device, _ = await begin(client, url, preauth=hub.tokens.preauth())
    credentials = await complete(device)
    assert (await device.call("caps/list"))["error"]["code"] == -32010
    assert (await hello(device, credentials))["result"]["proto"] == 2
    connection = hub.connections[credentials["device"]]
    assert connection.scope.member == "kid" and hub.authority.role(connection.scope) == "child"
    await device.send("ink/member", {"member": "parent", "authed": True, "epoch": 18}, rid=False)
    assert (await device.recv())["method"] == "caps/changed"
    assert connection.scope.member == "parent" and hub.authority.role(connection.scope) == "adult"
    await device.send("ink/member", {"member": "parent", "authed": False, "epoch": 19}, rid=False)
    assert (await device.recv())["method"] == "caps/changed"
    assert hub.authority.role(connection.scope) == "child"
    assert (await device.call("ink/member", {"member": "parent", "authed": True, "epoch": 18}))["error"]
    assert hub.authority.role(connection.scope) == "child"


async def cli(*args):
    root = Path(__file__).resolve().parents[3]
    process = await asyncio.create_subprocess_exec(sys.executable, "-m", "ink_hub", *args,
        env={**os.environ, "PYTHONPATH": str(root / "hub")},
        stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
    stdout, stderr = await asyncio.wait_for(process.communicate(), 10)
    return process.returncode, stdout.decode(), stderr.decode()


async def test_claim_cli_and_pair_file(make_hub, tmp_path):
    info = identity(tmp_path)
    hub, client, url = await make_hub(directory=tmp_path, lan=info)
    output = tmp_path / "ink-pair.json"
    rc, stdout, stderr = await cli("pair-file", "--data", str(tmp_path), "--out", str(output))
    assert rc == 0, stderr
    config = json.loads(output.read_text())
    assert set(config) == {"url", "console", "pin", "preauth"}
    assert config["url"].startswith("wss://") and config["console"].startswith("https://")
    assert config["pin"] == info["pin"] and config["preauth"] not in stdout + stderr
    assert output.stat().st_mode & 0o777 == 0o600
    device, _ = await begin(client, url, preauth=config["preauth"])
    await complete(device)
    device, reply = await begin(client, url, nonce="cli-claim")
    code = reply["result"]["code"]
    rc, stdout, stderr = await cli("claim", code, info["code"], "--name", "合成终端", "--port", url.rsplit(":", 1)[1])
    assert rc == 0 and json.loads(stdout) == {"offered": True}, stderr
    assert (await device.recv())["params"]["name"] == "合成终端"


async def test_serve_lan_prints_stable_identity(tmp_path):
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    root = Path(__file__).resolve().parents[3]
    process = await asyncio.create_subprocess_exec(sys.executable, "-m", "ink_hub", "serve", "--lan",
        "--data", str(tmp_path), "--port", "0", "--lan-port", str(port),
        env={**os.environ, "PYTHONPATH": str(root / "hub")},
        stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
    output = ""
    try:
        async with asyncio.timeout(10):
            while "家庭服务器已就绪" not in output:
                line = await process.stdout.readline()
                assert line, (await process.stderr.read()).decode()
                output += line.decode()
        rc, pin_output, stderr = await cli("pin", "--data", str(tmp_path), "--lan-port", str(port))
        assert rc == 0 and all(line in output for line in pin_output.splitlines()), stderr
        context = ssl.create_default_context(cafile=str(tmp_path / "tls/cert.pem"))
        context.check_hostname = False
        async with ClientSession() as client:
            async with client.get(f"https://127.0.0.1:{port}/setup", ssl=context) as reply:
                assert reply.status == 404
    finally:
        process.terminate()
        _, errors = await asyncio.wait_for(process.communicate(), 10)
        assert "Traceback" not in errors.decode(), errors.decode()


@pytest.mark.parametrize("remote", ["192.0.2.8", "2001:db8::1"])
def test_remote_protocol_gate(remote):
    assert not proto_allowed(1, remote, True, True)
    assert not proto_allowed(1, remote, False, True)
    assert proto_allowed(2, remote, True, False)
    assert proto_allowed(1, "127.0.0.1", False, True)
    assert not proto_allowed(1, "127.0.0.1", True, True)


async def test_stable_tls_pin_and_exclusive_route(tmp_path):
    info = identity(tmp_path)
    assert identity(tmp_path)["pin"] == info["pin"]
    assert (tmp_path / "tls/key.pem").stat().st_mode & 0o777 == 0o600
    cert = tmp_path / "tls/cert.pem"
    pub = subprocess.check_output(["openssl", "x509", "-in", str(cert), "-pubkey", "-noout"])
    der = subprocess.check_output(["openssl", "pkey", "-pubin", "-outform", "DER"], input=pub)
    digest = subprocess.check_output(["openssl", "dgst", "-sha256"], input=der).decode().split()[-1]
    assert info["pin"] == digest == hashlib.sha256(der).hexdigest()
    assert verification_code("0" * 64) == "AAAAAAAAAAAA"
    assert verification_code("f" * 64) == "777777777777"
    hub = Hub(tmp_path, dev_allow_proto1=True, lan=info)
    runner = web.AppRunner(hub.tls_application(), access_log=None)
    await runner.setup()
    site = web.TCPSite(runner, "127.0.0.1", 0, ssl_context=info["context"])
    await site.start()
    port = site._server.sockets[0].getsockname()[1]
    context = ssl.create_default_context(cafile=str(cert))
    context.check_hostname = False
    try:
        async with ClientSession() as client:
            async with client.get(f"https://127.0.0.1:{port}/setup", ssl=context) as reply:
                assert reply.status == 404
            async with client.ws_connect(f"https://127.0.0.1:{port}/v1/device", ssl=context) as ws:
                peer = ws._response.connection.transport.get_extra_info("ssl_object").getpeercert(binary_form=True)
                pem = ssl.DER_cert_to_PEM_cert(peer).encode()
                observed = subprocess.check_output(["openssl", "x509", "-pubkey", "-noout"], input=pem)
                observed = subprocess.check_output(["openssl", "pkey", "-pubin", "-outform", "DER"], input=observed)
                assert hashlib.sha256(observed).hexdigest() == info["pin"]
                await ws.send_json({"jsonrpc": "2.0", "id": 1, "method": "hello", "params": {"proto": 1}})
                assert (await ws.receive_json())["error"]["code"] == -32012
                await ws.send_json({"jsonrpc": "2.0", "id": 2, "method": "pair/begin", "params": {
                    "proto": 2, "nonce": "tls-synthetic-nonce", "preauth": hub.tokens.preauth()}})
                assert (await ws.receive_json())["result"]["expires_s"] == 600
                credentials = (await ws.receive_json())["params"]
                await ws.send_json({"jsonrpc": "2.0", "id": 3, "method": "hello", "params": {
                    "proto": 2, "device": credentials["device"], "token": credentials["token"],
                    "member": "kid", "epoch": 1, "authed": False}})
                assert (await ws.receive_json())["result"]["proto"] == 2
            with pytest.raises((ClientError, OSError)):
                await client.get(f"http://127.0.0.1:{port}/v1/device")
    finally:
        await runner.cleanup()


def test_missing_openssl_fails_explicitly(tmp_path, monkeypatch):
    monkeypatch.setenv("PATH", "")
    with pytest.raises(RuntimeError, match="openssl"):
        identity(tmp_path)
