import json
import sqlite3
import time
import uuid
from contextlib import contextmanager
from dataclasses import asdict

from ink_hub.data.scope import Scope
from ink_hub.bus.policy import digest


class JobQueue:
    def __init__(self, path, authority):
        self.path, self.authority = path, authority
        with self._open() as db:
            db.execute("""CREATE TABLE IF NOT EXISTS jobs(
                id TEXT PRIMARY KEY, owner TEXT, key TEXT, scope TEXT, kind TEXT,
                payload TEXT, fingerprint TEXT, effect TEXT, state TEXT, attempts INTEGER,
                lease REAL, result TEXT, UNIQUE(owner,key))""")

    @contextmanager
    def _open(self):
        db = sqlite3.connect(self.path, timeout=5)
        db.row_factory = sqlite3.Row
        try:
            with db:
                yield db
        finally:
            db.close()

    @staticmethod
    def _owner(scope):
        return json.dumps([scope.family, scope.actor, scope.member])

    def enqueue(self, scope, key, kind, payload, effect="none"):
        self.authority.check(scope)
        if effect not in ("none", "external"):
            raise ValueError("unknown effect")
        owner = self._owner(scope)
        fingerprint = digest({"kind": kind, "payload": payload, "effect": effect})
        with self._open() as db:
            db.execute("BEGIN IMMEDIATE")
            old = db.execute("SELECT * FROM jobs WHERE owner=? AND key=?", (owner, key)).fetchone()
            if old:
                if old["fingerprint"] != fingerprint:
                    raise ValueError("idempotency key reused with other arguments")
                return old["id"]
            jid = uuid.uuid4().hex
            db.execute("INSERT INTO jobs VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
                       (jid, owner, key, json.dumps(asdict(scope)), kind, json.dumps(payload), fingerprint,
                        effect, "queued", 0, 0, None))
            return jid

    def get(self, scope, jid):
        self.authority.check(scope)
        with self._open() as db:
            row = db.execute("SELECT * FROM jobs WHERE id=? AND owner=?", (jid, self._owner(scope))).fetchone()
            return dict(row) if row else None

    def recover(self, now=None):
        now = time.time() if now is None else now
        with self._open() as db:
            db.execute("UPDATE jobs SET state='unknown' WHERE state='external'")
            db.execute("UPDATE jobs SET state=CASE WHEN effect='none' THEN 'queued' ELSE 'unknown' END "
                       "WHERE state='running' AND lease<=?", (now,))
            db.execute("UPDATE jobs SET state='cancelled' WHERE state='cancelling'")

    def claim(self, now=None, lease_s=30):
        now = time.time() if now is None else now
        with self._open() as db:
            db.execute("BEGIN IMMEDIATE")
            rows = db.execute("SELECT * FROM jobs WHERE state='queued' ORDER BY rowid").fetchall()
            for row in rows:
                try:
                    self.authority.check(Scope(**json.loads(row["scope"])))
                except PermissionError:
                    db.execute("UPDATE jobs SET state='cancelled' WHERE id=?", (row["id"],))
                    continue
                db.execute("UPDATE jobs SET state='running',attempts=attempts+1,lease=? WHERE id=?", (now + lease_s, row["id"]))
                return dict(db.execute("SELECT * FROM jobs WHERE id=?", (row["id"],)).fetchone())
        return None

    def external_started(self, scope, jid):
        self._transition(scope, jid, "running", "external")

    def finish(self, scope, jid, result):
        self._transition(scope, jid, ("running", "external"), "done", json.dumps(result))

    def cancel(self, scope, jid):
        row = self.get(scope, jid)
        if row and row["state"] in ("queued", "running"):
            self._transition(scope, jid, row["state"], "cancelled")

    def _transition(self, scope, jid, before, after, result=None):
        self.authority.check(scope)
        before = (before,) if isinstance(before, str) else before
        with self._open() as db:
            db.execute("BEGIN IMMEDIATE")
            row = db.execute("SELECT state FROM jobs WHERE id=? AND owner=?", (jid, self._owner(scope))).fetchone()
            if not row or row["state"] not in before:
                raise ValueError("invalid job transition")
            db.execute("UPDATE jobs SET state=?,result=? WHERE id=?", (after, result, jid))

    async def run_one(self, handlers):
        import asyncio
        row = await asyncio.to_thread(self.claim)
        if not row:
            return False
        scope = Scope(**json.loads(row["scope"]))
        handler = handlers.get(row["kind"])
        if not handler:
            await asyncio.to_thread(self.cancel, scope, row["id"])
            return True
        if row["effect"] == "external":
            await asyncio.to_thread(self.external_started, scope, row["id"])
        try:
            result = await handler(scope, json.loads(row["payload"]))
        except Exception:
            if row["effect"] == "external":
                await asyncio.to_thread(self.recover)
            else:
                await asyncio.to_thread(self.cancel, scope, row["id"])
            return True
        try:
            await asyncio.to_thread(self.finish, scope, row["id"], result)
        except PermissionError:
            # The owner was revoked while the handler awaited; never expose its result.
            with self._open() as db:
                db.execute("UPDATE jobs SET state=?,result=NULL WHERE id=? AND state IN ('running','external')",
                           ("unknown" if row["effect"] == "external" else "cancelled", row["id"]))
        return True
