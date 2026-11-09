"""Remote input payloads; the browser node supplies the Playwright page."""
import struct

from ink_hub.gateway.wire import ProtocolError


async def handle_input(page, payload):
    if not payload:
        raise ProtocolError("网页输入为空")
    kind = payload[0]
    if kind in (1, 3) and len(payload) == 5:
        x, y = struct.unpack_from("<HH", payload, 1)
        await page.mouse.click(x, y, delay=650 if kind == 3 else 0)
    elif kind == 2 and len(payload) == 2 and payload[1] in (0, 1):
        await page.keyboard.press("PageDown" if payload[1] else "PageUp")
    elif kind == 4 and len(payload) <= 256:
        try:
            text = payload[1:].decode("utf-8")
        except UnicodeDecodeError:
            raise ProtocolError("网页文字编码无效") from None
        if "\0" in text:
            raise ProtocolError("网页文字无效")
        await page.locator(":focus").fill(text)
    else:
        raise ProtocolError("网页输入格式无效")
