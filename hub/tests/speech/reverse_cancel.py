"""Manual negative controls: mutate loaded functions only, never source files."""
import asyncio
import inspect
import sys
import textwrap

import pytest

from ink_hub.gateway import server
from ink_hub.gateway.server import Connection
from ink_hub.voice import flow, stream

case = sys.argv[1] if len(sys.argv) > 1 else "cancel"
if case == "cancel":
    source = textwrap.dedent(inspect.getsource(Connection.end_session))
    source = source.replace("task.cancel()", "pass").replace("await asyncio.gather(*tasks, return_exceptions=True)", "return")
    target, name, test = Connection, "end_session", "cancel_all_stages"
elif case == "legacy":
    source = textwrap.dedent(inspect.getsource(stream.recognize))
    source = source.replace("value = await asr.transcribe", "text = await asr.transcribe").replace("results = [(value, True)]", "results = [(text, True)]")
    target, name, test = stream, "recognize", "legacy_transcript"
elif case == "late":
    source = textwrap.dedent(inspect.getsource(Connection.receive))
    block = "            if session.ended.is_set():\n                return\n"
    assert block in source
    source = source.replace(block, "").replace("            session.seq = (seq + 1)", block + "            session.seq = (seq + 1)")
    target, name, test = Connection, "receive", "late_uplink"
elif case == "heard_complete":
    source = textwrap.dedent(inspect.getsource(Connection.end_session))
    source = source[:source.index("    if (session and session.history")]
    target, name, test = Connection, "end_session", "close_after_generation"
else:
    assert case in ("vendor", "model_close")
    source = textwrap.dedent(inspect.getsource(flow.run_turn))
    if case == "vendor":
        source = source.replace("except (ConnectionError, RuntimeError, TimeoutError, ClientError, ExceptionGroup, ValueError):", "except (ConnectionError, RuntimeError, TimeoutError):")
        test = "streaming_tts_failure"
    else:
        source = source.replace("async with aclosing(self.hub.model.stream(prompt, available, lang=lang, member=context)) as model_stream:",
                                "if True:\n                    model_stream = self.hub.model.stream(prompt, available, lang=lang, member=context)")
        test = "abort_closes_model"
    target, name = flow, "run_turn"
namespace = dict(getattr(target, name).__globals__)
exec(compile(source, "<negative-control>", "exec"), namespace)
setattr(target, name, namespace[name])
if name == "recognize":
    flow.recognize = namespace[name]
elif name == "run_turn":
    server.run_turn = namespace[name]
raise SystemExit(pytest.main(["-q", "hub/tests/speech/test_streaming.py", "-k", test]))
