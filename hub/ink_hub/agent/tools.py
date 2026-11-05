from ink_hub.gateway.wire import ProtocolError


class DeviceTools:
    def __init__(self, request, fw="development", manifest_hash=""):
        self.request = request
        self.fw, self.manifest_hash = fw, manifest_hash
        self.definitions = {}

    async def current(self, actions, *, guard=None):
        known = {key[1] for key in self.definitions}
        if actions and set(actions) <= known:
            return {name: self.definitions[next(k for k in self.definitions if k[1] == name)] for name in actions}
        view = {}
        cursor = None
        seen = set()
        for _ in range(256):
            reply = await self.request("tools/list", {"cursor": cursor} if cursor else {}, guard=guard)
            if not isinstance(reply, dict) or not isinstance(reply.get("tools"), list):
                raise ProtocolError("设备工具目录无效")
            for tool in reply["tools"]:
                if not isinstance(tool, dict) or not isinstance(tool.get("name"), str):
                    raise ProtocolError("设备工具描述无效")
                name = tool["name"]
                self.definitions[self.fw, name, tool.get("version", 1)] = tool
                view[name] = tool
            cursor = reply.get("nextCursor")
            if cursor is None:
                return {n: t for n, t in view.items() if not actions or n in actions}
            if not isinstance(cursor, str) or cursor in seen:
                raise ProtocolError("设备分页没有前进")
            seen.add(cursor)
        raise ProtocolError("设备目录页数超限")

    def invalidate(self, manifest_hash):
        if manifest_hash != self.manifest_hash:
            self.definitions.clear()
            self.manifest_hash = manifest_hash
