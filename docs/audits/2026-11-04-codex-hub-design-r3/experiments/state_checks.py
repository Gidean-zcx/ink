#!/usr/bin/env python3
"""Offline checks of v0.4 rules, NOT a hub implementation or integration test.

Uses synthetic labels only. No network, credentials, hardware, or file writes.
"""

import hashlib
import json


def append(chain, operation):
    previous = chain[-1]["hash"] if chain else "genesis"
    record = {"seq": len(chain) + 1, "previous": previous, "operation": operation}
    record["hash"] = hashlib.sha256(json.dumps(record, sort_keys=True).encode()).hexdigest()
    return chain + [record]


def valid_chain(chain):
    rebuilt = []
    for record in chain:
        rebuilt = append(rebuilt, record["operation"])
        if rebuilt[-1] != record:
            return False
    return True


def check_security_log_tail():
    # An encrypted/authenticated prefix remains valid: the absent suffix has
    # never reached the remote store. Encryption would not prove its presence.
    remote = append([], "earlier security operation")
    local = append(remote, "revoke example device")
    local = append(local, "forget example fact")
    assert valid_chain(local) and valid_chain(remote)
    restored_operations = [r["operation"] for r in remote]
    assert "revoke example device" not in restored_operations
    assert "forget example fact" not in restored_operations
    print("security_log: local=3 remote=1, valid remote prefix; missing tail is not detected")


def sync_action(*, known, complete, device_hub, current_hub, from_snapshot=False,
                history_gap=False, hub_has_data=True):
    # Give complete=false the strongest reading (a global guard), even though
    # it appears last in the document's table. The counterexample does not
    # depend on first-match ordering, collisions, or corrupt data.
    if not complete:
        return "restore"
    if known:
        return "manifest" if device_hub != current_hub or history_gap else "continue"
    if from_snapshot:
        return "manifest"
    if device_hub and device_hub != current_hub:
        return "manifest"
    if not device_hub and not hub_has_data:
        return "continue"
    return "restore"


def check_unknown_dataset():
    # I0 device backup contains A. Both sides later contain A+B; the hub is
    # exported/imported as I1 retaining A+B. Device independently restores
    # the older local backup: its fresh dataset is unknown, complete=true,
    # last successful hub remains I0, and no hub-issued snapshot is claimed.
    restored_device = {"A"}
    hub = {"A", "B"}
    args = dict(known=False, complete=True, device_hub="I0", current_hub="I1")
    decision = sync_action(**args)
    assert decision == "manifest"
    archived = hub - restored_device
    assert archived == {"B"}
    # The very same old-device restore, without hub import, requires preview.
    assert sync_action(**{**args, "current_hub": "I0"}) == "restore"
    assert sync_action(**{**args, "complete": False}) == "restore"
    print("sync_status: old device backup -> manifest after hub import; B archived without preview")


def check_fixed_rules():
    def after_auth_error(state, error):
        if state == "COMMITTING" and error in ("pending_token_expired", "auth_invalid"):
            return "UNPAIRED"
        return state

    assert after_auth_error("COMMITTING", "pending_token_expired") == "UNPAIRED"
    assert after_auth_error("COMMITTING", "auth_invalid") == "UNPAIRED"
    assert after_auth_error("BOUND", "auth_invalid") == "BOUND"
    # Fixed manifest ACK: committing V must leave later edits eligible.
    v, edits = 20, {21, 22}
    assert {revision for revision in edits if revision > v} == edits
    print("fixed_rules: expired pending pairing returns to UNPAIRED; ACK(V) retains edits after V")


def check_latency():
    adult_text = 30 + 200 + 300 + 150 + 300
    child_text = adult_text + 450
    assert adult_text == 980 and child_text == 1430
    assert 3000 - child_text == 1570
    assert child_text + 1600 == 3030
    # Conditional comparison only: model cross-border increment assumed 150,
    # all mainland-network overhead assumed 100 as in the design's prose.
    adult_audio_network = 200 + 150 + 150 + 150
    child_audio_network = adult_audio_network + 150
    assert child_audio_network - 100 == 700
    print("latency: first-content-token ceilings adult=2020 ms child=1570 ms (warm, no VAD wait)")
    print("latency: conditional extra network adult audio=550 ms child audio=700 ms; not measured")


if __name__ == "__main__":
    check_security_log_tail()
    check_unknown_dataset()
    check_fixed_rules()
    check_latency()
