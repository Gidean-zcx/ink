"""The real hub has no completion model; never manufacture a successful answer."""
from tests.fake_device import FakeDevice


async def test_suggest_unavailable_and_strict_payload(make_hub):
    _, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    for prefix in ("合成文字", "字" * 500):
        reply = await device.call("text/suggest", {"type": "plain", "prefix": prefix})
        assert reply["result"] == {"available": False, "suggestions": []}
    for bad in ({}, {"type": "password", "prefix": "FAKE_SECRET"},
                {"type": "plain", "prefix": "字" * 501},
                {"type": "plain", "prefix": ""}, {"type": "plain", "prefix": 1},
                {"type": "plain", "prefix": "示例", "page": "FAKE_PRIVATE"}):
        assert (await device.call("text/suggest", bad))["error"]["code"] == -32602
