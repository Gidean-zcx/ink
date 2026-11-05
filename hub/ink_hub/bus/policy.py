import hashlib
import json
from dataclasses import dataclass

from ink_hub.data.scope import Scope


def digest(arguments):
    value = json.dumps(arguments, ensure_ascii=False, sort_keys=True, separators=(",", ":"), allow_nan=False)
    return hashlib.sha256(value.encode()).hexdigest()


@dataclass(frozen=True)
class ActionRange:
    scope: Scope
    connection: str
    epoch: int
    action: str
    target: str
    materials: tuple[str, ...]
    expires_at: float
    arguments_digest: str

    def permits(self, scope, connection, epoch, action, target, materials, arguments, now):
        return (self.scope == scope and self.connection == connection and self.epoch == epoch
                and self.action == action and self.target == target
                and set(materials) <= set(self.materials) and now < self.expires_at
                and self.arguments_digest == digest(arguments))


def restricted_context(human, grant, named_materials, tools):
    """No implicit memory/history/dictation in an acting context (§7.5)."""
    if set(named_materials) - set(grant.materials):
        raise PermissionError("资料超出这次要求")
    return {"human": human, "materials": dict(named_materials),
            "tools": [t for t in tools if t == grant.action]}


def authorize(authority, scope, *, epoch, connection, action, arguments, now, effect="read",
              child_ok=True, grant=None, target="", materials=(), high_risk=False):
    authority.check(scope)
    if authority.role(scope) == "child" and (not child_ok or high_risk or effect == "system"):
        raise PermissionError("请家长操作")
    if high_risk:
        raise PermissionError("需要本人在设备上确认")
    asked = False
    if effect in ("external", "system"):
        if not grant or not grant.permits(scope, connection, epoch, action, target, materials, arguments, now):
            raise PermissionError("动作超出这次要求")
        asked = True
    return {"member": scope.member, "epoch": epoch, "asked": asked}
