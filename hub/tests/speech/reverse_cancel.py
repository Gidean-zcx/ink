"""Manual negative control: disable abort cancellation without changing files."""
import asyncio
import inspect
import textwrap

import pytest

from ink_hub.gateway.server import Connection

source = textwrap.dedent(inspect.getsource(Connection.end_session))
source = source.replace("task.cancel()", "pass").replace(
    "await asyncio.gather(*tasks, return_exceptions=True)", "return")
namespace = {"asyncio": asyncio}
exec(compile(source, "<negative-control>", "exec"), namespace)
Connection.end_session = namespace["end_session"]
raise SystemExit(pytest.main(["-q", "hub/tests/speech/test_streaming.py", "-k", "cancel_all_stages"]))
