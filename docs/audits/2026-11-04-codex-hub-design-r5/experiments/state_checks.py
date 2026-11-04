"""Bounded design-state checks, not a hub implementation or integration test.

No network, credentials, product imports, or disk databases. SQLite backups stay
in memory. Output contains only synthetic record numbers and state summaries.
Run from the repository root; stdout is the only output written by this script.
"""

import json
import sqlite3
from copy import deepcopy
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]


def contract_anchors():
    arch = (ROOT / "docs/hub/hub-architecture.md").read_text()
    proto = (ROOT / "docs/hub/device-protocol.md").read_text()
    checks = {
        "stop_before_open": "新机器开放之前，旧机器必须已经停了。" in arch,
        "read_log_before_stop_page": arch.index('D --> E["取回安全日志，验证')
        < arch.index('E --> I["成人核对一页'),
        "grant_fast_path": "不认识，带着没用过的 `restore_token`，而且中枢副本从发令牌之后没变过" in proto,
        "grant_ack_baseline": "`acked_ver` 等于快照的版本" in proto,
        "device_stores_grant": "记下 `restore_token`" in proto,
        "ack_allows_tombstone_prune": "ink_db_ack，更早的墓碑可以扔" in arch,
        "bounded_tombstones": "每个集合最多留 256 条墓碑" in arch,
        "blob_metadata_required": "内容和包含它目录记录的完整快照都进了对象存储才回真" in proto,
        "rotation_disabled": "首版不发 `next_token`" in proto,
    }
    assert all(checks.values()), checks
    return checks


def status(*, known, same_instance, complete=True, history_gap=False,
           has_grant=False, unused=False, same_replica=False, empty=False):
    """Protocol 3.9 rows, with no new user decision in this request.

    All requests are authenticated and belong to the same synthetic device,
    collection, and member. 'same_replica' is the version comparison from the
    specified grant binding, not an independent proof of non-rollback.
    """
    if not complete:
        return "wait"
    if known:
        return "manifest" if not same_instance or history_gap else "continue"
    if has_grant and unused and same_replica:
        return "continue"
    if empty:
        return "continue"
    return "wait"


def fresh_log_after_stop():
    # The new server reads/replays before displaying the stop-old-server page.
    remote_log = []
    new_server = {"records": {1}, "applied": 0, "open": False}
    fetched_log = list(remote_log)

    def replay(target, log):
        for operation, record in log:
            if operation == "forget":
                target["records"].discard(record)
        target["applied"] = len(log)

    replay(new_server, fetched_log)
    # An already-started job finishes uploading, or another adult acts, while
    # the page is open. This operation is fully in object storage before stop.
    remote_log.append(("forget", 1))
    old_server_stopped = True
    assert old_server_stopped  # The adult followed the new rule correctly.
    new_server["open"] = True  # Documented next step: publish instance, open.
    assert 1 in new_server["records"] and new_server["applied"] < len(remote_log)

    corrected = deepcopy(new_server)
    corrected["open"] = False
    replay(corrected, list(remote_log))  # Re-fetch/replay after confirmed stop.
    corrected["open"] = True
    assert 1 not in corrected["records"]
    return {
        "old_server_stopped_before_new_open": old_server_stopped,
        "remote_log_committed": len(remote_log),
        "new_server_log_applied": new_server["applied"],
        "forgotten_record_visible": 1 in new_server["records"],
        "control_refetch_after_stop_visible": 1 in corrected["records"],
    }


def memory_db():
    db = sqlite3.connect(":memory:")
    db.executescript("""
        CREATE TABLE records (id INTEGER PRIMARY KEY);
        CREATE TABLE state (dataset INTEGER, replica_ver INTEGER);
        CREATE TABLE restore_grant (base_ver INTEGER, used INTEGER);
        INSERT INTO records VALUES (1), (2);
        INSERT INTO state VALUES (1, 2);
        INSERT INTO restore_grant VALUES (2, 0);
    """)
    return db


