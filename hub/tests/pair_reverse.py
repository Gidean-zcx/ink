"""Manual negative controls; exit 1 is the required regression signal."""
import inspect
import os
import sys

for key in list(os.environ):
    if key == "DEEPSEEK_API_KEY" or key.startswith("INK_HUB_"):
        del os.environ[key]

import pytest

if sys.argv[1] == "claim":
    import ink_hub.gateway.pairing as pairing
    pairing.secrets.compare_digest = lambda *_: True
    target = "hub/tests/contract/test_pairing.py::test_claim_and_restart_keep_hashed_token"
elif sys.argv[1] == "schema":
    import ink_hub.voice.flow as flow
    import ink_hub.gateway.server as server
    from ink_hub.models.remote import RemoteModel
    original_stream = RemoteModel.stream

    async def names_only(self, question, tools, lang="zh", member=None):
        # Preserve the old adapter's behavior, rather than crashing on the old argument type.
        tools = {name: {"title": "设备能力：" + name, "inputSchema": {
            "type": "object", "properties": {}, "additionalProperties": True}} for name in tools}
        async for part in original_stream(self, question, tools, lang, member):
            yield part

    RemoteModel.stream = names_only
    source = inspect.getsource(flow.run_turn)
    old = "question, available, lang=lang"
    assert old in source
    namespace = dict(vars(flow))
    exec(source.replace(old, "question, list(available), lang=lang"), namespace)
    server.run_turn = namespace["run_turn"]
    target = "hub/tests/contract/test_hub_pair_model.py::test_real_schema_and_parameters_reach_device"
else:
    raise SystemExit("choose claim or schema")
raise SystemExit(pytest.main(["-q", target]))
