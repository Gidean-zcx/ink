from dataclasses import dataclass


@dataclass(frozen=True)
class Scope:
    family: str
    actor: str
    member: str
    permission_version: int


class Authority:
    def __init__(self):
        self._members = {}

    def add(self, family, member, role="adult", parents=()):
        old = self._members.get((family, member))
        self._members[family, member] = (role, frozenset(parents), old[2] + 1 if old else 1)

    def remove(self, family, member):
        role, parents, version = self._members[family, member]
        self._members[family, member] = ("removed", parents, version + 1)

    def issue(self, family, actor, member=None):
        member = member or actor
        caller = self._members.get((family, actor))
        target = self._members.get((family, member))
        if not caller or not target or "removed" in (caller[0], target[0]):
            raise PermissionError("家人不存在")
        if actor != member and not (target[0] == "child" and actor in target[1] and caller[0] == "adult"):
            raise PermissionError("不能查看其他大人的资料")
        # Both versions matter when a parent reads a child's data.
        version = caller[2] << 32 | target[2]
        return Scope(family, actor, member, version)

    def check(self, scope):
        if self.issue(scope.family, scope.actor, scope.member) != scope:
            raise PermissionError("权限已变化")

    def role(self, scope):
        self.check(scope)
        return self._members[scope.family, scope.actor][0]
