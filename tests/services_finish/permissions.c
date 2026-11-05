#include "product.h"

static unsigned token(void) {
    unsigned n = 0;
    sscanf(g_rc.object, "confirm:%u", &n);
    return n;
}

static void review_run(void *arg) {
    (void)arg; delay_ms(1800);
    CHECK(ink_clip_set("SYNTHETIC_PRIVATE_CLIPBOARD", false, INK_SRC_TOUCH) == 0, "human clipboard");
    CHECK(AI("clipboard.read", NULL) && g_rc.status == INK_RC_REJECTED, "permission initially denies AI");
    CHECK(AI("settings.set", "{\"id\":\"ai.read_clipboard\",\"value\":true}") &&
        g_rc.status == INK_RC_PENDING_CONFIRMATION, "permission change must require a person, actual=%s", ink_rc_name(g_rc.status));
    unsigned permission = token();
    CHECK(ink_setting_get("ai.read_clipboard") == 0, "waiting permission has no effect");
    CHECK(ink_confirm_decide(permission, true, INK_SRC_AI) != 0, "AI cannot accept its own request");
    CHECK(AI("clipboard.read", NULL) && !strstr(g_result, "SYNTHETIC_PRIVATE_CLIPBOARD"),
        "AI enabled its own permission and read the clipboard: %s", g_result);
    CHECK(AI("settings.set", "{\"id\":\"audio.child_max\",\"value\":100}") &&
        g_rc.status == INK_RC_PENDING_CONFIRMATION, "child rule change must ask, actual=%s", ink_rc_name(g_rc.status));
    unsigned child_rule = token();
    CHECK(ink_setting_get("audio.child_max") == 60, "waiting child rule has no effect");
    CHECK(ink_confirm_decide(permission, true, INK_SRC_TOUCH) == 0 &&
          WAIT_FOR(ink_setting_get("ai.read_clipboard") == 1, 1000), "person grants the bound permission");
    CHECK(ink_setting_get("audio.child_max") == 60, "accepting permission does not accept the other setting");
    CHECK(AI("clipboard.read", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "SYNTHETIC_PRIVATE_CLIPBOARD"), "granted permission works");
    CHECK(ink_confirm_decide(child_rule, true, INK_SRC_TOUCH) == 0 &&
          WAIT_FOR(ink_setting_get("audio.child_max") == 100, 1000), "person accepts exactly the bound child limit");
    CHECK(AI("settings.set", "{\"id\":\"audio.volume\",\"value\":30}") && g_rc.status == INK_RC_DONE, "ordinary volume remains automatic");
    CHECK(AI("settings.set", "{\"id\":\"display.light\",\"value\":10}") && g_rc.status == INK_RC_DONE, "ordinary brightness remains automatic");
    CHECK(BY_HAND("settings.set", "{\"id\":\"ai.read_clipboard\",\"value\":false}") &&
          g_rc.status == INK_RC_PENDING_CONFIRMATION, "permission revocation by hand also uses the card");
    CHECK(ink_confirm_decide(token(), true, INK_SRC_TOUCH) == 0 &&
          WAIT_FOR(ink_setting_get("ai.read_clipboard") == 0, 1000), "person revokes permission");
    CHECK(AI("clipboard.read", NULL) && g_rc.status == INK_RC_REJECTED, "revoked clipboard is private again");
    CHECK(AI("system.undo", "{\"object\":\"setting:ai.read_clipboard\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION,
          "undo cannot silently restore a revoked permission");
    unsigned undo_permission = token();
    CHECK(ink_setting_get("ai.read_clipboard") == 0, "permission inverse waits for a person too");
    CHECK(ink_confirm_decide(undo_permission, false, INK_SRC_TOUCH) == 0, "person can reject the permission inverse");
    CHECK(AI("settings.set", "{\"id\":\"audio.child_max\",\"value\":101}") && g_rc.status == INK_RC_INVALID, "invalid high-risk value is rejected");
    ink_receipt_t rc;
    CHECK(ink_setting_set("ai.read_clipboard", 1, INK_SRC_AI, &rc) == INK_RC_PENDING_CONFIRMATION &&
          ink_setting_get("ai.read_clipboard") == 0, "direct setter cannot bypass permission confirmation");
    unsigned stale = 0; sscanf(rc.object, "confirm:%u", &stale);
    CHECK(ink_member_switch("kid") == 0, "switch while permission waits");
    CHECK(ink_confirm_decide(stale, true, INK_SRC_TOUCH) != 0 && ink_setting_get("ai.read_clipboard") == 0, "old member's card cannot be accepted");
    CHECK(AI("settings.set", "{\"id\":\"audio.child_max\",\"value\":100}") && g_rc.status == INK_RC_REJECTED, "child cannot authorize its own rule");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name="review-regression", .fn=review_run,
        .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1,
        .flags=OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "review task");
}
