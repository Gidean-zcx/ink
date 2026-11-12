import asyncio
import gc
import json

import pytest

from ink_hub.gateway.server import Connection, Hub, Session
from ink_hub.models.fake import FakeModel
from ink_hub.voice.flow import run_dictation
from tests.fake_device import FakeDevice


class PausingModel(FakeModel):
    def __init__(self):
        super().__init__(text="synthetic private result", gate=asyncio.Event())
        self.finished = asyncio.Event()

    async def answer(self, question, tools):
        try:
            return await super().answer(question, tools)
        finally:
            self.finished.set()


async def test_registry_rechecks_permissions_after_handler(make_hub):
    model = PausingModel()
    hub, _, _ = await make_hub(model=model)
    scope = hub.authority.issue("demo", "default")
    task = asyncio.create_task(hub.registry.call(scope, "llm.ask", {"question": "示例"}, hub.authority))
    try:
        await asyncio.wait_for(model.started.wait(), 2)
        hub.authority.remove("demo", "default")
        model.gate.set()
        with pytest.raises(PermissionError):
            await asyncio.wait_for(task, 2)
    finally:
        model.gate.set()
        await asyncio.gather(task, return_exceptions=True)


async def test_direct_call_drops_result_after_revocation(make_hub):
    model = PausingModel()
    hub, client, url = await make_hub(model=model)
    device = FakeDevice(client, url)
    await device.connect()
    rid = await device.send("tools/call", {"name": "llm.ask", "arguments": {"question": "示例"}})
    try:
        await asyncio.wait_for(model.started.wait(), 2)
        hub.authority.remove("demo", "default")
        model.gate.set()
        await asyncio.wait_for(model.finished.wait(), 2)
        await hub.connections["example-device"].queue.join()
        assert not [m for m in map(json.loads, hub.samples) if m.get("id") == rid]
        assert not any("synthetic private result" in m for m in hub.samples)
    finally:
        model.gate.set()


@pytest.mark.parametrize("control", ["member", "disconnect", "reconnect"])
async def test_direct_call_does_not_block_connection_control(make_hub, control):
    model = PausingModel()
    hub, client, url = await make_hub(model=model)
    device = FakeDevice(client, url)
    await device.connect()
    old = hub.connections["example-device"]
    rid = await device.send("tools/call", {"name": "llm.ask", "arguments": {"question": "示例"}})
    try:
        await asyncio.wait_for(model.started.wait(), 2)
        if control == "member":
            await device.send("ink/member", {"member": "default", "epoch": 1}, rid=False)
            assert (await asyncio.wait_for(device.recv(), 2))["method"] == "caps/changed"
            assert old.epoch == 1
            assert "result" in await device.call("caps/list")
        elif control == "disconnect":
            await asyncio.wait_for(device.ws.close(), 2)
        else:
            replacement = FakeDevice(client, url)
            await asyncio.wait_for(replacement.connect(), 2)
            assert hub.connections["example-device"] is not old
        await asyncio.wait_for(model.finished.wait(), 2)
        await old.queue.join()
        assert not [m for m in map(json.loads, hub.samples) if m.get("id") == rid]
    finally:
        model.gate.set()


class Backpressure:
    def __init__(self):
        self.started, self.release = asyncio.Event(), asyncio.Event()
        self.queued = asyncio.Queue()
        self.sent = []

    async def send_str(self, data):
        if not self.started.is_set():
            self.started.set()
            await self.release.wait()
        self.sent.append(json.loads(data))

    async def close(self):
        pass

    async def wait_queued(self, *, method=None, rid=None):
        async with asyncio.timeout(2):
            while True:
                msg = await self.queued.get()
                if (method is not None and msg.get("method") == method
                        or rid is not None and msg.get("id") == rid and "method" not in msg):
                    return msg


