#!/usr/bin/env python3
"""Counterexamples to the proposed algorithm, NOT a running hub reproduction.

SQLite is real; family tables and sync states are minimal synthetic models.
"""
import sqlite3
import tempfile
from pathlib import Path

with tempfile.TemporaryDirectory(prefix='hub-audit-recovery-') as tmp:
    p = Path(tmp)
    hub = sqlite3.connect(p / 'hub.db', isolation_level=None)
    family = sqlite3.connect(p / 'family.db', isolation_level=None)
    hub.execute('CREATE TABLE members(id TEXT PRIMARY KEY)')
    family.execute('CREATE TABLE notes(member TEXT, body TEXT)')
    hub.execute('VACUUM INTO ?', (str(p / 'hub.snapshot'),))
    # A legitimate request completes between the two snapshots.
    hub.execute("INSERT INTO members VALUES ('synthetic-member')")
    family.execute("INSERT INTO notes VALUES ('synthetic-member','synthetic-note')")
    family.execute('VACUUM INTO ?', (str(p / 'family.snapshot'),))
    with sqlite3.connect(p / 'hub.snapshot') as h, sqlite3.connect(p / 'family.snapshot') as f:
        members = {r[0] for r in h.execute('SELECT id FROM members')}
        orphaned = [r[0] for r in f.execute('SELECT member FROM notes') if r[0] not in members]
        assert orphaned == ['synthetic-member']
        print('independent_VACUUM_INTO: each_integrity_check=ok; orphaned_member_references=1')
        assert h.execute('PRAGMA integrity_check').fetchone()[0] == 'ok'
        assert f.execute('PRAGMA integrity_check').fetchone()[0] == 'ok'
    hub.close()
    family.close()

# Delete version 2 was received durably, ACKed, then purged on the device.
server_backup = {'object': {'ver': 1, 'body': 'synthetic-note'}}
device_live, device_tombstones = {}, {}
device_version = 2
restored_server = dict(server_backup)
changes_since_backup = list(device_live.values()) + list(device_tombstones.values())
assert not changes_since_backup and 'object' in restored_server and device_version == 2
print('restore_after_delete_ACK: server_has_deleted_object=True; device_changes_empty=True')

# Restore the device to generation A v10; version-only dedupe still holds A v11.
seen = {('notes', 'member', 'object', 11): 'old-body'}
restored_device_next = ('notes', 'member', 'object', 11)
assert restored_device_next in seen
print('device_restore_version_reuse: new_body_dropped_by_documented_dedupe_key=True')
