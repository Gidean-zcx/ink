#ifndef UI_FIX_PRODUCT
#define UI_FIX_PRODUCT "../../system/ui/shell/airplane_card.c"
#endif
#include UI_FIX_PRODUCT
#include "common.h"
static void show_fixture(void) {
    ink_receipt_t rc = {.status = INK_RC_REJECTED, .detail = "airplane mode is on", .say = "飞行模式已打开，暂时不能打电话"};
    CHECK(ink_shell_airplane_card(&rc), "Z06 existing refusal presenter accepts receipt");
}
static void inspect_child(void) {
    CHECK(fx_label(A.card.panel, "不能打电话") && fx_label(A.card.panel, "请家长关闭飞行模式"), "Z06 child sees both affected action and parent recovery path");
    CHECK(!fx_label(A.card.panel, "关闭飞行模式") || A.why, "Z06 child has no airplane switch action");
    CHECK(lv_obj_get_child_count(A.card.row) == 1, "Z06 child only has acknowledgement");
    fx_fits(A.card.body, A.card.panel);
}
static void run(void *ctx) {
    (void)ctx; fx_start(); int64_t original = ink_setting_get("net.airplane");
    on_ui(show_fixture); fx_capture("airplane-adult"); fx_press("取消");
    CHECK(!A.card.root && ink_setting_get("net.airplane") == original, "Z06 cancel leaves mode unchanged");
    CHECK(!ink_member_switch("kid"), "enter child space"); fx_quiet(); on_ui(show_fixture); fx_capture("airplane-child");
    if (!fx_before) on_ui(inspect_child);
    fx_press("知道了"); CHECK(!A.card.root && ink_setting_get("net.airplane") == original, "Z06 child acknowledgement cannot disable mode");
    ink_probe_count("test.finished");
}
FX_MAIN("ui-fix-g-airplane")