@pytest.fixture
async def blocked_connection(tmp_path):
    hub = Hub(tmp_path, model=FakeModel(tool="reader.next_page"))
    sink = Backpressure()
    connection = Connection(hub, sink)
    connection.scope = hub.authority.issue("demo", "default")
    original_put = connection.queue.put

    async def observe(item):
        await original_put(item)
        sink.queued.put_nowait(json.loads(item[2]))

    connection.queue.put = observe
    blocker = asyncio.create_task(connection.notify("caps/changed", {}))
    errors = []
    loop = asyncio.get_running_loop()
    old_handler = loop.get_exception_handler()
    loop.set_exception_handler(lambda _, context: errors.append(context))
    try:
        await asyncio.wait_for(sink.started.wait(), 2)
        yield connection, sink
    finally:
        sink.release.set()
        await connection.close()
        await asyncio.gather(blocker, return_exceptions=True)
        gc.collect()
        loop.set_exception_handler(old_handler)
        assert not errors, errors


@pytest.mark.parametrize("change", ["permission", "member", "cancel"])
@pytest.mark.parametrize("method", ["tools/list", "tools/call"])
async def test_queued_request_is_invalidated(blocked_connection, change, method):
    connection, sink = blocked_connection
    task = asyncio.create_task(connection.request(method, {}))
    member = None
    try:
        await sink.wait_queued(method=method)
        if change == "permission":
            connection.hub.authority.add("demo", "default", "child")
        elif change == "member":
            member = asyncio.create_task(connection.receive(json.dumps({
                "jsonrpc": "2.0", "method": "ink/member", "params": {"member": "default", "epoch": 1}})))
            await sink.wait_queued(method="caps/changed")
            assert connection.epoch == 1
        else:
            task.cancel()
        sink.release.set()
        with pytest.raises(asyncio.CancelledError if change == "cancel" else ConnectionError):
            await asyncio.wait_for(task, 2)
        if member:
            await asyncio.wait_for(member, 2)
        await connection.queue.join()
        assert not [m for m in sink.sent if m.get("method") == method]
        assert not connection.pending
    finally:
        sink.release.set()
        task.cancel()
        await asyncio.gather(task, *([member] if member else []), return_exceptions=True)


async def test_queued_direct_result_rechecks_permission(blocked_connection):
    connection, sink = blocked_connection
    receive = asyncio.create_task(connection.receive(json.dumps({
        "jsonrpc": "2.0", "id": 10, "method": "tools/call",
        "params": {"name": "llm.ask", "arguments": {"question": "示例"}}})))
    try:
        await sink.wait_queued(rid=10)
        connection.hub.authority.remove("demo", "default")
        sink.release.set()
        await asyncio.wait_for(receive, 2)
        await connection.queue.join()
        assert not [m for m in sink.sent if m.get("id") == 10]
    finally:
        sink.release.set()
        await asyncio.gather(receive, return_exceptions=True)


async def test_member_change_discards_queued_turn_command(blocked_connection):
    connection, sink = blocked_connection
    connection.hub.model.text = ""  # Queue the tool itself, without a streamed preamble.
    connection.tools.definitions["development", "reader.next_page", 1] = {
        "name": "reader.next_page", "effect": "navigate", "inputSchema": {"type": "object"}}
    await connection.receive(json.dumps({"jsonrpc": "2.0", "id": 10, "method": "ai.ask",
                                         "params": {"question": "示例", "page": {"actions": ["reader.next_page"]}}}))
    await sink.wait_queued(method="tools/call")
    member = asyncio.create_task(connection.receive(json.dumps({
        "jsonrpc": "2.0", "method": "ink/member", "params": {"member": "default", "epoch": 1}})))
    try:
        await sink.wait_queued(method="caps/changed")
        sink.release.set()
        await asyncio.wait_for(member, 2)
        await connection.queue.join()
        assert connection.epoch == 1
        assert not [m for m in sink.sent if m.get("method") == "tools/call"]
        assert not connection.pending
    finally:
        sink.release.set()
        await asyncio.gather(member, return_exceptions=True)


async def test_reply_is_rechecked_before_returning(make_hub):
    hub, client, url = await make_hub()
    device = FakeDevice(client, url)
    await device.connect()
    connection = hub.connections["example-device"]
    task = asyncio.create_task(connection.request("tools/list", {}))
    request = await device.recv()
    connection.pending[request["id"]].set_result({"tools": []})
    hub.authority.add("demo", "default", "child")
    with pytest.raises(ConnectionError):
        await asyncio.wait_for(task, 2)
    assert not connection.pending


