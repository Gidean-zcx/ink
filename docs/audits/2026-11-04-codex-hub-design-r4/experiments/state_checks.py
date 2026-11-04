#!/usr/bin/env python3
"""Executable counterexamples for the v0.5 design, not product tests.

Only synthetic in-memory data; no network, disk database, credentials, or
product imports. SQLite's backup() supplies the actual old/new metadata
boundary. The other checks are finite state traces of the documented rules.
Exit 0 means all counterexamples and controls behaved as stated, not that the
proposed product passed a recovery test.
"""

import json
import sqlite3


def check_owner_check_then_write():
    """Architecture 396: separate ownership read and ordinary object write."""
    store = {"owner": "old", "writes": []}
    old_authorized = store["owner"] == "old"
    # The old process can pause after its check, even with strong consistency.
    store["owner"] = "new"
    new_authorized = store["owner"] == "new"
    if new_authorized:
        store["writes"].append({"writer": "new", "records": ["A", "B"]})
    if old_authorized:
        store["writes"].append({"writer": "old", "records": ["A"]})
    assert store["owner"] == "new"
    assert [w["writer"] for w in store["writes"]] == ["new", "old"]

    # Control: an atomic fence at publication, not another earlier GET.
    fenced = {"owner": "new", "published": {"writer": "new"}}
    if fenced["owner"] == "old":
        fenced["published"] = {"writer": "old"}
    assert fenced["published"]["writer"] == "new"
    return {
        "id": "owner_check_then_write",
        "result": "counterexample reproduced",
        "trace": ["old checks owner", "new takes over", "new writes", "old writes"],
        "old_write_after_takeover": True,
        "atomic_publication_fence_rejects_old": True,
        "boundary": "both writers are admitted; no shared object key or restic snapshot layout assumed",
    }


def check_partitioned_old_server():
    """Architecture 396/412: serving continues without an ownership lease."""
    old = {"running": True, "revoked": False, "object_store_reachable": False}
    new = {"running": True, "revoked": True, "security_log_committed": True}
    # Old cannot perform the upload-time ownership check and runs as specified
    # by the object-store outage rule. It can still reach an old device session.
    old_accepts_device = old["running"] and not old["revoked"]
    assert old_accepts_device and new["security_log_committed"]
    return {
        "id": "partitioned_old_server",
        "result": "counterexample reproduced",
        "old_still_serves_device_revoked_on_new": old_accepts_device,
        "assumption": "old server retains device connectivity but loses object-store connectivity",
    }


def check_blob_metadata_boundary():
    """Protocol 272: bytes-only durable ACK can precede recoverable metadata."""
    live = sqlite3.connect(":memory:")
    remote_snapshot = sqlite3.connect(":memory:")
    try:
        live.execute("CREATE TABLE objects (id TEXT, member TEXT, name TEXT, durable INTEGER)")
        live.commit()
        live.backup(remote_snapshot)  # Last completed database snapshot, t=0.
        object_store = {}
        live.execute("INSERT INTO objects VALUES ('file-A', 'member-A', 'sample', 0)")
        live.commit()
        object_store["opaque-object-A"] = b"synthetic encrypted payload"
        live.execute("UPDATE objects SET durable=1 WHERE id='file-A'")
        live.commit()
        status_durable = "opaque-object-A" in object_store
        device_original_exists = not status_durable
        # Host dies before the next database snapshot. Restore enumerates
        # unconfirmed files in the old database, as described in section 4.5.
        recoverable_rows = remote_snapshot.execute("SELECT * FROM objects").fetchall()
        pending_rows = remote_snapshot.execute("SELECT * FROM objects WHERE durable=0").fetchall()
        assert status_durable and not device_original_exists
        assert recoverable_rows == [] and pending_rows == []
        bytes_survive_initial_restore = bool(object_store)
        # With no catalogue entry or retained snapshot reference, the documented
        # unreferenced-object GC can delete this blob after the retention period.
        referenced_objects = set()
        after_retention = {k: v for k, v in object_store.items() if k in referenced_objects}
        assert after_retention == {}

        # Control: delaying durable ACK until metadata is in a completed remote
        # snapshot restores the association without introducing a new service.
        live.backup(remote_snapshot)
        assert remote_snapshot.execute("SELECT count(*) FROM objects").fetchone()[0] == 1
        return {
            "id": "blob_metadata_boundary",
            "result": "counterexample reproduced",
            "durable_reported": status_durable,
            "device_original_exists": device_original_exists,
            "bytes_survive_initial_restore": bytes_survive_initial_restore,
            "restored_catalogue_rows": len(recoverable_rows),
            "restored_pending_upload_rows": len(pending_rows),
            "unreferenced_gc_after_retention_removes_bytes": not after_retention,
            "metadata_snapshot_before_ack_control_passed": True,
            "assumption": "no extra durable catalogue/journal beyond the design's database snapshots",
        }
    finally:
        live.close()
        remote_snapshot.close()


