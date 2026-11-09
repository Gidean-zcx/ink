"""The device's input format reaches the browser once, using synthetic values."""
import struct
from types import SimpleNamespace
from unittest.mock import AsyncMock

import pytest

from ink_hub.gateway.wire import ProtocolError, frame, parse_frame
from ink_hub.remote.input import handle_input


def browser():
    field = SimpleNamespace(fill=AsyncMock())
    page = SimpleNamespace(mouse=SimpleNamespace(click=AsyncMock()),
                           keyboard=SimpleNamespace(press=AsyncMock()), locator=lambda _: field)
    return page, field


async def test_remote_input():
    page, field = browser()
    for payload in (b"\x01" + struct.pack("<HH", 180, 240), b"\x02\x01", b"\x02\x00",
                    b"\x03" + struct.pack("<HH", 220, 300), b"\x04" + "合成输入".encode()):
        kind, sid, _, _, _, received = parse_frame(frame(4, 2, 0, payload))
        assert (kind, sid) == (4, 2)
        await handle_input(page, received)
    assert page.mouse.click.call_args_list[0].args == (180, 240)
    assert page.mouse.click.call_args_list[1].kwargs == {"delay": 650}
    assert [c.args[0] for c in page.keyboard.press.call_args_list] == ["PageDown", "PageUp"]
    field.fill.assert_awaited_once_with("合成输入")


@pytest.mark.parametrize("payload", [b"", b"\x01", b"\x02\x02", b"\x03\0\0", b"\x04\xff",
                                      b"\x04a\0b", b"\x04" + b"x" * 256, b"\x05"])
async def test_invalid_input_has_no_effect(payload):
    page, field = browser()
    with pytest.raises(ProtocolError):
        await handle_input(page, payload)
    page.mouse.click.assert_not_awaited()
    page.keyboard.press.assert_not_awaited()
    field.fill.assert_not_awaited()

async def test_gateway_routes_session_input_and_denies_child(make_hub):
    from ink_hub.gateway.server import Session
    from tests.fake_device import FakeDevice

    hub, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    conn = hub.connections["example-device"]
    page, field = browser()
    hub.authority.add("demo", "default", "adult")
    conn.scope = hub.authority.issue("demo", "default")
    conn.sessions[2] = Session("browser.session", conn.epoch, browser=page)
    await device.ws.send_bytes(frame(4, 2, 0, b"\x02\x01"))
    await device.ws.send_bytes(frame(4, 2, 1, b"\x03" + struct.pack("<HH", 40, 80)))
    await device.ws.send_bytes(frame(4, 2, 2, b"\x04" + "合成提交".encode()))
    await device.call("ping", {})
    page.keyboard.press.assert_awaited_once_with("PageDown")
    page.mouse.click.assert_awaited_once_with(40, 80, delay=650)
    field.fill.assert_awaited_once_with("合成提交")
    hub.authority.add("demo", "default", "child")
    conn.scope = hub.authority.issue("demo", "default")
    await device.ws.send_bytes(frame(4, 2, 3, b"\x02\x00"))
    denied = await device.recv()
    assert denied["method"] == "rt.nav" and denied["params"]["blocked"] is True
    assert 2 not in conn.sessions
    assert page.keyboard.press.await_count == 1
