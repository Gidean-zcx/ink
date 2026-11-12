"""Only hashes of enrollment credentials are durable."""
import hashlib
import secrets
import sqlite3
from contextlib import closing

from ink_hub.gateway.wire import ProtocolError


class PairingStore:
    def __init__(self, path, clock):
        self.path, self.clock = path, clock
        self.query("CREATE TABLE IF NOT EXISTS pairing_tokens(hash TEXT PRIMARY KEY, device TEXT, "
                   "nonce TEXT, txid TEXT, state TEXT, expires REAL)")

    def query(self, sql, args=()):
        with closing(sqlite3.connect(self.path)) as db, db:
            db.row_factory = sqlite3.Row
            return db.execute(sql, args).fetchall()

    @staticmethod
    def digest(token):
        return hashlib.sha256(token.encode()).hexdigest()

    def preauth(self):
        token = secrets.token_hex(24)
        self.query("INSERT INTO pairing_tokens VALUES(?,?,?,?,?,?)",
                   (self.digest(token), "", "", "", "preauth", self.clock() + 86400))
        return token

    def consume(self, token):
        rows = self.query("DELETE FROM pairing_tokens WHERE hash=? AND state='preauth' AND expires>? RETURNING hash",
                          (self.digest(token), self.clock()))
        if not rows:
            raise ProtocolError("pair_code_invalid", -32030)

    def pending(self, nonce, txid, resume=False):
        rows = self.query("SELECT * FROM pairing_tokens WHERE nonce=? AND txid=? AND state='pending' AND expires>?",
                          (nonce, txid, self.clock()))
        if resume and not rows:
            raise ProtocolError("pair_expired", -32031)
        device = rows[0]["device"] if rows else "dv_" + secrets.token_hex(12)
        token = secrets.token_hex(24)
        if rows:
            self.query("UPDATE pairing_tokens SET hash=?,expires=? WHERE hash=?",
                       (self.digest(token), self.clock() + 7 * 86400, rows[0]["hash"]))
        else:
            self.query("INSERT INTO pairing_tokens VALUES(?,?,?,?,?,?)",
                       (self.digest(token), device, nonce, txid, "pending", self.clock() + 7 * 86400))
        return device, token

    def authenticate(self, device, token):
        rows = self.query("SELECT * FROM pairing_tokens WHERE hash=? AND device=? AND state IN ('pending','active')",
                          (self.digest(token), device))
        if not rows:
            raise ProtocolError("auth_invalid", -32010)
        row = rows[0]
        if row["state"] == "pending" and row["expires"] <= self.clock():
            code = -32013 if self.clock() <= row["expires"] + 30 * 86400 else -32010
            raise ProtocolError("pending_token_expired" if code == -32013 else "auth_invalid", code)
        self.query("UPDATE pairing_tokens SET state='active' WHERE hash=?", (row["hash"],))
