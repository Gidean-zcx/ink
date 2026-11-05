import asyncio
import threading
from concurrent.futures import ThreadPoolExecutor

import pytest

from ink_hub.jobs.runner import Runner
from tests.recovery.test_jobs import queue


@pytest.mark.parametrize("failure", [False, True])
async def test_cancel_running_job_keeps_scheduler_alive(queue, monkeypatch, failure):
    q, scope = queue
    started, release = asyncio.Event(), asyncio.Event()
    calls = 0

    async def handler(owner, payload):
        nonlocal calls
        calls += 1
        if calls == 1:
            started.set()
            await release.wait()
            if failure:
                raise RuntimeError("synthetic handler failure")
        return {"private": "synthetic private result"}

    monkeypatch.setattr("ink_hub.jobs.runner.example_hash", handler)
    runner = Runner(q)
    first = await runner.submit(scope, "first", "example.hash", {"text": "synthetic example"})
    task = asyncio.create_task(runner.run())
    try:
        await asyncio.wait_for(started.wait(), 2)
        q.cancel(scope, first)
        second = await runner.submit(scope, "second", "example.hash", {"text": "synthetic example"})
        release.set()
        row = await asyncio.wait_for(runner.wait(scope, second), 2)
        assert row["state"] == "done"
        assert not task.done()
        cancelled = q.get(scope, first)
        assert cancelled["state"] == "cancelled" and cancelled["result"] is None
        assert calls == 2
        assert await runner.submit(scope, "first", "example.hash", {"text": "synthetic example"}) == first
        assert q.get(scope, first)["attempts"] == 1
    finally:
        release.set()
        task.cancel()
        await asyncio.gather(task, return_exceptions=True)


@pytest.mark.parametrize("first", ["cancel", "finish"])
def test_cancel_and_finish_respect_transaction_order(queue, first):
    q, scope = queue
    jid = q.enqueue(scope, "key", "example", {})
    q.claim()
    if first == "cancel":
        q.cancel(scope, jid)
        q.finish(scope, jid, {"private": "synthetic result"})
        row = q.get(scope, jid)
        assert row["state"] == "cancelled" and row["result"] is None
    else:
        q.finish(scope, jid, {"ok": True})
        q.cancel(scope, jid)
        assert q.get(scope, jid)["state"] == "done"


def test_concurrent_cancels_are_idempotent(queue):
    q, scope = queue
    jid = q.enqueue(scope, "key", "example", {})
    q.claim()
    barrier = threading.Barrier(2)

    def cancel():
        barrier.wait(timeout=2)
        q.cancel(scope, jid)

    with ThreadPoolExecutor(max_workers=2) as executor:
        tasks = [executor.submit(cancel) for _ in range(2)]
        for task in tasks:
            task.result(timeout=2)
    row = q.get(scope, jid)
    assert row["state"] == "cancelled" and row["result"] is None


async def test_cancel_before_external_dispatch_prevents_handler(queue, monkeypatch):
    q, scope = queue
    jid = q.enqueue(scope, "key", "example", {}, effect="external")
    original_claim = q.claim

    def claim_then_cancel():
        row = original_claim()
        q.cancel(scope, jid)
        return row

    monkeypatch.setattr(q, "claim", claim_then_cancel)

    async def handler(owner, payload):
        pytest.fail("cancelled external job executed")

    assert await q.run_one({"example": handler})
    row = q.get(scope, jid)
    assert row["state"] == "cancelled" and row["result"] is None


@pytest.mark.parametrize("effect", ["none", "external"])
async def test_revocation_during_handler_failure_does_not_stop_queue(queue, effect):
    q, scope = queue
    jid = q.enqueue(scope, "key", "example", {}, effect=effect)

    async def handler(owner, payload):
        q.authority.remove("example", "a")
        raise RuntimeError("synthetic failure after revocation")

    assert await q.run_one({"example": handler})
    q.authority.add("example", "a")
    row = q.get(q.authority.issue("example", "a"), jid)
    assert row["state"] == ("unknown" if effect == "external" else "cancelled")
    assert row["result"] is None


async def test_revocation_before_external_dispatch_does_not_stop_queue(queue, monkeypatch):
    q, scope = queue
    jid = q.enqueue(scope, "key", "example", {}, effect="external")
    original_claim = q.claim

    def claim_then_revoke():
        row = original_claim()
        q.authority.remove("example", "a")
        return row

    monkeypatch.setattr(q, "claim", claim_then_revoke)

    async def handler(owner, payload):
        pytest.fail("revoked external job executed")

    assert await q.run_one({"example": handler})
    q.authority.add("example", "a")
    row = q.get(q.authority.issue("example", "a"), jid)
    assert row["state"] == "unknown" and row["result"] is None
