from tests.fake_device import FakeDevice


def answer_text(device):
    text = "".join(m["params"]["text"] for m in device.messages if isinstance(m, dict) and m.get("method") == "ai.delta")
    device.messages.clear()
    return text


async def test_answer_follows_device_language(make_hub):
    _, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    await device.call("ai.ask", {"question": "示例", "lang": "en", "page": {"actions": []}})
    assert answer_text(device) == "This is a sample answer."
    await device.call("ai.ask", {"question": "示例", "lang": "zh", "page": {"actions": []}})
    assert answer_text(device) == "这是示例回答。"
    await device.call("ai.ask", {"question": "示例", "page": {"actions": []}})
    assert answer_text(device) == "这是示例回答。"  # an older device without "lang" gets Chinese


async def test_bad_language_is_refused(make_hub):
    _, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    await device.call("ai.ask", {"question": "示例", "lang": 3, "page": {"actions": []}})
    assert answer_text(device) == "这次没有完成，请重试。"