def sync_action(*, complete, known, same_instance, gap, latest_snapshot, hub_has_data):
    """Transcription of protocol lines 293-300, in the specified order."""
    if not complete:
        return "wait"
    if known and same_instance and not gap:
        return "continue"
    if known:
        return "manifest"
    if latest_snapshot:
        return "manifest"
    if not hub_has_data:
        return "continue"
    return "wait"


def check_reused_latest_snapshot():
    snapshot = {"A"}  # Latest issued snapshot S; installed as dataset D1.
    hub_current = snapshot | {"B"}  # D1 subsequently synchronizes a new record.
    # Restore S again as a new unknown dataset D2. No newer snapshot was issued.
    args = dict(complete=True, known=False, same_instance=False, gap=False,
                latest_snapshot=True, hub_has_data=True)
    action = sync_action(**args)
    moved_to_history = hub_current - snapshot if action == "manifest" else set()
    assert action == "manifest" and moved_to_history == {"B"}
    # Controls: the original old-instance-only counterexample is now closed;
    # incomplete data cannot pass the new first row.
    assert sync_action(**{**args, "latest_snapshot": False}) == "wait"
    assert sync_action(**{**args, "complete": False}) == "wait"
    return {
        "id": "reused_latest_snapshot",
        "result": "counterexample reproduced",
        "selected_action": action,
        "newer_records_moved_to_history_without_preview": sorted(moved_to_history),
        "original_old_instance_only_case_now_waits": True,
        "complete_false_control_waits": True,
        "assumption": "latest-issued snapshot identifier remains valid after its first installation; no consumption rule is specified",
    }


def check_rotation_before_snapshot():
    """Credential versions are integers, never actual credentials or hashes."""
    backed_up_version = 0
    device_versions = {0, 1}
    live_accepted_versions = {1}
    assert device_versions & live_accepted_versions  # New hello succeeds.
    device_versions.discard(0)  # Old retained only until this hello, section 6.3.
    restored_accepted_versions = {backed_up_version}
    can_authenticate = bool(device_versions & restored_accepted_versions)
    assert not can_authenticate
    retained_until_durable = {0, 1}
    assert retained_until_durable & restored_accepted_versions
    return {
        "id": "rotation_before_snapshot",
        "result": "counterexample reproduced",
        "new_hello_success_before_crash": True,
        "can_authenticate_after_restore": can_authenticate,
        "retain_until_remote_durable_control_passed": True,
        "boundary": "rotation is optional in the first release; counterexample applies when enabled",
    }


def check_security_log_and_interval_controls():
    """Positive scope for HUB3-001, and the explicitly limited RPO claim."""
    records = [{"remote": True}, {"remote": False}]
    completed = [r for r in records if r["remote"]]
    restored = [r for r in records if r["remote"]]
    manual_open_confirmed = False
    assert completed == restored
    assert not manual_open_confirmed
    # Snapshot starts every 300 s, upload takes 60 s; host dies just before
    # the second upload completes. Its last committed snapshot covers t=0.
    last_committed_point = 0
    failure_time = 359
    actual_window = failure_time - last_committed_point
    assert actual_window > 300
    return {
        "id": "security_log_and_interval_controls",
        "result": "controls passed",
        "completed_security_operations_survive_single_writer_restore": True,
        "manual_gate_blocks_automatic_reopen_with_old_prefix": True,
        "snapshot_interval_seconds": 300,
        "example_committed_snapshot_age_seconds": actual_window,
        "boundary": "synthetic schedule, not a measured backup time; no separate high finding",
    }


def main():
    checks = [check_owner_check_then_write, check_partitioned_old_server,
              check_blob_metadata_boundary, check_reused_latest_snapshot,
              check_rotation_before_snapshot, check_security_log_and_interval_controls]
    results = [check() for check in checks]
    print(json.dumps({"design_commit": "2059f32b081b3e15b47dfdbc8ba944f85071cc3c",
                      "scope": "synthetic design-state checks, not product integration tests",
                      "checks": results}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
