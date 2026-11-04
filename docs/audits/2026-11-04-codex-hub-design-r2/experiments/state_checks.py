#!/usr/bin/env python3
"""Small counterexamples to specified rules, NOT a hub implementation/test.

All records are synthetic. No network or files are used by these state models.
Assertions mean the documented rule admits the counterexample, not that a real
service has experienced it. Corrective controls are included where practical.
"""
from collections import deque
from copy import deepcopy
import json

results = []

# P-1: complete was durably received, first hello is delayed beyond 10 minutes.
# Protocol 3.2 allows pairing only without a token; 4 auth_invalid retries it.
device_has_credential = True  # Boolean only: no credential values are produced.
server_pending_expired = True
can_begin = not device_has_credential
can_hello = device_has_credential and not server_pending_expired
assert not can_begin and not can_hello
results.append({"case": "pending_expired_after_device_commit", "pair_begin_allowed": can_begin,
                "hello_succeeds": can_hello, "result": "no automatic recovery transition specified"})

# R-1: an irreversible record after the daily backup is lost with the host.
backup = {"revoked": False, "memory_visible": True, "irreversible_log": []}
live = deepcopy(backup)
live.update(revoked=True, memory_visible=False, irreversible_log=["revoke", "forget"])
recovered = deepcopy(backup)  # original host and its non-rollback file are lost
assert not recovered["revoked"] and recovered["memory_visible"]
results.append({"case": "daily_backup_loses_post_backup_security_log",
                "revocation_restored": recovered["revoked"],
                "forgotten_fact_visible": recovered["memory_visible"]})

# R-2: restoring the SAME backup twice, then incrementing its stored epoch.
base = {"epoch": 0, "acked_ver": 1, "records": {1: "synthetic memo"}}
first = deepcopy(base)
first["epoch"] += 1
device = {"epoch": first["epoch"], "last_ver": 2, "history_gap": False,
          "records": {}, "tombstones": {}}
# After first restore, device deleted record 1 and hub acked ver 2; tombstone gone.
first.update(acked_ver=2, records={})
second = deepcopy(base)
second["epoch"] += 1
needs_manifest = second["epoch"] != device["epoch"] or device["history_gap"]
assert not needs_manifest and 1 in second["records"] and not device["records"]
results.append({"case": "same_backup_restored_twice", "first_epoch": first["epoch"],
                "second_epoch": second["epoch"], "manifest_triggered": needs_manifest,
                "deleted_object_present_on_hub": 1 in second["records"]})
assert "restore-session-one" != "restore-session-two"  # a fresh, non-rollback ID distinguishes restores

# R-3: manifest started at ver 10, but no fixed-cutover cursor was specified.
# A legal-looking implementation ACKs the last_ver sampled when the upload ends.
manifest = {1: {"ver": 10, "body": "old"}}
device_records = {1: {"ver": 11, "body": "new"}}
hub_records = deepcopy(manifest)
unsafe_acked = 11
pending = [r for r in device_records.values() if r["ver"] > unsafe_acked]
assert not pending and hub_records != device_records
safe_acked = 10
assert [r for r in device_records.values() if r["ver"] > safe_acked]
results.append({"case": "manifest_without_fixed_watermark", "manifest_ver": 10,
                "acked_ver": unsafe_acked, "new_edit_resent": bool(pending),
                "boundary": "one admitted interpretation; production algorithm does not yet exist"})

# C-1: confirm.list reports only pending. Device-side action already ran.
pending_confirmations = {17: "synthetic external action"}
events = deque(maxlen=8)
pending_confirmations.pop(17)
external_action_executed = True
events.append({"kind": "confirm.result", "token": 17, "decision": "accepted"})
for n in range(8):
    events.append({"kind": "unrelated", "seq": n})
result_seen = any(e["kind"] == "confirm.result" for e in events)
in_list = 17 in pending_confirmations
document_fallback_says_not_executed = not result_seen and not in_list
assert external_action_executed and document_fallback_says_not_executed
results.append({"case": "missing_confirmation_is_not_negative_execution_proof",
                "actually_executed": external_action_executed,
                "fallback_not_executed": document_fallback_says_not_executed})

# M-1: A -> child -> A, same WSS. _meta.member alone cannot reject old work.
sent = {"member": "adult-A", "epoch": 17}
current = {"member": "adult-A", "epoch": 19}
wire_meta = {"member": sent["member"]}
assert wire_meta["member"] == current["member"] and sent["epoch"] != current["epoch"]
results.append({"case": "member_name_cannot_prove_member_epoch", "member_check_passed": True,
                "epoch_check_would_pass": False})

# V-1: device timer includes cold connection; the server message has no deadline.
device_deadline_s, server_receive_s, server_budget_s = 45, 8, 42
server_finish_s = server_receive_s + server_budget_s
assert server_finish_s > device_deadline_s
results.append({"case": "server_42_seconds_after_cold_connection", "device_deadline_s": 45,
                "server_finish_s": server_finish_s, "late_by_s": server_finish_s - 45})

# V-2: buffer capacity is not the free space. Device accepts 512 x 60ms frames.
# At the burst point, 200 frames are queued and a 400-frame tail remains.
capacity, already_queued, tail = 512, 200, 400
dropped = max(0, already_queued + tail - capacity)
assert tail * 0.060 < 30.72 and dropped == 88
results.append({"case": "tail_under_30s_still_exceeds_remaining_buffer", "queued_frames": already_queued,
                "tail_frames": tail, "overflow_frames": dropped,
                "boundary": "synthetic queue occupancy, not measured network/audio performance"})

print(json.dumps(results, ensure_ascii=False, indent=2))
