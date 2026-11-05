import asyncio
from dataclasses import replace

import pytest

from ink_hub.bus.policy import ActionRange, authorize, digest, restricted_context
from ink_hub.data.scope import Authority
from ink_hub.data.store import Store


@pytest.fixture
def authority():
    auth = Authority()
    for family in ("family-a", "family-b"):
        auth.add(family, "adult-a")
        auth.add(family, "adult-b")
        auth.add(family, "child", "child", ("adult-a", "adult-b"))
    return auth


@pytest.mark.parametrize("family", ["family-a", "family-b"])
@pytest.mark.parametrize("actor", ["adult-a", "adult-b", "child"])
def test_member_and_family_partition(tmp_path, authority, family, actor):
    store = Store(tmp_path, authority)
    for f in ("family-a", "family-b"):
        for m in ("adult-a", "adult-b", "child"):
            store.put(authority.issue(f, m), "example", f + "/" + m)
    scope = authority.issue(family, actor)
    assert store.get(scope, "example") == family + "/" + actor
    if actor == "child":
        with pytest.raises(PermissionError):
            authority.issue(family, actor, "adult-a")
    else:
        other = "adult-b" if actor == "adult-a" else "adult-a"
        with pytest.raises(PermissionError):
            authority.issue(family, actor, other)
        assert store.get(authority.issue(family, actor, "child"), "example") == family + "/child"


def test_old_permission_version_rejected(tmp_path, authority):
    scope = authority.issue("family-a", "adult-a")
    store = Store(tmp_path, authority)
    store.put(scope, "example", "synthetic")
    authority.add("family-a", "adult-a")
    with pytest.raises(PermissionError):
        store.get(scope, "example")


def test_removed_parent_no_longer_reads_child(authority):
    scope = authority.issue("family-a", "adult-a", "child")
    authority.remove("family-a", "adult-a")
    with pytest.raises(PermissionError):
        authority.check(scope)


def grant_for(scope):
    return ActionRange(scope, "connection-example", 1, "message.send", "contact-example",
                       ("named-note",), 100, digest({"text": "示例"}))


@pytest.mark.parametrize("change", ["scope", "connection", "epoch", "action", "target", "materials", "arguments", "now"])
def test_each_range_rule_has_counterexample(authority, change):
    scope = authority.issue("family-a", "adult-a")
    grant = grant_for(scope)
    args = dict(scope=scope, connection="connection-example", epoch=1, action="message.send",
                target="contact-example", materials=("named-note",), arguments={"text": "示例"}, now=1)
    assert grant.permits(**args)
    changes = {"scope": authority.issue("family-b", "adult-a"), "connection": "other", "epoch": 3,
               "action": "call.start", "target": "someone-else", "materials": ("dictation-private",),
               "arguments": {"text": "不同内容"}, "now": 100}
    args[change] = changes[change]
    assert not grant.permits(**args)


def test_a_child_a_cannot_reuse_grant(authority):
    scope = authority.issue("family-a", "adult-a")
    grant = grant_for(scope)
    with pytest.raises(PermissionError):
        authorize(authority, scope, epoch=3, connection="connection-example", action=grant.action,
                  target=grant.target, arguments={"text": "示例"}, now=1, effect="external", grant=grant)


def test_model_cannot_self_assert_asked_and_high_risk_never_auto(authority):
    scope = authority.issue("family-a", "adult-a")
    kwargs = dict(epoch=1, connection="connection-example", action="message.send", target="contact-example",
                  arguments={"text": "示例"}, now=1, effect="external", grant=grant_for(scope))
    assert authorize(authority, scope, **kwargs)["asked"] is True
    with pytest.raises(PermissionError):
        authorize(authority, scope, **{**kwargs, "grant": None})
    with pytest.raises(PermissionError):
        authorize(authority, scope, **kwargs, high_risk=True)


def test_restricted_context_has_only_named_materials(authority):
    grant = grant_for(authority.issue("family-a", "adult-a"))
    context = restricted_context("示例原话", grant, {"named-note": "example"}, ["message.send", "memory.search", "dictation.list"])
    assert context == {"human": "示例原话", "materials": {"named-note": "example"}, "tools": ["message.send"]}
    with pytest.raises(PermissionError):
        restricted_context("示例原话", grant, {"private": "secret"}, [])


async def test_interleaved_outputs_keep_own_scope(tmp_path, authority):
    store = Store(tmp_path, authority)
    scopes = [authority.issue(f, m) for f in ("family-a", "family-b") for m in ("adult-a", "adult-b", "child")]
    async def operation(scope):
        value = scope.family + scope.member
        await asyncio.to_thread(store.put, scope, "example", value)
        assert await asyncio.to_thread(store.get, scope, "example") == value
    await asyncio.gather(*(operation(s) for s in scopes))
