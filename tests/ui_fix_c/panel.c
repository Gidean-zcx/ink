#ifndef UI_FIX_PRODUCT
#define UI_FIX_PRODUCT "../../system/ui/shell/ai_panel.c"
#endif
#define key fx_product_panel_key
#include UI_FIX_PRODUCT
#undef key
#include "common.h"

static char book[4096], restored[4096], long_answer[4096];
static void fx_book(void) { CHECK(ink_ui_snapshot(book, sizeof book, NULL) > 0, "book position snapshot"); }
static void fx_restored(void) { CHECK(ink_ui_snapshot(restored, sizeof restored, NULL) > 0, "restored book snapshot"); }
static void fx_ask(void) { ink_ai_ask_text("合成 UI 面板问题"); }
static void fx_long(void) {
    const char *line = "合成回答用于检查分页和大字号，书页保持原位。";
    long_answer[0] = 0;
    for (int i = 0; i < 16; i++) strcat(long_answer, line);
    strcat(long_answer, TR(INK_AI_ANSWER_CUT));
    ink_shell_ai_words(long_answer); ink_shell_ai_status("回答完成", INK_AI_ERROR_NONE);
}
static void fx_panel_check(void) {
    if (fx_before) return;
    CHECK(A.pages > 1 && A.page == 0, "first page of long answer");
    CHECK(strstr(lv_label_get_text(A.source), "只保留前一部分"), "first page warns that the answer was cut");
    CHECK(strstr(lv_label_get_text(A.source), "翻页"), "first page announces more pages");
    fx_fits(A.source, A.root); fx_fits(A.counter, A.actions);
    lv_obj_t *close = fx_bound(A.root, "ai.panel.hide");
    CHECK(close && lv_obj_is_visible(A.counter), "page counter and close remain visible");
    if (close) { lv_area_t a; lv_obj_get_coords(close, &a); CHECK(a.y1 >= 0 && a.y2 < ink_ui_height(), "close is on screen"); }
    CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(A.replay, 0)), "听解释"), "first replay label does not claim it was heard");
    CHECK(strstr(fx_snapshot, "只保留前一部分"), "AI sees the same truncation boundary");
}
static void fx_last(void) {
    ink_receipt_t rc;
    while (A.page + 1 < A.pages) ink_cap_invoke("ai.panel.next", NULL, INK_SRC_AI, 0, &rc);
    if (!fx_before) CHECK(strstr(lv_label_get_text(A.source), "只保留前一部分"), "last page keeps the truncation warning");
}
static void fx_listening_check(void) {
    if (fx_before) return;
    CHECK(fx_bound(A.root, "ai.panel.listen") && fx_label(A.root, "说完了"), "tap listening has a visible finish action");
}
static void fx_speech_check(void) {
    if (fx_before) return;
    CHECK(A.speaking && fx_label(A.root, "停止朗读"), "stop action names speech");
}
static void fx_replay_check(void) {
    if (!fx_before) CHECK(!strcmp(lv_label_get_text(lv_obj_get_child(A.replay, 0)), "再听一遍"), "played answer is labelled replay");
}
static bool fx_speaking(void) {
    char playing[16] = ""; ink_state_get_str(ink_state_find("audio.now_playing"), playing, sizeof playing);
    return !strcmp(playing, "voice");
}
static void run(void *ctx) {
    (void)ctx; fx_start();
    fx_ok("reader.open_book", "{\"title\":\"测试长书\"}"); fx_ok("reader.goto_page", "{\"page\":5}"); on_ui(fx_book);
    on_ui(fx_ask); CHECK(WAIT_FOR(sim_metric_value("ai.answers") >= 1, 20000), "synthetic answer arrives"); fx_quiet();
    on_ui(fx_long); fx_capture("panel"); on_ui(fx_panel_check); on_ui(fx_last); fx_capture("panel-last");
    fx_press("ai.panel.hide"); on_ui(fx_restored); CHECK(!strcmp(book, restored), "closing AI restores the exact book position");
    fx_ok("ai.panel.listen", NULL); fx_quiet(); on_ui(fx_listening_check); fx_capture("panel-listen");
    if (fx_before) fx_ok("ai.panel.listen", NULL); else fx_press("ai.panel.listen");
    CHECK(WAIT_FOR(fx_speaking(), 20000), "real synthetic speech begins"); fx_quiet(); on_ui(fx_speech_check); fx_capture("panel-speech");
    fx_press("shell.ai_stop"); CHECK(WAIT_FOR(!fx_speaking(), 5000), "stop ends speech"); fx_quiet(); on_ui(fx_replay_check);
    fx_capture("panel-replay"); fx_ok("ai.panel.hide", NULL);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    vtk_host_call(fx_options, NULL); product_main();
    osal_task_cfg_t cfg = {.name = "ui-fix-panel", .fn = run, .stack_bytes = 24576, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start UI panel driver");
}
