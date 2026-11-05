import re
import sqlite3
from pathlib import Path


class Store:
    def __init__(self, root, authority):
        self._root = Path(root)
        self._authority = authority

    def _open(self, scope):
        self._authority.check(scope)
        if not re.fullmatch(r"[a-zA-Z0-9_-]{1,48}", scope.family):
            raise PermissionError("家庭编号无效")
        directory = self._root / "families" / scope.family
        directory.mkdir(parents=True, exist_ok=True)
        db = sqlite3.connect(directory / "family.db")
        db.execute("CREATE TABLE IF NOT EXISTS demo(member TEXT, key TEXT, value TEXT, PRIMARY KEY(member,key))")
        return db

    def put(self, scope, key, value):
        db = self._open(scope)
        try:
            with db:
                db.execute("INSERT OR REPLACE INTO demo VALUES(?,?,?)", (scope.member, key, value))
        finally:
            db.close()

    def get(self, scope, key):
        db = self._open(scope)
        try:
            row = db.execute("SELECT value FROM demo WHERE member=? AND key=?", (scope.member, key)).fetchone()
            return row[0] if row else None
        finally:
            db.close()
