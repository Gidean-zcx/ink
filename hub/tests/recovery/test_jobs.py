import pytest

from ink_hub.data.scope import Authority
from ink_hub.jobs.queue import JobQueue


@pytest.fixture
def queue(tmp_path):
    auth = Authority()
    auth.add("example", "a")
    auth.add("example", "b")
    return JobQueue(tmp_path / "hub.db", auth), auth.issue("example", "a")


def test_queued_and_safe_running_recover(queue):
    q, scope = queue
    jid = q.enqueue(scope, "key", "example", {})
    assert q.claim(now=1, lease_s=2)["id"] == jid
    reopened = JobQueue(q.path, q.authority)
    reopened.recover(now=2)
    assert reopened.get(scope, jid)["state"] == "running"
    reopened.recover(now=4)
    assert reopened.claim(now=4)["attempts"] == 2
    reopened.finish(scope, jid, {"example": True})
    assert q.enqueue(scope, "key", "example", {}) == jid
    assert not reopened.claim(now=100)


def test_uncertain_external_never_replayed(queue):
    q, scope = queue
    jid = q.enqueue(scope, "key", "example", {}, effect="external")
    q.claim(now=1)
    q.external_started(scope, jid)
    reopened = JobQueue(q.path, q.authority)
    reopened.recover(now=2)
    assert reopened.get(scope, jid)["state"] == "unknown"
    assert reopened.claim(now=1000) is None
    with pytest.raises(ValueError):
        reopened.finish(scope, jid, {})


def test_duplicate_key_has_owner_and_arguments(queue):
    q, scope = queue
    jid = q.enqueue(scope, "key", "example", {"x": 1})
    assert jid == q.enqueue(scope, "key", "example", {"x": 1})
    with pytest.raises(ValueError):
        q.enqueue(scope, "key", "example", {"x": 2})
    other = q.authority.issue("example", "b")
    assert q.get(other, jid) is None
    assert q.enqueue(other, "key", "example", {}) != jid


def test_permissions_rechecked_before_claim(queue):
    q, scope = queue
    jid = q.enqueue(scope, "key", "example", {})
    q.authority.remove("example", "a")
    assert q.claim() is None
    with pytest.raises(PermissionError):
        q.get(scope, jid)


def test_cancelled_job_stays_cancelled(queue):
    q, scope = queue
    jid = q.enqueue(scope, "key", "example", {})
    q.cancel(scope, jid)
    q.recover(now=100)
    assert q.get(scope, jid)["state"] == "cancelled"
    assert q.claim() is None


async def test_runner_deduplicates_success(queue):
    q, scope = queue
    count = 0
    async def handler(owner, payload):
        nonlocal count
        count += 1
        return {"example": payload}
    jid = q.enqueue(scope, "key", "example", {})
    assert await q.run_one({"example": handler})
    assert q.get(scope, jid)["state"] == "done"
    q.enqueue(scope, "key", "example", {})
    assert not await q.run_one({"example": handler})
    assert count == 1


async def test_runner_failure_after_external_request_is_unknown(queue):
    q, scope = queue
    async def handler(owner, payload):
        raise ConnectionError("fake disconnect")
    jid = q.enqueue(scope, "key", "example", {}, effect="external")
    await q.run_one({"example": handler})
    assert q.get(scope, jid)["state"] == "unknown"
