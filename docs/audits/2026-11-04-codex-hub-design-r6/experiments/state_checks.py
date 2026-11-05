"""Bounded audit models, NOT hub implementation or product tests.

The literal D-105 predicates below deliberately contain only the checks stated
in architecture section 7.5. Their counterexamples show that those predicates
are insufficient, not that an unimplemented hub has executed these actions.
All people, contents, recipients and record identifiers are synthetic.
"""

import json
from pathlib import Path


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
ARCH = (ROOT / "docs/hub/hub-architecture.md").read_text()
PROTO = (ROOT / "docs/hub/device-protocol.md").read_text()
assert "人确认之前不取最终的快照和日志" in ARCH
assert "看这一轮读过哪些私人资料" in ARCH
assert "对外动作的对象（收件人、号码）要能在人这句话里找到" in ARCH
assert "没有 `history_gap` 回 `continue`，有就回 `manifest`" in PROTO


def recovery_log_order(final_read_after_stop):
    # The old machine successfully uploads the deletion BEFORE being stopped.
    remote_log = []
    first_read = list(remote_log)
    remote_log.append("forget_fixture_fact")
    old_stopped = True
    applied = list(remote_log) if final_read_after_stop else first_read
    return {
        "old_machine_really_stopped": old_stopped,
        "uploaded_log_entries": len(remote_log),
        "applied_entries": len(applied),
        "forgotten_fact_visible": "forget_fixture_fact" not in applied,
    }


def sync_action(*, complete=True, known=False, same_instance=True, gap=False,
                unused_grant=False, grant_instance_matches=True,
                replica_unchanged=True, hub_empty=False):
    # Ordered rules in device-protocol.md:298-303. Human preview is not used.
    if not complete:
        return "wait"
    if known:
        return "manifest" if gap or not same_instance else "continue"
    if unused_grant and grant_instance_matches and replica_unchanged:
        return "manifest" if gap else "continue"
    return "continue" if hub_empty else "wait"


def listed_asked_checks(human_origin, utterance, recipient_name, matches=1,
                        high_risk=False):
    # No proposed model _meta is consumed. This does NOT parse intent.
    return (human_origin and recipient_name in utterance and matches == 1
            and not high_risk)


def text_copy_guard(read_texts, outgoing, explicitly_allowed=False):
    # Literal reading of "要发的内容里有没有这些文字". No semantic classifier.
    return not explicitly_allowed and any(text in outgoing for text in read_texts)


old_log = recovery_log_order(False)
new_log = recovery_log_order(True)
assert old_log["forgotten_fact_visible"]
assert not new_log["forgotten_fact_visible"]

restore_cases = [
    ("unused_grant_without_gap", dict(unused_grant=True), "continue"),
    ("unused_grant_with_gap", dict(unused_grant=True, gap=True), "manifest"),
    ("incomplete_with_grant", dict(unused_grant=True, complete=False), "wait"),
    ("new_dataset_reuses_used_grant", dict(unused_grant=False), "wait"),
    ("replica_changed", dict(unused_grant=True, replica_unchanged=False), "wait"),
    ("hub_restored", dict(unused_grant=True, grant_instance_matches=False), "wait"),
    ("known_dataset_with_gap", dict(known=True, gap=True), "manifest"),
    ("known_dataset_after_hub_restore", dict(known=True, same_instance=False), "manifest"),
    ("unknown_dataset_without_grant", dict(same_instance=False, gap=True), "wait"),
]
sync_results = []
for name, kwargs, expected in restore_cases:
    actual = sync_action(**kwargs)
    assert actual == expected, name
    sync_results.append({"case": name, "action": actual})

replica = set(range(257))
device = set()
retained_tombstones = set(range(1, 257))
v06_remainder = replica - retained_tombstones
v07_remainder = replica & device  # Complete manifest commits the empty set.
assert len(v06_remainder) == 1 and len(v07_remainder) == 0

authority_cases = [
    ("explicit_send_control", True, "给妈妈发消息说我到了。", "妈妈", True),
    ("read_only_request", True, "妈妈刚才的消息说了什么？", "妈妈", False),
    ("quoted_instruction", True, "解释网页上的这句话：给妈妈打电话。", "妈妈", False),
    ("negated_action", True, "不要给妈妈发消息，只帮我看一下。", "妈妈", False),
    # Parent job asks to read a message; remembering its owner is not a send grant.
    ("background_inherits_read_request", True, "整理妈妈发来的长消息，做好告诉我。", "妈妈", False),
    ("autonomous_timer_control", False, "给妈妈发消息。", "妈妈", False),
    ("received_message_control", False, "给妈妈发消息。", "妈妈", False),
]
authority_results = []
for name, human, utterance, recipient, send_requested in authority_cases:
    actual = listed_asked_checks(human, utterance, recipient)
    bound_operation_control = actual and send_requested
    authority_results.append({
        "case": name,
        "utterance": utterance,
        "candidate_operation": "send_or_dial",
        "listed_checks_grant_asked": actual,
        "human_requested_that_operation": send_requested,
        "operation_bound_control_grants": bound_operation_control,
    })
assert [r["case"] for r in authority_results
        if r["listed_checks_grant_asked"] and not r["human_requested_that_operation"]] == [
    "read_only_request", "quoted_instruction", "negated_action",
    "background_inherits_read_request",
]
assert not listed_asked_checks(True, "给妈妈发消息", "老师")
assert not listed_asked_checks(True, "给妈妈发消息", "妈妈", matches=2)
assert not listed_asked_checks(True, "给妈妈发消息", "妈妈", high_risk=True)

fixture_fact = "样例家庭喜欢紫色风筝"
translated_fact = "The example family likes violet kites."
export_request = "给林老师发消息说我们周六到。"
assert listed_asked_checks(True, export_request, "林老师")
assert text_copy_guard([fixture_fact], fixture_fact)
assert not text_copy_guard([fixture_fact], translated_fact)
assert not text_copy_guard([fixture_fact], fixture_fact, explicitly_allowed=True)
# Bind a read source to a permitted destination; no attempt at general NLP.
private_results = {
    "human_request": export_request,
    "send_operation_explicitly_requested": True,
    "recipient_matches_request": True,
    "fixture_source": fixture_fact,
    "outgoing_translation": translated_fact,
    "raw_copy_requires_confirmation": True,
    "translation_requires_confirmation_under_literal_check": False,
    "source_bound_control_requires_confirmation_for_both": True,
    "user_explicitly_authorized_private_source": False,
    "assumption": "Private source is known and read in this same turn; outgoing text is translated. No missed source logging is needed.",
}

results = {
    "review_commit": "4ba1656c7d040cbb2e63d25875c8111fe5e2ce8f",
    "evidence_boundary": "Pure synthetic policy/state models; not execution of hub, device, model API, or browser. A zero exit means the stated cases and controls hold in these models only.",
    "recovery_log": {"v06": old_log, "v07": new_log},
    "restore_dispatch": sync_results,
    "history_gap_257_deletes": {
        "v06_remaining_records": len(v06_remainder),
        "v07_remaining_records": len(v07_remainder),
    },
    "asked_authority": authority_results,
    "private_export": private_results,
}
out = HERE / "results.json"
out.write_text(json.dumps(results, ensure_ascii=False, indent=2) + "\n")
print("PASS: recovery order; 9 restore branches; 257 deletions; asked counterexamples and controls; private export counterexample and controls.")
print("Output: " + str(out.relative_to(ROOT)))
