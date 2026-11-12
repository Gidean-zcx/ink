"""Protocol 2 enrollment, driven by the device and a local human claim."""
import asyncio
import secrets

from aiohttp import web

from .tls import loopback
from .wire import ProtocolError, response


class Pairing:
    def __init__(self, hub):
        self.hub, self.codes = hub, {}

    async def expire(self, code):
        entry = self.codes.pop(code, None)
        if entry:
            entry["timer"].cancel()
            connection = entry["connection"]
            if not connection.closed:
                await connection.notify("pair/expired", {})

    async def begin(self, connection, rid, params):
        if params.get("proto") != 2:
            raise ProtocolError("proto_unsupported", -32012)
        if not self.hub.lan:
            raise ProtocolError("unavailable", -32021)
        nonce = params.get("nonce")
        if not isinstance(nonce, str) or not 1 <= len(nonce.encode()) <= 63:
            raise ProtocolError("配对随机数无效")
        for code, entry in list(self.codes.items()):
            if entry["expires"] <= self.hub.clock():
                await self.expire(code)
        found = next(((code, entry) for code, entry in self.codes.items() if entry["nonce"] == nonce), None)
        if not found and len(self.codes) >= 32:
            raise ProtocolError("busy", -32022)
        txid = params.get("txid")
        credentials = None
        if txid is not None:
            if not isinstance(txid, str) or not 1 <= len(txid) <= 39:
                raise ProtocolError("pair_expired", -32031)
            credentials = self.hub.tokens.pending(nonce, txid, resume=True)
        elif "preauth" in params:
            if not isinstance(params["preauth"], str):
                raise ProtocolError("pair_code_invalid", -32030)
            self.hub.tokens.consume(params["preauth"])
            txid = secrets.token_hex(16)
            credentials = self.hub.tokens.pending(nonce, txid)
        if found:
            code, entry = found
            old = entry["connection"]
            if old is not connection:
                await old.close()
            entry["connection"] = connection
        else:
            code = "".join(secrets.choice("ABCDEFGHJKLMNPQRSTUVWXYZ23456789") for _ in range(8))
            entry = {"nonce": nonce, "connection": connection, "expires": self.hub.clock() + 600, "errors": 0}
            entry["timer"] = asyncio.get_running_loop().call_later(600, lambda: asyncio.create_task(self.expire(code)))
            self.codes[code] = entry
        connection.pair_code = code
        await connection.send(response(rid, {"code": code, "expires_s": max(0, round(entry["expires"] - self.hub.clock()))}))
        if credentials:
            entry["txid"] = txid
            await self.complete(entry, credentials)

    async def outbound(self, connection, method, params):
        connection.rid += 1
        await connection.send({"jsonrpc": "2.0", "id": connection.rid, "method": method, "params": params})

    async def complete(self, entry, credentials=None):
        device, token = credentials or self.hub.tokens.pending(entry["nonce"], entry["txid"])
        await self.outbound(entry["connection"], "pair/complete", {
            "txid": entry["txid"], "device": device, "token": token,
            "urls": [self.hub.lan["url"]], "pins": [self.hub.lan["pin"]],
            "members_rev": 1, "now_ms": int(self.hub.clock() * 1000)})

    async def receive(self, connection, rid, method, params):
        if method == "pair/begin":
            await self.begin(connection, rid, params)
            return
        entry = self.codes.get(connection.pair_code)
        if (not entry or entry["connection"] is not connection or not entry.get("txid")
                or params.get("txid") != entry["txid"]):
            raise ProtocolError("pair_code_invalid", -32030)
        if entry["expires"] <= self.hub.clock():
            await self.expire(connection.pair_code)
            raise ProtocolError("pair_expired", -32031)
        if method not in ("pair/accept", "pair/reject"):
            raise ProtocolError("未提供这个方法", -32601)
        await connection.send(response(rid, {}))
        if method == "pair/accept":
            await self.complete(entry)
        else:
            entry.pop("txid", None)

    async def claim(self, request):
        if not loopback(request.remote) or request.secure:
            raise web.HTTPForbidden()
        params = await request.json()
        entry = self.codes.get(params.get("code"))
        if not entry or entry["expires"] <= self.hub.clock():
            raise web.HTTPBadRequest(text="认领码无效或已过期")
        if not secrets.compare_digest(str(params.get("check", "")).encode(), self.hub.lan["code"].encode()):
            entry["errors"] += 1
            if entry["errors"] >= 5:
                await self.expire(params["code"])
            raise web.HTTPForbidden(text="核对码不对")
        name = params.get("name", "我的设备")
        if not isinstance(name, str) or not 1 <= len(name.encode()) <= 63:
            raise web.HTTPBadRequest(text="设备名过长或无效")
        entry["txid"] = secrets.token_hex(16)
        await self.outbound(entry["connection"], "pair/offer", {"txid": entry["txid"],
                            "family": "我的家", "by": "本机用户", "name": name})
        return web.json_response({"offered": True})
