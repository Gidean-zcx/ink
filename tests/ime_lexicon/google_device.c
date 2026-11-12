/* End-to-end decoder comparison, member learning and real keyboard frames. */
#include <stdlib.h>
#include <time.h>
#include "ink/sys/storage.h"
#include "ink/sys/textinput.h"
#include "../../system/services/text/text_internal.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"
#include "ime_cases.h"
#include "google_sentences.h"
static bool google, verify, learning;
static const char *key_now, *picture;
static char key_buf[41], row[2048];
static ink_text_cand_t choices[60];
static int count, selected, row_index;
static bool nine_now;
static volatile bool captured;
static struct timespec tick;
static double elapsed;
static void host_mode(void *arg) { (void)arg; const char *m = getenv("INK_GOOGLE_MODE"); google = !m || strcmp(m, "old"); verify = m && !strcmp(m, "verify"); learning = m && (!strcmp(m, "learn") || verify); }
static void host_start(void *arg) { (void)arg; clock_gettime(CLOCK_MONOTONIC, &tick); }
static void host_stop(void *arg) { (void)arg; struct timespec end; clock_gettime(CLOCK_MONOTONIC, &end); elapsed = (end.tv_sec - tick.tv_sec) * 1e6 + (end.tv_nsec - tick.tv_nsec) / 1e3; }
static void host_row(void *arg) { (void)arg; printf("GOOGLE\t%d\t%s\t%.3f", row_index, key_now, elapsed); for (int i = 0; i < count && i < 5; i++) printf("\t%s:%u", choices[i].word, choices[i].consumed); printf("\n"); }
static void set_engine(void) { ink_receipt_t rc; CHECK(ink_setting_set("text.google", google, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "set member decoder"); }
static void focus_ui(void) { ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 4095}; CHECK(ink_text_focus(&cfg, NULL, NULL) > 0, "focus public input session"); if (ink_text_english()) ink_text_key(INK_TEXT_KEY_LANGUAGE); }
static void focus(void) { on_ui(set_engine); on_ui(focus_ui); CHECK(WAIT_FOR(py_dict_loaded(), 10000), "dictionary opens on the simulated device"); }
static void query_ui(void) {
    ink_receipt_t rc; ink_setting_set("text.nine_key", nine_now, INK_SRC_TOUCH, &rc);
    while (*ink_text_composing()) ink_text_backspace();
    ink_text_select(0, ink_text_char_offset(ink_text_value(), strlen(ink_text_value()))); ink_text_backspace();
    vtk_host_call(host_start, NULL);
    for (const char *p = key_now; *p; p++) { ink_text_key(*p); count = ink_text_candidates(0, choices, 60); }
    vtk_host_call(host_stop, NULL);
}
static void query(const char *key, bool nine) { key_now = key; nine_now = nine; on_ui(query_ui); }
static int find(const char *word) { for (int i = 0; i < count; i++) if (!strcmp(choices[i].word, word)) return i; return -1; }
static void pick_ui(void) { ink_text_pick(selected); count = ink_text_candidates(0, choices, 60); }
static void password_ui(void) { ink_text_cfg_t cfg = {.type = INK_TEXT_PASSWORD}; ink_text_focus(&cfg, NULL, NULL); for (const char *p = "SYNTHETIC_SECRET"; *p; p++) ink_text_key(*p); CHECK(!ink_text_candidates(0, choices, 60), "password has no decoder candidates"); }
static void frame(void *arg) { sim_display_snapshot(arg); captured = true; }
static void capture(const char *name) { CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 15000), "keyboard frame settles"); picture = name; captured = false; vtk_event_at(vtk_now() + SIM_US(1), frame, (void *)picture); CHECK(WAIT_FOR(captured, 2000), "keyboard frame saved"); }
static void digitize(const char *p) { const char *map = "22233344455566677778889999"; int i = 0; for (; *p; p++) key_buf[i++] = map[*p - 'a']; key_buf[i] = 0; }
static void font_ui(void) { ink_receipt_t rc; ink_setting_set("display.font_size", selected, INK_SRC_TOUCH, &rc); }
static void compare(void) {
    for (row_index = 0; row_index < (int)(sizeof CASES / sizeof *CASES); row_index++) { query(CASES[row_index].key, false); vtk_host_call(host_row, NULL); }
    for (int n = 0; n < 2; n++) for (int i = 0; i < 10; i++) {
        row_index = 90 + n * 10 + i;
        if (n) { digitize(SENTENCES[i].key); query(key_buf, true); } else query(SENTENCES[i].key, false);
        vtk_host_call(host_row, NULL);
    }
    static const char *const keys[] = {"nihao", "zhongguo", "jintiantianqizenmeyang", "xx", "94664"};
    for (int font = 1; font <= 3; font += 2) {
        selected = font; on_ui(font_ui);
        for (int i = 0; i < 5; i++) { query(keys[i], i == 4); snprintf(row, sizeof row, "ime-%s-%s-%s", google ? "google" : "old", font == 1 ? "normal" : "large", keys[i]); capture(row); }
    }
}
static void composition(void) {
    query("woaibeijingtiananmen", false);
    CHECK(count && !strcmp(choices[0].word, "我爱北京天安门"), "full sentence is candidate zero");
    selected = find("我爱"); CHECK(selected >= 0, "partial word is selectable");
    if (selected >= 0) on_ui(pick_ui);
    CHECK(!strcmp(ink_text_value(), "我爱") && !strcmp(ink_text_composing(), "beijingtiananmen"), "partial pick retains the unconsumed spelling");
    CHECK(count && !strcmp(choices[0].word, "北京天安门"), "suffix continues composing");
    selected = 0; on_ui(pick_ui);
    CHECK(!strcmp(ink_text_value(), "我爱北京天安门") && !*ink_text_composing(), "sentence commits once");
    CHECK(count > 0, "local continuation candidates follow the selection");
    query("946644862", true); selected = find("中国");
    ink_probe_value("ime.google_nine_china", selected >= 0);
    if (selected >= 0) { on_ui(pick_ui); ink_probe_value("ime.google_nine_suffix", !strcmp(ink_text_composing(), "2")); }
}
static void switch_to(const char *member) { CHECK(ink_member_switch(member) == 0, "switch synthetic member"); CHECK(WAIT_FOR(!ink_text_focused(), 3000), "handoff ends old session"); focus(); }
static void member_learning(void) {
    query("beijing", false);
    if (verify) CHECK(count && !strcmp(choices[0].word, "背景"), "owner learning survives restart");
    else { selected = find("背景"); CHECK(selected >= 0, "rare selection is available"); if (selected >= 0) on_ui(pick_ui); query("beijing", false); CHECK(count && !strcmp(choices[0].word, "背景"), "owner selected word becomes first"); }
    switch_to("kid"); query("beijing", false); CHECK(count && !strcmp(choices[0].word, "北京"), "child cannot see owner's learned ranking");
    switch_to("owner"); query("beijing", false); CHECK(count && !strcmp(choices[0].word, "背景"), "owner ranking returns");
    on_ui(password_ui);
    CHECK(AI("text.state", NULL) && !strstr(g_result, "SYNTHETIC_SECRET"), "password is absent from AI state");
    CHECK(AI("keyboard.pick", "{\"index\":0}") && g_rc.status == INK_RC_REJECTED, "AI cannot pick for password");
}
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home(); focus();
    if (learning) member_learning(); else { compare(); if (google) composition(); }
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 10000), "state durable"); ink_probe_count("test.finished");
}
void ink_main(void) { vtk_host_call(host_mode, NULL); product_main(); osal_task_cfg_t cfg = {.name = "google-ime-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL}; CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start decoder driver"); }
