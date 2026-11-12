from dataclasses import dataclass
import asyncio
from typing import Protocol
from ink_hub.models import from_environment


@dataclass(frozen=True)
class ModelResult:
    text: str
    tool: str | None = None
    arguments: dict | None = None


class Model(Protocol):
    async def answer(self, question: str, tools: list[str], lang: str = "zh") -> ModelResult: ...


class FakeModel:
    def __init__(self, text="这是示例回答。", tool=None, arguments=None, gate=None, fail=False,
                 texts=None):
        self.text, self.tool, self.arguments = text, tool, arguments or {}
        # The device's interface language picks the answer's language (device protocol "lang").
        self.texts = {"en": "This is a sample answer.", **(texts or {})}
        self.gate, self.fail = gate, fail
        self.calls = 0
        self.started = asyncio.Event()
        # Preserve the gateway's existing default entry point and injectable fake.
        self.provider = from_environment()

    async def answer(self, question, tools, lang="zh"):
        self.calls += 1
        self.started.set()
        if self.provider is not None:
            return await self.provider.answer(question, tools, lang)
        if self.gate:
            await self.gate.wait()
        if self.fail:
            raise RuntimeError("fake vendor failure")
        return ModelResult(self.texts.get(lang, self.text), self.tool if self.tool in tools else None, self.arguments)