def grant_rewinds_with_database():
    # Assumption under test: grant state is in the regularly snapshotted DB;
    # the device retains the stored grant reference. The documents define no
    # non-rollback grant ledger, per-instance invalidation, or mandatory erasure.
    live = memory_db()
    saved = sqlite3.connect(":memory:")
    live.backup(saved)  # Grant issued, not yet consumed.
    device_dataset = 2
    device_records = {1, 2}
    device_has_grant = True
    with live:
        live.execute("UPDATE restore_grant SET used = 1")
        live.execute("UPDATE state SET dataset = 2")
    # Delete, successfully sync, then prune the acknowledged tombstone.
    device_records.remove(2)
    with live:
        live.execute("DELETE FROM records WHERE id = 2")
        live.execute("UPDATE state SET replica_ver = 3")
    device_tombstones = {}  # acked_ver = 3 permits pruning.

    restored = sqlite3.connect(":memory:")
    saved.backup(restored)
    known_dataset, restored_ver = restored.execute("SELECT * FROM state").fetchone()
    base_ver, used = restored.execute("SELECT * FROM restore_grant").fetchone()
    action = status(
        known=known_dataset == device_dataset, same_instance=False,
        has_grant=device_has_grant, unused=not used,
        same_replica=restored_ver == base_ver,
    )
    assert action == "continue" and not device_tombstones
    hub_records = {r[0] for r in restored.execute("SELECT id FROM records")}
    assert hub_records - device_records == {2}
    # A grant issued by the previous hub_instance cannot authorize the shortcut.
    fenced_action = status(known=False, same_instance=False, has_grant=False)
    assert fenced_action == "wait"
    # A new, explicit choice of the device copy followed by reconciliation fixes
    # the missing deletion. The control does not silently authorize this choice.
    reconciled = hub_records & device_records
    assert reconciled == device_records
    result = {
        "grant_used_before_rollback": True,
        "grant_used_in_restored_snapshot": bool(used),
        "hub_instance_changed": True,
        "action": action,
        "returned_acked_ver": base_ver,
        "device_previous_acked_ver": 3,
        "remaining_delete_tombstones": len(device_tombstones),
        "deleted_records_still_in_hub": sorted(hub_records - device_records),
        "control_invalidate_prior_instance_action": fenced_action,
        "control_after_new_preview_and_manifest_records": sorted(reconciled),
    }
    live.close()
    saved.close()
    restored.close()
    return result


def grant_with_history_gap():
    # No hub rollback and no grant reuse: the first report is delayed by a
    # disconnect after atomic installation. The dataset remains complete.
    hub_records = set(range(1, 258))
    device_records = set(hub_records)
    snapshot_ver = 257
    tombstones = {}
    current_ver = snapshot_ver
    for record in sorted(device_records):
        device_records.remove(record)
        current_ver += 1
        tombstones[record] = current_ver
        if len(tombstones) > 256:
            del tombstones[next(iter(tombstones))]
    history_gap = True
    action = status(known=False, same_instance=True, history_gap=history_gap,
                    has_grant=True, unused=True, same_replica=True)
    assert action == "continue"
    # from_ver equals the returned baseline. The remaining 256 tombstones are
    # valid incremental records, but cannot communicate the evicted deletion.
    from_ver = snapshot_ver
    hub_acked_ver = snapshot_ver
    assert from_ver == hub_acked_ver
    for record in tombstones:
        hub_records.discard(record)
    assert hub_records == {1} and not device_records
    # Same known dataset would take the manifest row even in the original table.
    next_status_action = status(known=True, same_instance=True, history_gap=True)
    assert next_status_action == "manifest"
    manifest_result = hub_records & device_records
    assert manifest_result == set()
    return {
        "hub_rollback": False,
        "grant_previously_used": False,
        "device_complete": True,
        "reported_history_gap": history_gap,
        "action_on_first_report": action,
        "increment_from_ver_matches_ack": from_ver == hub_acked_ver,
        "retained_tombstones": len(tombstones),
        "deleted_records_remaining_after_delta": sorted(hub_records),
        "later_status_if_gap_retained": next_status_action,
        "control_manifest_remaining_records": sorted(manifest_result),
    }


def closed_path_controls():
    outcomes = {
        "ordinary_unknown_old_instance": status(known=False, same_instance=False),
        "reused_grant_new_dataset": status(known=False, same_instance=True,
                                          has_grant=True, unused=False, same_replica=True),
        "replica_changed_since_issue": status(known=False, same_instance=True,
                                             has_grant=True, unused=True, same_replica=False),
        "incomplete_with_grant": status(known=False, same_instance=True, complete=False,
                                       has_grant=True, unused=True, same_replica=True),
        "first_install_unchanged": status(known=False, same_instance=True,
                                         has_grant=True, unused=True, same_replica=True),
        "same_dataset_retry": status(known=True, same_instance=True),
        "known_dataset_after_hub_restore": status(known=True, same_instance=False),
    }
    assert list(outcomes.values()) == ["wait", "wait", "wait", "wait",
                                       "continue", "continue", "manifest"]
    return outcomes


if __name__ == "__main__":
    print(json.dumps({
        "scope": "synthetic design transitions; no product execution",
        "contract_anchors": contract_anchors(),
        "read_log_then_stop": fresh_log_after_stop(),
        "grant_state_rollback": grant_rewinds_with_database(),
        "grant_history_gap": grant_with_history_gap(),
        "closed_path_controls": closed_path_controls(),
    }, ensure_ascii=False, indent=2))
