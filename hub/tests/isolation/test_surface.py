import pytest

from ink_hub.gateway.wire import ProtocolError


async def test_every_installed_call_has_scope_and_describes_arguments(make_hub):
    hub, _, _ = await make_hub()
    cases = {"llm.ask": {"question": "示例"}, "tts.speak": {"text": "示例"},
             "asr.transcribe": {"example": True}, "job.status": {"id": "unknown-example"}}
    assert {c.name for c in hub.registry.visible("child", form="call")} == set(cases)
    for family in ("synthetic-a", "synthetic-b"):
        hub.authority.add(family, "adult-a")
        hub.authority.add(family, "adult-b")
        hub.authority.add(family, "child", "child", ("adult-a", "adult-b"))
        for member in ("adult-a", "adult-b", "child"):
            scope = hub.authority.issue(family, member)
            for cap in hub.registry.visible(hub.authority.role(scope), form="call"):
                arguments = cases[cap.name]
                assert set(cap.tool()["inputSchema"]["required"]) == set(arguments)
                result = await hub.registry.call(scope, cap.name, arguments, hub.authority)
                assert result["status"] == "done"
                with pytest.raises(ProtocolError):
                    await hub.registry.call(scope, cap.name, {**arguments, "_meta": {"member": "adult-b"}}, hub.authority)
            hub.authority.remove(family, member)
            for name, arguments in cases.items():
                with pytest.raises(PermissionError):
                    await hub.registry.call(scope, name, arguments, hub.authority)
