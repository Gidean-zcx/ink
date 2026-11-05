from dataclasses import dataclass
import asyncio
from typing import Protocol


@dataclass(frozen=True)
class ModelResult:
    text: str
    tool: str | None = None
    arguments: dict | None = None


class Model(Protocol):
    async def answer(self, question: str, tools: list[str]) -> ModelResult: ...


class FakeModel:
    def __init__(self, text="这是示例回答。", tool=None, arguments=None, gate=None, fail=False):
        self.text, self.tool, self.arguments = text, tool, arguments or {}
        self.gate, self.fail = gate, fail
        self.calls = 0
        self.started = asyncio.Event()

    async def answer(self, question, tools):
        self.calls += 1
        self.started.set()
        if self.gate:
            await self.gate.wait()
        if self.fail:
            raise RuntimeError("fake vendor failure")
        return ModelResult(self.text, self.tool if self.tool in tools else None, self.arguments)
