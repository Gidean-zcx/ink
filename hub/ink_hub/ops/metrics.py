import asyncio
import resource
import sys
from collections import deque


class Metrics:
    def __init__(self):
        self.lag = deque(maxlen=10000)
        self.turns = 0
        self.tool_calls = 0
        self.event_gaps = 0

    async def monitor(self):
        loop = asyncio.get_running_loop()
        while True:
            target = loop.time() + 0.01
            future = loop.create_future()
            handle = loop.call_at(target, lambda: future.set_result(None) if not future.done() else None)
            try:
                await future
            finally:
                handle.cancel()
            self.lag.append(max(0, (loop.time() - target) * 1000))

    def snapshot(self):
        values = sorted(self.lag) or [0]
        peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        return {"peak_rss_bytes": peak if sys.platform == "darwin" else peak * 1024,
                "loop_samples": len(self.lag), "loop_p95_ms": values[int((len(values) - 1) * 0.95)],
                "loop_max_ms": values[-1], "turns": self.turns, "tool_calls": self.tool_calls}
