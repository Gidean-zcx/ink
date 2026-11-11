/* Enrollment presentation uses synthetic receipts; service flow stays in boot-flow. */
#ifndef INK_UI_FIX_D_SOURCE
#define INK_UI_FIX_D_SOURCE "../../system/ui/shell/boot_flow.c"
#endif
#define key u_boot_key
#include INK_UI_FIX_D_SOURCE
#undef key
#include "common.h"

static const char *u_reason;
static void u_first(void) { B.message[0] = 0; show(FIRST); }
static void u_first_check(void) {
    U_CHECK(u_primary(u_label(lv_screen_active(), "在设备上设置")) &&
        !u_primary(u_label(lv_screen_active(), "先用起来")), "O01 setup has one primary button and an offline exit");
}
static void u_claim(void) {
    snprintf(B.claim, sizeof B.claim, "https://example.com/claim/ABCD1234#s=SYNTH_FINGERPRINT");
    snprintf(B.check, sizeof B.check, "TESTCHECK123"); B.message[0] = 0;
    show(CLAIM);
}
static bool u_visible_contains(lv_obj_t *root, const char *text) {
    if (!lv_obj_is_visible(root)) return false;
    if (lv_obj_check_type(root, &lv_label_class) && strstr(lv_label_get_text(root), text)) return true;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++)
        if (u_visible_contains(lv_obj_get_child(root, (int32_t)i), text)) return true;
    return false;
}
static void u_claim_closed(void) {
    U_CHECK(!u_visible_contains(lv_screen_active(), B.check) && !u_visible_contains(lv_screen_active(), "ABCD1234"), "O07 phone scan hides both joining codes");
}
static void u_claim_open(void) {
    U_CHECK(u_visible_contains(lv_screen_active(), B.check) && u_visible_contains(lv_screen_active(), "ABCD1234"), "O07 computer expansion reveals both required codes");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && !strstr(g_snap, B.check) && !strstr(g_snap, "ABCD1234"), "O07 AI never receives either joining code");
}
static void u_error(void) {
    snprintf(B.picked, sizeof B.picked, "SYNTH_PRIVATE"); show(JOINING);
    ink_receipt_t rc = {.status = INK_RC_FAILED}; snprintf(rc.detail, sizeof rc.detail, "%s", u_reason);
    reply((void *)(((uintptr_t)ink_member_epoch() << 4) | JOIN_REPLY), &rc);
}
static void u_error_check(void) {
    const char *expected = !strcmp(u_reason, "wrong password") ? "密码不对" :
        !strcmp(u_reason, "that network is not around") ? "找不到这个网络" : "连接超时";
    U_CHECK(B.page == JOIN_FAIL && strstr(B.message, expected), "O06 connection failure preserves its actual cause: %.50s", u_reason);
    CHECK(!strcmp(B.picked, "SYNTH_PRIVATE"), "O06 failure preserves the selected network");
}
static void u_restore(void) {
    B.preview[0] = 0; snprintf(B.message, sizeof B.message, "还没有可恢复的备份，可以当新设备用。"); show(RESTORE);
}
static void u_restore_check(void) {
    U_CHECK(u_primary(u_label(lv_screen_active(), "当新设备用")) &&
        !u_primary(u_label(lv_screen_active(), "再查一次")), "O10 no backup makes direct use the primary action");
}
static void u_hub(void) { show(NO_HUB); }
static void u_hub_check(void) {
    U_CHECK(u_label(lv_screen_active(), "返回选网") && u_label(lv_screen_active(), "先用起来"), "O08 no-server page has usable setup and offline exits");
    lv_area_t message = u_rect(u_label(lv_screen_active(), B.message));
    lv_area_t back = u_rect(u_label(lv_screen_active(), "返回选网"));
    U_CHECK(message.y2 < back.y1, "O08 server instructions leave space above the secondary action");
}
static void u_revoked(void) { show(REVOKED); }
static void u_run(void *ctx) {
    (void)ctx; u_start(); on_ui(u_first); u_quiet(); on_ui(u_first_check); u_capture("FirstBoot");
    on_ui(u_claim); u_quiet(); on_ui(u_claim_closed); u_capture("BootClaim");
    u_command("shell.launch", "{\"id\":\"boot.computer\"}"); on_ui(u_claim_open); u_capture("BootComputer");
    if (!u_before) { u_command("shell.launch", "{\"id\":\"boot.computer\"}"); on_ui(u_claim_closed); }
    const char *errors[] = {"wrong password", "that network is not around", "it took too long"};
    for (unsigned i = 0; i < sizeof errors / sizeof *errors; i++) {
        u_reason = errors[i]; on_ui(u_error); u_quiet(); on_ui(u_error_check);
        if (i == 0) u_capture("BootAuthFail");
        if (i == 2) u_capture("BootTimeout");
    }
    on_ui(u_restore); u_quiet(); on_ui(u_restore_check); u_capture("BootRestore");
    on_ui(u_hub); u_quiet(); on_ui(u_hub_check); u_capture("BootNoHub");
    on_ui(u_revoked); u_quiet(); u_capture("BootRevoked");
    ink_probe_count("test.finished");
}
void ink_main(void) { u_launch(u_run); }
