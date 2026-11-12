import hashlib
import ssl
import subprocess

import pytest
from aiohttp import ClientError, ClientSession, web

from ink_hub.gateway.server import Hub
from ink_hub.gateway.tls import identity, proto_allowed, verification_code


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
    hub = Hub(tmp_path, dev_allow_proto1=True)
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
            with pytest.raises((ClientError, OSError)):
                await client.get(f"http://127.0.0.1:{port}/v1/device")
    finally:
        await runner.cleanup()


def test_missing_openssl_fails_explicitly(tmp_path, monkeypatch):
    monkeypatch.setenv("PATH", "")
    with pytest.raises(RuntimeError, match="openssl"):
        identity(tmp_path)
