import pytest

from ink_hub.gateway.wire import ProtocolError
from ink_hub.voice.flow import validate_photos
from tests.fake_device import FakeDevice


async def test_photo_is_accepted_without_real_vision(make_hub):
    _, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    photo = {"id": 1, "width": 128, "height": 128, "gray4": "a" * 16384}
    result = await device.call("ai.ask", {"question": "合成图片问题", "page": {"actions": []}, "attachments": [photo]})
    assert result["result"]["done"]
    assert any(m.get("method") == "ai.delta" for m in device.messages if isinstance(m, dict))


async def test_replay_stub_reports_unavailable(make_hub):
    _, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    result = await device.call("ai.replay", {"stream": 1, "text": "合成回答"})
    assert result["error"] == {"code": -32021, "message": "unavailable"}


@pytest.mark.parametrize("photo", [
    {"width": 513, "height": 1, "gray4": "f" * 513},
    {"width": 512, "height": 129, "gray4": "f" * 66048},
    {"width": 0, "height": 1, "gray4": ""},
    {"width": 1, "height": 1, "gray4": "ff"},
    {"width": 1, "height": 1, "gray4": "g"},
    {"width": True, "height": 1, "gray4": "f"},
])
def test_invalid_photo_is_rejected(photo):
    with pytest.raises(ProtocolError):
        validate_photos([photo])


def test_attachment_count_and_maximum_size():
    validate_photos([])
    validate_photos([{"width": 512, "height": 128, "gray4": "f" * 65536}])
    with pytest.raises(ProtocolError):
        validate_photos([{}, {}])
