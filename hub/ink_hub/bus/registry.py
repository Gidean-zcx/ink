from dataclasses import dataclass

from ink_hub.gateway.wire import ProtocolError
from .schema import validate


def object_schema(properties):
    return {"type": "object", "properties": properties, "required": list(properties), "additionalProperties": False}


@dataclass(frozen=True)
class Capability:
    name: str
    title: str
    handler: object = None
    form: str = "call"
    child_ok: bool = True
    effect: str = "read"
    expose: frozenset = frozenset({"app", "ai"})
    input_schema: dict | None = None

    def directory(self):
        entry = {"name": self.name}
        if self.form != "call":
            entry["form"] = self.form
        return entry

    def tool(self):
        return {"name": self.name, "title": self.title, "effect": self.effect,
                "confirm": "none", "inputSchema": self.input_schema or {"type": "object", "properties": {}}}


class Registry:
    def __init__(self):
        self._caps = {}

    def register(self, cap):
        if cap.name in self._caps or len(cap.name.encode()) > 39:
            raise ValueError("能力名重复或过长")
        self._caps[cap.name] = cap

    def visible(self, role, source="app", form=None):
        return [c for c in self._caps.values() if source in c.expose and (role != "child" or c.child_ok)
                and (form is None or c.form == form)]

    async def call(self, scope, name, arguments, authority):
        authority.check(scope)
        cap = self._caps.get(name)
        if not cap or cap.form != "call" or "app" not in cap.expose:
            raise ProtocolError("未提供这个能力", -32601)
        if authority.role(scope) == "child" and not cap.child_ok:
            raise ProtocolError("请家长操作", -32020)
        if not isinstance(arguments, dict) or "_meta" in arguments:
            raise ProtocolError("参数无效")
        validate(arguments, cap.input_schema or {"type": "object"})
        result = await cap.handler(scope, arguments)
        authority.check(scope)
        return result
