/* Exercise real draft and memo storage with one injected memo write failure. */
#include "ink/sys/data.h"
static bool u_refuse;
static int u_put(ink_db_t *db, const ink_db_write_t *w, uint32_t *id) {
    return u_refuse ? INK_DB_ERR_FULL : ink_db_put(db, w, id);
}
#ifndef INK_UI_FIX_D_SOURCE
#define INK_UI_FIX_D_SOURCE "../../system/apps/memo/memo.c"
#endif
#define ink_db_put u_put
#include INK_UI_FIX_D_SOURCE
#undef ink_db_put
#include "common.h"

static int u_editor_height;
static void u_editor_check(void) {
    lv_obj_t *area = u_textarea(M.field); CHECK(area, "memo uses the real text area");
    if (!area) return;
    lv_area_t field = u_rect(M.field), keyboard; ink_shell_keyboard_area(&keyboard);
    lv_obj_t *caption = u_label(M.field, lv_label_get_text(M.notice));
    lv_area_t notice = u_rect(caption), input = u_rect(area);
    U_CHECK(caption && notice.y2 < input.y1 && field.y2 < keyboard.y1, "N09 save result, editor and keyboard do not overlap");
    U_CHECK(!u_label(lv_screen_active(), "已存在这台设备"), "N09 memo shows only its own save result");
    u_editor_height = lv_obj_get_height(area);
}
static void u_failure_check(void) {
    u_editor_check();
    U_CHECK(strstr(lv_label_get_text(M.notice), "草稿") && strstr(lv_label_get_text(M.notice), "没存上"), "N09 failure distinguishes the saved draft from the unsaved memo");
    CHECK(!strcmp(ink_textfield_text(M.field), "合成备忘，失败后文字仍在。"), "N09 failure preserves exact text");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0 && strstr(g_snap, "没存上") && strstr(g_snap, "memo.retry"), "N09 AI sees the same failure and retry action");
}
static void u_detail_check(void) {
    lv_obj_t *edit = u_cap(lv_screen_active(), "memo.edit");
    lv_obj_t *del = u_cap(lv_screen_active(), "memo.delete");
    lv_obj_t *convert = u_cap(lv_screen_active(), "memo.convert");
    lv_area_t a = u_rect(edit), b = u_rect(del), c = u_rect(convert);
    U_CHECK(edit && del && convert && b.x1 - a.x2 - 1 >= ink_ui_px(INK_GAP) - 1 &&
        c.x1 - b.x2 - 1 >= ink_ui_px(INK_GAP) - 1, "N03 detail footer separates its three actions");
    U_CHECK(u_primary(u_label(lv_screen_active(), "编辑备忘")) && !u_primary(u_label(lv_screen_active(), "删除备忘")), "N03 edit is the primary detail action");
}
static void u_keyboard_height_change(void) {
    /* A prior phone/number field is a real taller keyboard geometry at editor creation. */
    ink_text_cfg_t cfg = {.type = INK_TEXT_NUMBER, .max = 8};
    ink_text_focus(&cfg, NULL, NULL);
}
static void u_run(void *ctx) {
    (void)ctx; u_start();
    u_command("app.open", "{\"id\":\"memo\"}");
    CHECK(WAIT_FOR(M.db && M.list, 15000), "memo collection is ready");
    u_command("memo.edit", NULL);
    u_command("text.insert", "{\"text\":\"合成备忘，失败后文字仍在\"}");
    CHECK(WAIT_FOR(!M.saving && ink_db_idle() && ink_text_draft_saved() == 1, 15000), "memo and draft are durable");
    u_quiet(); on_ui(u_editor_check); u_capture("MemoSaved");
    u_refuse = true; u_command("text.insert", "{\"text\":\"。\"}");
    CHECK(WAIT_FOR(M.mode == FAIL && ink_text_draft_saved() == 1, 15000), "failure keeps the durable draft");
    u_quiet(); on_ui(u_failure_check); u_capture("MemoFail");
    u_refuse = false; u_command("memo.retry", NULL);
    CHECK(WAIT_FOR(M.mode == EDIT && !M.saving && ink_db_idle(), 15000), "explicit retry saves the unchanged draft");
    u_quiet(); on_ui(u_editor_check); u_capture("MemoRetried");
    uint32_t id = M.id; char args[160]; u_command("text.submit", NULL);
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id);
    u_command("memo.open", args); on_ui(u_detail_check); u_capture("MemoDetail");
    /* Number -> full -> nine -> full must recover the editor's available height. */
    on_ui(u_keyboard_height_change); u_command("memo.edit", args);
    on_ui(u_editor_check); int full = u_editor_height; u_capture("MemoFromNumber");
    u_command("keyboard.key", "{\"code\":4}"); on_ui(u_editor_check); u_capture("MemoNine");
    U_CHECK(abs(u_editor_height - full) <= ink_ui_px(INK_GAP_XS), "memo keeps available height when switching to nine keys");
    u_command("keyboard.key", "{\"code\":4}"); on_ui(u_editor_check); u_capture("MemoFull");
    U_CHECK(abs(u_editor_height - full) <= ink_ui_px(INK_GAP_XS), "memo restores full-keyboard height");
    ink_probe_count("test.finished");
}
void ink_main(void) { u_launch(u_run); }
