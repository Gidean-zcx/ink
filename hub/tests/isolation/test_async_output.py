import asyncio

from ink_hub.models.fake import FakeModel
from tests.fake_device import FakeDevice


async def test_permission_change_invalidates_pending_output(make_hub):
    gate = asyncio.Event()
    model = FakeModel(gate=gate)
    hub, client, url = await make_hub(model=model)
    device = FakeDevice(client, url)
    await device.connect()
    rid = await device.send("ai.ask", {"question": "示例", "page": {"actions": []}})
    await device.answer(await device.recv())
    await asyncio.wait_for(model.started.wait(), 2)
    task = hub.connections["example-device"].turns[rid]
    hub.authority.add("demo", "default", "child")
    gate.set()
    await asyncio.wait_for(task, 2)
    assert not [m for m in hub.samples if '"ai.delta"' in m]


async def test_closing_other_session_does_not_cancel_text_turn(make_hub):
    gate = asyncio.Event()
    model = FakeModel(gate=gate)
    hub, client, url = await make_hub(model=model)
    device = FakeDevice(client, url)
    await device.connect()
    await device.call("rt.open", {"sid": 4, "cap": "asr.stream", "params": {}})
    rid = await device.send("ai.ask", {"question": "示例", "page": {"actions": []}})
    await device.answer(await device.recv())
    await asyncio.wait_for(model.started.wait(), 2)
    await device.send("rt.close", {"sid": 4}, rid=False)
    await device.call("caps/list")
    gate.set()
    assert (await device.until(rid))["result"]["done"]