async def test_session_guard_reaches_device_request(blocked_connection):
    connection, sink = blocked_connection
    session = Session("voice.chat", connection.epoch)
    connection.sessions[7] = session
    guard = lambda: connection.valid_output(connection.scope, connection.epoch, 7, session)
    task = asyncio.create_task(connection.request("tools/list", {}, guard=guard))
    try:
        await sink.wait_queued(method="tools/list")
        await connection.end_session(7)
        sink.release.set()
        with pytest.raises(ConnectionError):
            await asyncio.wait_for(task, 2)
        await connection.queue.join()
        assert not [m for m in sink.sent if m.get("method") == "tools/list"]
    finally:
        sink.release.set()
        task.cancel()
        await asyncio.gather(task, return_exceptions=True)


async def test_close_releases_queued_and_inflight_senders(blocked_connection):
    connection, sink = blocked_connection
    task = asyncio.create_task(connection.request("tools/list", {}))
    try:
        await sink.wait_queued(method="tools/list")
        await asyncio.wait_for(connection.close(), 2)
        with pytest.raises(ConnectionError):
            await asyncio.wait_for(task, 2)
        await connection.queue.join()
        assert not connection.pending
        assert not [m for m in sink.sent if m.get("method") == "tools/list"]
    finally:
        task.cancel()
        await asyncio.gather(task, return_exceptions=True)


async def test_queued_dictation_drops_revoked_output(blocked_connection):
    connection, sink = blocked_connection
    session = Session("asr.stream", connection.epoch, frames=[b"example"])
    connection.sessions[4] = session
    task = asyncio.create_task(run_dictation(connection, 4, session))
    try:
        await sink.wait_queued(method="rt.asr")
        connection.hub.authority.add("demo", "default", "child")
        sink.release.set()
        await asyncio.wait_for(task, 2)
        await connection.queue.join()
        assert not [m for m in sink.sent if m.get("method") in ("rt.asr", "rt.asr.done")]
    finally:
        sink.release.set()
        task.cancel()
        await asyncio.gather(task, return_exceptions=True)


async def test_direct_calls_have_bounded_concurrency(make_hub):
    model = PausingModel()
    hub, client, url = await make_hub(model=model)
    device = FakeDevice(client, url)
    await device.connect()
    try:
        for _ in range(4):
            await device.send("tools/call", {"name": "llm.ask", "arguments": {"question": "示例"}})
        await asyncio.wait_for(model.started.wait(), 2)
        rejected = await asyncio.wait_for(device.call("tools/call", {
            "name": "llm.ask", "arguments": {"question": "示例"}}), 2)
        assert rejected["error"]["code"] == -32022
        assert model.calls == 4
        assert "result" in await device.call("caps/list")
    finally:
        model.gate.set()


async def test_direct_vendor_failure_returns_error(make_hub):
    _, client, url = await make_hub(model=FakeModel(fail=True))
    device = FakeDevice(client, url)
    await device.connect()
    reply = await device.call("tools/call", {"name": "llm.ask", "arguments": {"question": "示例"}})
    assert reply["error"]["code"] == -32603
    assert "result" in await device.call("caps/list")


@pytest.mark.parametrize("output", ["receipt", "error"])
async def test_revoked_turn_tail_finishes_without_exception(blocked_connection, output):
    connection, sink = blocked_connection
    connection.tools.definitions["development", "reader.next_page", 1] = {
        "name": "reader.next_page", "effect": "navigate", "inputSchema": {"type": "object"}}
    params = {"question": "", "page": {"actions": ["reader.next_page"]}}
    if output == "error":
        params["budget_ms"] = -1
    await connection.receive(json.dumps({"jsonrpc": "2.0", "id": 10, "method": "ai.ask", "params": params}))
    task = connection.turns[10]
    try:
        if output == "receipt":
            await sink.wait_queued(rid=10)
        else:
            await sink.wait_queued(method="ai.delta")
        connection.hub.authority.remove("demo", "default")
        sink.release.set()
        await asyncio.wait_for(task, 2)
        await connection.queue.join()
        assert not [m for m in sink.sent if m.get("id") == 10 or m.get("method") == "ai.delta"]
    finally:
        sink.release.set()
        task.cancel()
        await asyncio.gather(task, return_exceptions=True)
