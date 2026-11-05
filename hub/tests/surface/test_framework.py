import asyncio
import subprocess

import pytest

from ink_hub.bus.schema import validate
from ink_hub.gateway.wire import ProtocolError
from ink_hub.jobs.queue import JobQueue
from ink_hub.jobs.runner import Runner
from ink_hub.data.scope import Authority
from ink_hub.ops.preflight import inspect


@pytest.mark.parametrize("value", [{}, {"count": True}, {"count": 0}, {"count": 5}, {"count": "1"}, {"count": 1, "_meta": {"asked": True}}])
def test_schema_counterexamples(value):
    schema = {"type": "object", "required": ["count"], "additionalProperties": False,
              "properties": {"count": {"type": "integer", "minimum": 1, "maximum": 4}}}
    validate({"count": 2}, schema)
    with pytest.raises(ProtocolError):
        validate(value, schema)


def test_unsupported_schema_cannot_silently_drop_constraints():
    with pytest.raises(ProtocolError):
        validate({"count": 1}, {"type": "object", "not": {"required": ["count"]}})


def test_preflight_resource_probe_can_be_unavailable(tmp_path, monkeypatch):
    monkeypatch.setattr("platform.system", lambda: "Darwin")
    def denied(*args, **kwargs):
        raise subprocess.CalledProcessError(1, "sysctl")
    monkeypatch.setattr(subprocess, "check_output", denied)
    assert inspect(tmp_path, ())['memory_bytes'] is None


async def test_scheduler_executes_persisted_job_after_reopen(tmp_path):
    auth = Authority()
    auth.add("example", "member")
    scope = auth.issue("example", "member")
    queue = JobQueue(tmp_path / "hub.db", auth)
    jid = queue.enqueue(scope, "example-key", "example.hash", {"text": "synthetic example"})
    runner = Runner(JobQueue(queue.path, auth))
    task = asyncio.create_task(runner.run())
    try:
        row = await asyncio.wait_for(runner.wait(scope, jid), 3)
        assert row["state"] == "done"
        assert await runner.submit(scope, "example-key", "example.hash", {"text": "synthetic example"}) == jid
        assert queue.get(scope, jid)["attempts"] == 1
    finally:
        task.cancel()
        await asyncio.gather(task, return_exceptions=True)
