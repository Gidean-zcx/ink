/* UX20 correction presentation on the existing evaluator and real capability bus. */
#include "ui_fix_b/common.h"
#ifndef INK_TEST_CALCULATOR
#define INK_TEST_CALCULATOR "../system/apps/calculator/calculator.c"
#endif
#include INK_TEST_CALCULATOR

static void correction_layout(void) {
    lv_obj_t *hint = test_label(lv_screen_active(), "按删除修改算式，再计算");
    CHECK(hint && lv_obj_is_visible(hint) && test_fits(hint), "UX20 Q03 correction hint is fully visible");
    if (hint) {
        lv_area_t h, r; lv_obj_get_coords(hint, &h); lv_obj_get_coords(K.result_label, &r);
        CHECK(r.y2 < h.y1 && h.y2 < ink_ui_px(keys_top()), "UX20 Q03 error, correction and keys do not overlap");
    }
    CHECK(test_fits(K.expr_label), "UX20 Q02 expression stays visible above the error");
}
static void calc_key(const char *key) {
    char args[40]; snprintf(args, sizeof args, "{\"key\":\"%s\"}", key);
    CHECK(BY_HAND("calc.key", args) && g_rc.status == INK_RC_DONE, "press real calculator key %s", key); test_quiet();
}
static void test_run(void *arg) {
    (void)arg; test_init();
    CHECK(AI("app.open", "{\"id\":\"calculator\"}") && g_rc.status == INK_RC_DONE, "open calculator"); test_quiet();
    const char *const expressions[] = {"5/0", "1+", "9999999999*10"};
    const char *const frames[] = {"division-error", "syntax-error", "range-error"};
    _Static_assert(sizeof expressions / sizeof *expressions == sizeof frames / sizeof *frames, "error fixtures match");
    for (unsigned i = 0; i < sizeof expressions / sizeof *expressions; i++) {
        char args[96]; snprintf(args, sizeof args, "{\"expr\":\"%s\"}", expressions[i]);
        CHECK(AI("calc.eval", args) && g_rc.status == INK_RC_INVALID, "real evaluator rejects %s", expressions[i]); test_quiet();
        CHECK(!strcmp(K.expr, expressions[i]), "UX20 Q03 the error keeps the whole expression");
        on_ui(correction_layout); test_frame(frames[i]);
        CHECK(strstr(test_snapshot, "按删除修改算式，再计算"), "UX20 Q03 the AI sees the same correction hint");
        calc_key("back");
        CHECK(strlen(K.expr) == strlen(expressions[i]) - 1 && !strncmp(K.expr, expressions[i], strlen(K.expr)), "UX20 Q03 delete edits one character after an error");
        if (!i) { calc_key("2"); calc_key("="); CHECK(!strcmp(K.result, "2.5"), "UX20 Q03 correcting 5/0 to 5/2 works"); test_frame("corrected"); }
    }
    ink_probe_count("test.finished");
}
