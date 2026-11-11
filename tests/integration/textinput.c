/*
 * Text input and the clipboard (system services design §9), on the whole product. INK_TEXT_PHASE picks the run: main (the
 * session, the engine, the keyboard, the text field, what the AI can do, the clipboard), nocard and baddict (without a
 * good dictionary the small table does). Results are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_storage.h"
#include "ink/sys/crc.h"
#include "ink/sys/members.h"
#include "ink/sys/res.h"
#include "ink/sys/settings.h"
#include "ink/sys/textfield.h"
#include "ink/sys/textinput.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

static volatile int g_entered, g_ended, g_changed;
static char g_entered_text[256];
static void cl_changed(void *ctx) {
    (void)ctx;
    g_changed++;
}
static void cl_entered(void *ctx, const char *value) {
    (void)ctx;
    snprintf(g_entered_text, sizeof g_entered_text, "%s", value);
    g_entered++;
}
static void cl_ended(void *ctx) {
    (void)ctx;
    g_ended++;
}
static const ink_text_client_t CLIENT = {cl_changed, cl_entered, cl_ended, NULL};

/* Everything about the session runs on the UI task. */
static ink_text_cfg_t g_cfg;
static int g_sess;
static void do_focus(void) { g_sess = ink_text_focus(&g_cfg, &CLIENT, NULL); }
static void do_blur(void) { ink_text_blur(g_sess); }
static void focus_as(ink_text_type_t type, const char *initial, size_t max) {
    g_cfg = (ink_text_cfg_t){.type = type, .initial = initial, .max = max, .hint = "写点什么"};
    on_ui(do_focus);
}
static const char *g_typing;
static void do_type(void) {
    for (const char *p = g_typing; *p; p++) ink_text_key(*p);
}
static void type(const char *s) {
    g_typing = s;
    on_ui(do_type);
}
static int g_pick_i;
static void do_pick(void) { ink_text_pick(g_pick_i); }
static void pick(int i) {
    g_pick_i = i;
    on_ui(do_pick);
}
static void do_backspace(void) { ink_text_backspace(); }
static void do_enter(void) { ink_text_enter(); }

static ink_text_cand_t g_c[64];
static int g_nc;
static char g_val[512], g_comp[64];
static void do_read(void) {
    g_nc = ink_text_candidates(0, g_c, 64);
    snprintf(g_val, sizeof g_val, "%s", ink_text_value());
    snprintf(g_comp, sizeof g_comp, "%s", ink_text_composing());
}
static void look(void) { on_ui(do_read); }

static bool cand_is(int i, const char *word, int consumed) { return i < g_nc && !strcmp(g_c[i].word, word) && g_c[i].consumed == consumed; }

static void engine(void) {
    focus_as(INK_TEXT_PLAIN, NULL, 0);
    CHECK(WAIT_FOR(sim_metric_value("text.dict_loaded") >= 1, 3000), "the dictionary comes in by itself");
    type("xian");
    look();
    CHECK(!strcmp(g_comp, "xian") && !g_val[0], "letters are composed (%s, %s)", g_comp, g_val);
    CHECK(cand_is(0, "先", 4) && cand_is(1, "现", 4) && cand_is(2, "线", 4) && cand_is(3, "西安", 4), "the whole key first, by frequency; the data decides what it means (%s %s %s %s)", g_c[0].word, g_c[1].word, g_c[2].word, g_c[3].word);
    CHECK(cand_is(4, "下", 3) && cand_is(5, "夏", 3) && cand_is(6, "西", 2) && cand_is(7, "系", 2), "then shorter keys (%s %s %s %s)", g_c[4].word, g_c[5].word, g_c[6].word, g_c[7].word);
    pick(6);
    look();
    CHECK(!strcmp(g_val, "西") && !strcmp(g_comp, "an"), "a pick takes its letters, the rest go on (%s, %s)", g_val, g_comp);
    CHECK(cand_is(0, "安", 2) && cand_is(1, "按", 2), "with candidates of their own (%s)", g_c[0].word);
    pick(0);
    look();
    CHECK(!strcmp(g_val, "西安") && !g_comp[0] && g_nc == 0, "done (%s)", g_val);

    /* whole words win */
    type("zhongguo");
    look();
    CHECK(cand_is(0, "中国", 8), "a whole word first (%s)", g_c[0].word);
    /* space takes the first */
    type(" ");
    look();
    CHECK(!strcmp(g_val, "西安中国") && !g_comp[0], "space takes the first (%s)", g_val);
    /* other characters: what is composed goes in as typed */
    type("abc,");
    look();
    CHECK(!strcmp(g_val, "西安中国abc，") && !g_comp[0], "a Chinese comma commits composing (%s)", g_val);
    /* enter with something composed: it goes in, nothing is sent */
    int entered = g_entered;
    type("xyz");
    on_ui(do_enter);
    look();
    CHECK(!strcmp(g_val, "西安中国abc，xyz") && g_entered == entered, "enter takes the letters first (%s)", g_val);
    on_ui(do_enter);
    CHECK(g_entered == entered + 1 && !strcmp(g_entered_text, "西安中国abc，xyz"), "then it means enter (%s)", g_entered_text);
    /* backspace: letters, then characters, whole */
    type("ni");
    on_ui(do_backspace);
    look();
    CHECK(!strcmp(g_comp, "n"), "a letter first (%s)", g_comp);
    on_ui(do_backspace);
    for (int i = 0; i < 7; i++) on_ui(do_backspace);
    look();
    CHECK(!strcmp(g_val, "西安中国"), "characters, whole (%s)", g_val);
    on_ui(do_backspace);
    look();
    CHECK(!strcmp(g_val, "西安中"), "not a byte of one (%s)", g_val);
    /* a word this member takes comes first */
    on_ui(do_blur);
    CHECK(g_ended >= 1, "the session ended, and said so");
    focus_as(INK_TEXT_PLAIN, NULL, 0);
    for (int i = 0; i < 3; i++) {
        type("xian");
        look();
        int at = -1;
        for (int k = 0; k < g_nc; k++)
            if (!strcmp(g_c[k].word, "线")) at = k;
        pick(at);
    }
    type("xian");
    look();
    CHECK(cand_is(0, "线", 4), "what was picked comes first now (%s)", g_c[0].word);
    CHECK(cand_is(4, "下", 3), "within keys that long only");
    on_ui(do_blur);
    /* not for someone else */
    CHECK(ink_member_switch("kid") == 0, "someone else");
    delay_ms(300);
    focus_as(INK_TEXT_PLAIN, NULL, 0);
    type("xian");
    look();
    CHECK(cand_is(0, "先", 4), "has their own order (%s)", g_c[0].word);
    /* and a switch ends what was being typed */
    int ended = g_ended;
    ink_member_test_reset();
    CHECK(WAIT_FOR(g_ended == ended + 1, 1000) && !ink_text_focused(), "the device changing hands ends the session");
}

static void types(void) {
    /* a password: nothing composed, nothing learned, nothing shown */
    focus_as(INK_TEXT_PASSWORD, NULL, 16);
    type("xianAB1!");
    look();
    CHECK(!strcmp(g_val, "xianAB1!") && !g_comp[0] && g_nc == 0, "a password is typed as it is (%s)", g_val);
    CHECK(AI("text.state", NULL) && strstr(g_result, "\"type\":\"password\"") && strstr(g_result, "\"hidden\":true") && !strstr(g_result, "xianAB"), "the AI does not see it (%s)", g_result);
    CHECK(AI("text.insert", "{\"text\":\"x\"}") && g_rc.status == INK_RC_REJECTED, "nor fills it in");
    CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_REJECTED, "nor sends it");
    on_ui(do_blur);
    /* numbers and phone numbers: only what belongs */
    focus_as(INK_TEXT_NUMBER, NULL, 0);
    type("a1.5-x");
    look();
    CHECK(!strcmp(g_val, "1.5-"), "a number field (%s)", g_val);
    on_ui(do_blur);
    focus_as(INK_TEXT_PHONE, NULL, 0);
    type("+86 138#");
    look();
    CHECK(!strcmp(g_val, "+86138#"), "a phone field (%s)", g_val);
    on_ui(do_blur);
    /* at most this many bytes, whole characters */
    focus_as(INK_TEXT_PLAIN, "abcd", 8);
    type("nihao ");
    look();
    CHECK(!strcmp(g_val, "abcd你") || !strcmp(g_val, "abcd"), "as much as fits (%s)", g_val);
    CHECK(strlen(g_val) <= 8, "and never more (%zu)", strlen(g_val));
    on_ui(do_blur);
    /* one session at a time */
    focus_as(INK_TEXT_PLAIN, "第一个", 0);
    int ended = g_ended, first = g_sess;
    focus_as(INK_TEXT_PLAIN, "第二个", 0);
    look();
    CHECK(g_sess != first && g_ended == ended + 1 && !strcmp(g_val, "第二个"), "the next takes over (%s)", g_val);
    on_ui(do_blur);
}

/* ---- editing: the cursor, the selection, undo ---- */

static size_t g_a, g_b, g_cur, g_sel_a, g_sel_b;
static bool g_ok;
static const char *g_commit;
static char g_selected[128];
static int g_selected_n;
static void do_cursor(void) { ink_text_set_cursor(g_a); }
static void do_select(void) { ink_text_select(g_a, g_b); }
static void do_word(void) { ink_text_select_word(g_a); }
static void do_move(void) { ink_text_move((int)(long)g_a, g_b != 0); }
static void do_undo(void) { g_ok = ink_text_undo(); }
static void do_redo(void) { g_ok = ink_text_redo(); }
static void do_commit(void) { ink_text_commit_utf8(g_commit); }
static void do_where(void) {
    g_cur = ink_text_cursor();
    ink_text_selection(&g_sel_a, &g_sel_b);
    g_selected_n = ink_text_selected(g_selected, sizeof g_selected);
    snprintf(g_val, sizeof g_val, "%s", ink_text_value());
    snprintf(g_comp, sizeof g_comp, "%s", ink_text_composing());
}
static void where(void) { on_ui(do_where); }
static void cursor_to(size_t at) {
    g_a = at;
    on_ui(do_cursor);
}
static void select_range(size_t a, size_t b) {
    g_a = a;
    g_b = b;
    on_ui(do_select);
}
static void select_word_at(size_t at) {
    g_a = at;
    on_ui(do_word);
}
static void move_by(int chars, bool end) {
    g_a = (size_t)(long)chars;
    g_b = end;
    on_ui(do_move);
}
static void commit(const char *s) {
    g_commit = s;
    on_ui(do_commit);
}
static bool undo(void) {
    on_ui(do_undo);
    return g_ok;
}
static bool redo(void) {
    on_ui(do_redo);
    return g_ok;
}
#define VALUE_IS(want, what) \
    do { \
        where(); \
        CHECK(!strcmp(g_val, want), "%s (%s)", what, g_val); \
    } while (0)

static void editing(void) {
    focus_as(INK_TEXT_PLAIN, "今天天气很好", 0);
    where();
    CHECK(g_cur == 6 && g_sel_a == 6 && g_sel_b == 6, "the cursor starts at the end, in characters (%d)", (int)g_cur);
    /* at the cursor */
    cursor_to(2);
    commit("真的");
    VALUE_IS("今天真的天气很好", "text goes in at the cursor");
    CHECK(g_cur == 4, "and the cursor after it (%d)", (int)g_cur);
    type("ab");
    where();
    CHECK(!strcmp(g_comp, "ab") && !strcmp(g_val, "今天真的天气很好"), "what is being composed is not in it yet");
    on_ui(do_enter);
    VALUE_IS("今天真的ab天气很好", "then in that place");
    CHECK(g_cur == 6, "(%d)", (int)g_cur);
    on_ui(do_backspace);
    VALUE_IS("今天真的a天气很好", "backspace takes what is before the cursor");
    cursor_to(2);
    on_ui(do_backspace);
    VALUE_IS("今真的a天气很好", "a whole character");
    CHECK(g_cur == 1, "and the cursor goes with it (%d)", (int)g_cur);
    cursor_to(0);
    on_ui(do_backspace);
    VALUE_IS("今真的a天气很好", "nothing before it: nothing happens");
    cursor_to(999);
    where();
    CHECK(g_cur == 8, "past the end: the end (%d)", (int)g_cur);
    /* moving the cursor takes in what was being composed */
    cursor_to(1);
    type("xy");
    cursor_to(0);
    VALUE_IS("今xy真的a天气很好", "what was being composed goes in where it was");
    /* selecting */
    select_range(3, 5);
    where();
    CHECK(g_sel_a == 3 && g_sel_b == 5 && !strcmp(g_selected, "真的"), "selected (%d, %d, %s)", (int)g_sel_a, (int)g_sel_b, g_selected);
    commit("很");
    VALUE_IS("今xy很a天气很好", "text takes the place of what is selected");
    CHECK(g_sel_a == g_sel_b && g_cur == 4, "and nothing is selected after (%d)", (int)g_cur);
    select_range(4, 2); /* either way round */
    where();
    CHECK(g_sel_a == 2 && g_sel_b == 4 && !strcmp(g_selected, "y很"), "(%s)", g_selected);
    on_ui(do_backspace);
    VALUE_IS("今xa天气很好", "backspace takes all of what is selected");
    on_ui(do_blur);
    /* a word */
    focus_as(INK_TEXT_PLAIN, "say hello, 今天天气好。再见", 0);
    select_word_at(5);
    where();
    CHECK(!strcmp(g_selected, "hello"), "letters that go together (%s)", g_selected);
    select_word_at(12);
    where();
    CHECK(!strcmp(g_selected, "天天"), "Chinese: two characters (%s)", g_selected);
    select_word_at(15);
    where();
    CHECK(!strcmp(g_selected, "气好") , "the last before punctuation: with the one before it (%s)", g_selected);
    select_word_at(9);
    where();
    CHECK(!strcmp(g_selected, ","), "punctuation: itself (%s)", g_selected);
    /* the arrows: nothing selected, the cursor */
    cursor_to(1);
    move_by(1, true);
    where();
    CHECK(g_cur == 2 && g_sel_a == g_sel_b, "moved (%d)", (int)g_cur);
    move_by(-5, true);
    where();
    CHECK(g_cur == 0, "not before the start (%d)", (int)g_cur);
    /* selected: its end, or its start */
    select_word_at(12);
    move_by(1, true);
    where();
    CHECK(!strcmp(g_selected, "天天气"), "the end moves (%s)", g_selected);
    move_by(-1, false);
    where();
    CHECK(!strcmp(g_selected, "今天天气"), "the start moves (%s)", g_selected);
    move_by(-20, true);
    where();
    CHECK(g_sel_a == g_sel_b && g_sel_a == 11, "the end does not go past the start (%d, %d)", (int)g_sel_a, (int)g_sel_b);
    on_ui(do_blur);
    /* only so much fits, whole characters, wherever it goes in */
    focus_as(INK_TEXT_PLAIN, "一二三四", 16);
    cursor_to(1);
    commit("五六七");
    VALUE_IS("一五二三四", "as much as fits (the limit is in bytes)");
    on_ui(do_blur);
    /* undo */
    focus_as(INK_TEXT_PLAIN, "开头", 0);
    CHECK(!undo(), "nothing to undo yet");
    type("a");
    type("b");
    type(",");
    VALUE_IS("开头ab，", "keys");
    CHECK(undo(), "undone");
    VALUE_IS("开头", "keys in a row are one step");
    CHECK(!undo(), "and that was all");
    CHECK(redo(), "redone");
    VALUE_IS("开头ab，", "back");
    where();
    CHECK(g_cur == 5, "and the cursor (%d)", (int)g_cur);
    delay_ms(3200);
    type(".");
    CHECK(undo(), "after a while it is a step of its own");
    VALUE_IS("开头ab，", "only that");
    CHECK(redo(), "redone");
    cursor_to(2);
    type(".");
    CHECK(undo(), "moved in between: a step of its own");
    VALUE_IS("开头ab，。", "only that");
    where();
    CHECK(g_cur == 2, "the cursor comes back too (%d)", (int)g_cur);
    commit("新的");
    CHECK(!redo(), "something new: there is no going forward any more");
    CHECK(undo(), "undone");
    VALUE_IS("开头ab，。", "what text.commit put in goes as one");
    select_range(0, 2);
    on_ui(do_backspace);
    VALUE_IS("ab，。", "selected, deleted");
    CHECK(undo(), "undone");
    VALUE_IS("开头ab，。", "it comes back");
    where();
    CHECK(g_sel_a == 0 && g_sel_b == 2, "selected as it was (%d, %d)", (int)g_sel_a, (int)g_sel_b);
    /* a pause, or moving in between, ends a step */
    on_ui(do_blur);
    focus_as(INK_TEXT_PLAIN, "", 0);
    type("1");
    delay_ms(3200);
    type("2");
    CHECK(undo(), "undone");
    VALUE_IS("1", "a pause ended the step");
    on_ui(do_blur);
    focus_as(INK_TEXT_PLAIN, "", 0);
    type("1");
    cursor_to(0);
    type("2");
    CHECK(undo(), "undone");
    VALUE_IS("1", "moving ended the step");
    /* how far back */
    on_ui(do_blur);
    focus_as(INK_TEXT_PLAIN, "", 0);
    for (int i = 0; i < 20; i++) commit("x");
    int steps = 0;
    while (undo()) steps++;
    CHECK(steps == 16, "the last 16 (%d)", steps);
    VALUE_IS("xxxx", "the rest stays");
    on_ui(do_blur);
    /* gone with the session */
    focus_as(INK_TEXT_PLAIN, "第一个", 0);
    commit("的");
    on_ui(do_blur);
    focus_as(INK_TEXT_PLAIN, "", 0);
    CHECK(!undo(), "another field starts with nothing to undo");
    on_ui(do_blur);
    /* a password keeps nothing, and nothing comes out */
    focus_as(INK_TEXT_PASSWORD, "", 0);
    type("secret");
    CHECK(!undo(), "no history of a password");
    commit("more");
    on_ui(do_backspace);
    CHECK(!undo(), "however it changed");
    select_range(0, 3);
    where();
    CHECK(g_selected_n == -1, "what is selected does not come out (%d)", g_selected_n);
    on_ui(do_blur);
    /* the AI, the same */
    CHECK(AI("text.select", "{\"start\":0}") && g_rc.status == INK_RC_FAILED, "no field: nothing to select");
    focus_as(INK_TEXT_PLAIN, "今天天气很好", 0);
    CHECK(AI("text.state", NULL) && strstr(g_result, "\"cursor\":6") && strstr(g_result, "\"selection_start\":6") && strstr(g_result, "\"selection_end\":6") && !strstr(g_result, "can_undo"), "it sees where (%s)", g_result);
    CHECK(AI("text.select", "{\"start\":2,\"end\":4}") && g_rc.status == INK_RC_DONE, "it selects (%s)", g_rc.detail);
    CHECK(AI("text.insert", "{\"text\":\"阳光\"}") && g_rc.status == INK_RC_DONE, "and writes over it");
    VALUE_IS("今天阳光很好", "the same as a person");
    CHECK(AI("text.state", NULL) && strstr(g_result, "\"can_undo\":true"), "it sees it can be undone (%s)", g_result);
    CHECK(AI("text.undo", NULL) && g_rc.status == INK_RC_DONE, "and undoes (%s)", g_rc.detail);
    VALUE_IS("今天天气很好", "as a person would");
    CHECK(AI("text.redo", NULL) && g_rc.status == INK_RC_DONE, "and back");
    VALUE_IS("今天阳光很好", "as a person would");
    CHECK(AI("text.select", "{\"start\":1}") && g_rc.status == INK_RC_DONE, "no end: the cursor");
    where();
    CHECK(g_cur == 1 && g_sel_a == g_sel_b, "(%d, %d, %d)", (int)g_cur, (int)g_sel_a, (int)g_sel_b);
    on_ui(do_blur);
    g_cfg = (ink_text_cfg_t){.type = INK_TEXT_PLAIN, .ai_hidden = true, .initial = "mimi"};
    on_ui(do_focus);
    CHECK(AI("text.select", "{\"start\":0,\"end\":2}") && g_rc.status == INK_RC_REJECTED, "not in a field that keeps to itself");
    CHECK(AI("text.undo", NULL) && g_rc.status == INK_RC_REJECTED, "nor undo there");
    CHECK(AI("text.state", NULL) && !strstr(g_result, "selection_start"), "nor sees where (%s)", g_result);
    on_ui(do_blur);
    focus_as(INK_TEXT_PASSWORD, "", 0);
    CHECK(AI("text.select", "{\"start\":0}") && g_rc.status == INK_RC_REJECTED, "nor a password");
    on_ui(do_blur);
}

static void for_the_ai(void) {
    CHECK(AI("text.state", NULL) && strstr(g_result, "\"focused\":false"), "nothing has the focus (%s)", g_result);
    CHECK(AI("text.insert", "{\"text\":\"你好\"}") && g_rc.status == INK_RC_FAILED, "nowhere to write");
    CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_FAILED, "nothing to send");
    focus_as(INK_TEXT_PLAIN, NULL, 0);
    type("ni");
    CHECK(AI("text.state", NULL) && strstr(g_result, "\"focused\":true") && strstr(g_result, "\"composing\":\"ni\"") && strstr(g_result, "\"hint\":\"写点什么\""), "the AI sees what is going on (%s)", g_result);
    int entered = g_entered;
    CHECK(AI("text.insert", "{\"text\":\"帮你写的\"}") && g_rc.status == INK_RC_DONE, "the AI writes (%s)", g_rc.detail);
    look();
    CHECK(!strcmp(g_val, "ni帮你写的") && !g_comp[0], "as if typed (%s)", g_val);
    CHECK(AI("text.submit", NULL) && g_rc.status == INK_RC_DONE && g_entered == entered + 1 && !strcmp(g_entered_text, "ni帮你写的"), "and sends it (%s)", g_entered_text);
    on_ui(do_blur);
    /* a field that keeps to itself */
    g_cfg = (ink_text_cfg_t){.type = INK_TEXT_PLAIN, .ai_hidden = true};
    on_ui(do_focus);
    type("mimi");
    CHECK(AI("text.state", NULL) && strstr(g_result, "\"hidden\":true") && !strstr(g_result, "mimi"), "what a page keeps from the AI (%s)", g_result);
    on_ui(do_blur);
}

static void clipboard(void) {
    char out[64];
    bool sens = true;
    CHECK(ink_clip_get(out, sizeof out, INK_SRC_TOUCH, &sens) == 0 && !sens, "empty");
    CHECK(BY_HAND("clipboard.write", "{\"text\":\"一段话\"}") && g_rc.status == INK_RC_DONE, "written");
    CHECK(ink_clip_get(out, sizeof out, INK_SRC_TOUCH, NULL) > 0 && !strcmp(out, "一段话"), "the person reads it (%s)", out);
    CHECK(AI("clipboard.read", NULL) && g_rc.status == INK_RC_REJECTED, "the AI does not, by default (%s)", ink_rc_name(g_rc.status));
    CHECK(setting_by_hand("ai.read_clipboard", 1), "person confirms clipboard access");
    CHECK(AI("clipboard.read", NULL) && g_rc.status == INK_RC_DONE && strstr(g_result, "一段话"), "unless allowed (%s)", g_result);
    CHECK(AI("clipboard.write", "{\"text\":\"密码\",\"sensitive\":true}") && g_rc.status == INK_RC_DONE, "something sensitive");
    CHECK(AI("clipboard.read", NULL) && g_rc.status == INK_RC_REJECTED, "the AI never reads that (%s)", g_result);
    CHECK(ink_clip_get(out, sizeof out, INK_SRC_TOUCH, &sens) > 0 && sens, "the person can");
    delay_ms(59000);
    CHECK(ink_clip_get(out, sizeof out, INK_SRC_TOUCH, NULL) > 0, "for a while");
    delay_ms(2000);
    CHECK(ink_clip_get(out, sizeof out, INK_SRC_TOUCH, NULL) == 0, "a minute later it is gone");
    /* not sensitive: it stays, and a new text ends the countdown */
    ink_clip_set("敏感", true, INK_SRC_TOUCH);
    ink_clip_set("普通", false, INK_SRC_TOUCH);
    delay_ms(61000);
    CHECK(ink_clip_get(out, sizeof out, INK_SRC_TOUCH, NULL) > 0 && !strcmp(out, "普通"), "what replaced it stays (%s)", out);
    /* gone with the member */
    CHECK(ink_member_switch("kid") == 0, "someone else");
    CHECK(ink_clip_get(out, sizeof out, INK_SRC_TOUCH, NULL) == 0, "it is not theirs");
    ink_member_test_reset();
    CHECK(setting_by_hand("ai.read_clipboard", 0), "person confirms revoking clipboard access");
    char *huge = malloc(INK_CLIP_MAX + 2);
    memset(huge, 'x', INK_CLIP_MAX + 1);
    huge[INK_CLIP_MAX + 1] = 0;
    CHECK(ink_clip_set(huge, false, INK_SRC_TOUCH) != 0, "too long is refused");
    free(huge);
}

/* The keyboard, by finger. */
static int g_found_x, g_found_y;
static const char *g_find;
static bool g_found;
static void find_in(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o) && !g_found; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_label_class) && !strcmp(lv_label_get_text(c), g_find) && !lv_obj_has_flag(lv_obj_get_parent(c), LV_OBJ_FLAG_HIDDEN)) {
            lv_area_t a;
            lv_obj_update_layout(c);
            lv_obj_get_coords(lv_obj_get_parent(c), &a);
            g_found_x = (a.x1 + a.x2) / 2;
            g_found_y = (a.y1 + a.y2) / 2;
            g_found = true;
            return;
        }
        find_in(c);
    }
}
static void do_find(void) {
    g_found = false;
    find_in(lv_layer_sys());
}
static bool press(const char *label) {
    g_find = label;
    on_ui(do_find);
    if (!g_found) return false;
    tap(g_found_x, g_found_y);
    return true;
}

static lv_obj_t *g_field;
static volatile int g_field_entered;
static char g_field_text[128];
static void field_entered(void *ctx, const char *value) {
    (void)ctx;
    snprintf(g_field_text, sizeof g_field_text, "%s", value);
    g_field_entered++;
}
static void field_page(lv_obj_t *scr, void *arg) {
    (void)arg;
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEND, .hint = "搜索"};
    g_field = ink_textfield_create(scr, &cfg, field_entered, NULL, false);
    lv_obj_align(g_field, LV_ALIGN_TOP_MID, 0, ink_dp(40));
}
static const ink_activity_t FIELD_PAGE = {.name = "fieldpage", .create = field_page};
static void open_field_page(void) { ink_ui_push(&FIELD_PAGE, NULL); }
static void close_field_page(void) { ink_ui_pop(); }
static int g_fx, g_fy;
static void field_where(void) {
    lv_area_t a;
    lv_obj_update_layout(g_field);
    lv_obj_get_coords(g_field, &a);
    g_fx = (a.x1 + a.x2) / 2;
    g_fy = (a.y1 + a.y2) / 2;
}

static void do_focus_end(void) { ink_text_set_cursor(9999); }

static void keyboard(void) {
    on_ui(open_field_page);
    delay_ms(1500);
    on_ui(field_where);
    CHECK(!press("q"), "no keyboard before anyone needs one");
    tap(g_fx, g_fy);
    delay_ms(300);
    CHECK(ink_text_focused(), "a tap on the field starts typing");
    CHECK(press("h"), "the keyboard is up");
    CHECK(press("a") && press("o"), "its keys work");
    look();
    CHECK(!strcmp(g_comp, "hao"), "as composed (%s)", g_comp);
    delay_ms(300);
    CHECK(sim_metric_value("text.key_to_visible_ms") > 0, "keys are measured to the panel");
    CHECK(press("好"), "the candidate is on the panel");
    look();
    CHECK(!strcmp(g_val, "好") && !g_comp[0], "and picks (%s)", g_val);
    delay_ms(600);
    double key_ms = sim_metric_value("text.key_to_visible_ms") > 0 ? sim_metric_last("text.key_to_visible_ms") : 0;
    CHECK(key_ms > 0 && key_ms < 800, "a key is seen quickly (%.0f ms)", key_ms);
    /* a burst is not lost, however busy the panel */
    const char *burst = "zhongguo";
    g_find = "z";
    on_ui(do_find);
    for (const char *p = burst; *p; p++) {
        char s[2] = {*p, 0};
        g_find = s;
        on_ui(do_find);
        hw(HW_PRESS, g_found_x, g_found_y);
        delay_ms(20);
        hw(HW_RELEASE, g_found_x, g_found_y);
        delay_ms(30);
    }
    delay_ms(2000);
    look();
    CHECK(!strcmp(g_comp, "zhongguo"), "eight quick presses, eight letters (%s)", g_comp);
    CHECK(press("中国"), "and its word");
    delay_ms(300);
    CHECK(press("删除"), "delete");
    look();
    CHECK(!strcmp(g_val, "好中"), "(%s)", g_val);
    /* symbols, and back */
    CHECK(press("符号") && press("1") && press("字母") && press("q"), "another page of keys");
    look();
    CHECK(!strcmp(g_val, "好中1") && !strcmp(g_comp, "q"), "(%s, %s)", g_val, g_comp);
    CHECK(press("删除") && press("发送"), "the enter key says what it does");
    CHECK(WAIT_FOR(g_field_entered == 1, 500) && !strcmp(g_field_text, "好中1"), "and the page hears (%s)", g_field_text);
    /* pressed long: the last thing there is selected (nothing is under the finger), copy, paste */
    hw(HW_PRESS, g_fx, g_fy);
    delay_ms(900);
    hw(HW_RELEASE, g_fx, g_fy);
    delay_ms(300);
    CHECK(press("复制"), "pressed long: a menu");
    char clip[32] = "";
    ink_clip_get(clip, sizeof clip, INK_SRC_TOUCH, NULL);
    CHECK(!strcmp(clip, "1"), "what is selected is copied (%s)", clip);
    on_ui(do_focus_end);
    hw(HW_PRESS, g_fx, g_fy);
    delay_ms(900);
    hw(HW_RELEASE, g_fx, g_fy);
    delay_ms(300);
    CHECK(press("粘贴"), "again");
    look();
    CHECK(!strcmp(g_val, "好中1"), "pasted over what is selected (%s)", g_val);
    /* leaving the page ends it, and the keyboard goes */
    on_ui(close_field_page);
    delay_ms(400);
    CHECK(!ink_text_focused() && !press("q"), "the page goes, the session and the keyboard with it");
}

static void *g_hog[128];
static int g_nhog;

static void memory_short(void) {
    focus_as(INK_TEXT_PLAIN, NULL, 0);
    on_ui(do_blur);
    delay_ms(300);
    double was = sim_metric_value("text.dict_reclaimed");
    while (g_nhog < 128 && ink_mem_check() < INK_PRESSURE_CRITICAL) {
        void *p = osal_malloc(128 * 1024, OSAL_MEM_EXTERNAL);
        if (!p) break;
        memset(p, 1, 128 * 1024);
        g_hog[g_nhog++] = p;
    }
    CHECK(WAIT_FOR(sim_metric_value("text.dict_reclaimed") > was, 500), "memory very short: the dictionary goes (%s)", ink_pressure_name(ink_mem_pressure()));
    while (g_nhog) osal_free(g_hog[--g_nhog]);
    ink_mem_check();
    double loads = sim_metric_value("text.dict_loaded");
    focus_as(INK_TEXT_PLAIN, NULL, 0);
    type("de");
    look();
    CHECK(g_nc >= 1, "something still answers meanwhile (%d)", g_nc);
    CHECK(WAIT_FOR(sim_metric_value("text.dict_loaded") > loads, 3000), "and it comes back");
    look();
    CHECK(cand_is(0, "的", 2) && cand_is(1, "得", 2), "with all it knew (%d, %s)", g_nc, g_c[0].word);
    on_ui(do_blur);
}

static void without_dictionary(void) {
    focus_as(INK_TEXT_PLAIN, NULL, 0);
    delay_ms(2000);
    CHECK(sim_metric_value("text.dict_loaded") == 0, "no dictionary came in");
    type("shi");
    look();
    CHECK(cand_is(0, "是", 3) && g_nc == 1, "the small table has the commonest (%d, %s)", g_nc, g_nc ? g_c[0].word : "");
    pick(0);
    look();
    CHECK(!strcmp(g_val, "是"), "and types (%s)", g_val);
    type("xyz");
    on_ui(do_enter);
    look();
    CHECK(!strcmp(g_val, "是xyz"), "what it does not know goes in as typed (%s)", g_val);
    on_ui(do_blur);
}

static void write_card_file(const void *data, size_t n) {
    hal_file_t *f = hal_fs_open("/content/.system/ime/base.ikd", HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    CHECK(f != NULL, "written on the card");
    if (f) {
        hal_fs_write(f, data, n);
        hal_fs_close(f);
    }
}

static void bad_dictionaries(void) {
    /* a byte gone wrong in a good file */
    static uint8_t good[8192];
    hal_file_t *in = hal_fs_open("/content/.system/ime/base.ikd", HAL_O_READ);
    ssize_t got = in ? hal_fs_read(in, good, sizeof good) : -1;
    if (in) hal_fs_close(in);
    CHECK(got > 100, "the good file (%d)", (int)got);
    good[got - 1] ^= 0x40;
    write_card_file(good, (size_t)got);
    focus_as(INK_TEXT_PLAIN, NULL, 0);
    CHECK(WAIT_FOR(sim_metric_value("text.dict_bad") == 1, 2000), "the checksum sees it (%.0f)", sim_metric_value("text.dict_bad"));
    on_ui(do_blur);
    delay_ms(100);
    /* a checksum that is right, an offset that is not */
    static struct __attribute__((packed)) {
        char magic[4];
        uint32_t nkeys, nentries, keys_off, entries_off, text_off, text_len, crc;
        struct __attribute__((packed)) {
            uint32_t text_off;
            uint16_t key_len, count;
            uint32_t first;
        } key;
        struct __attribute__((packed)) {
            uint32_t text_off;
            uint16_t word_len, freq;
        } entry;
        char text[4];
    } bad = {.magic = "IKD1", .nkeys = 1, .nentries = 1, .keys_off = 32, .entries_off = 44, .text_off = 52, .text_len = 4, .key = {0, 2, 1, 0}, .entry = {0x7fffffff, 2000, 1}, .text = "dede"};
    bad.crc = ink_crc32(0, &bad.key, sizeof bad - 32);
    write_card_file(&bad, sizeof bad);
    focus_as(INK_TEXT_PLAIN, NULL, 0);
    CHECK(WAIT_FOR(sim_metric_value("text.dict_bad") == 2, 2000), "an offset out of the file is seen (%.0f)", sim_metric_value("text.dict_bad"));
    type("de");
    look();
    CHECK(cand_is(0, "的", 2) && g_nc == 1, "the small table instead");
    on_ui(do_blur);
    /* plain rubbish, and half a file */
    char rubbish[64];
    memset(rubbish, 0x5a, sizeof rubbish);
    write_card_file(rubbish, sizeof rubbish);
    delay_ms(100);
    focus_as(INK_TEXT_PLAIN, NULL, 0);
    CHECK(WAIT_FOR(sim_metric_value("text.dict_bad") == 3, 2000), "rubbish is seen");
    on_ui(do_blur);
    write_card_file(&bad, 40);
    delay_ms(100);
    focus_as(INK_TEXT_PLAIN, NULL, 0);
    CHECK(WAIT_FOR(sim_metric_value("text.dict_bad") == 4, 2000), "half a file is seen");
    type("shi");
    look();
    CHECK(cand_is(0, "是", 3), "and the device goes on");
    on_ui(do_blur);
}

/* On the page: a tap puts the cursor, a long press selects a word, the arrows adjust (placeholder look). */
static char g_label_text[256];
static void find_field_label(void) {
    lv_obj_t *l = lv_obj_get_child(g_field, 0);
    snprintf(g_label_text, sizeof g_label_text, "%s", lv_label_get_text(l));
}
static int g_left_x, g_left_y;
static int g_letter;
static void letter_where(void) {
    lv_area_t a;
    lv_point_t p;
    lv_obj_t *l = lv_obj_get_child(g_field, 0);
    lv_obj_update_layout(g_field);
    lv_obj_get_coords(l, &a);
    lv_label_get_letter_pos(l, (uint32_t)g_letter, &p);
    g_left_x = a.x1 + p.x + 2;
    g_left_y = a.y1 + p.y + 4;
}
static void field_left(void) {
    lv_area_t a;
    lv_obj_t *l = lv_obj_get_child(g_field, 0);
    lv_obj_update_layout(g_field);
    lv_obj_get_coords(l, &a);
    g_left_x = a.x1 + 6;
    g_left_y = (a.y1 + a.y2) / 2;
}

static void field_editing(void) {
    on_ui(open_field_page);
    delay_ms(1500);
    on_ui(field_where);
    tap(g_fx, g_fy);
    delay_ms(300);
    CHECK(ink_text_focused(), "typing");
    commit("hello world");
    delay_ms(300);
    on_ui(find_field_label);
    CHECK(!strcmp(g_label_text, "hello world"), "the cursor at the end is not marked (%s)", g_label_text);
    on_ui(field_left);
    tap(g_left_x, g_left_y);
    delay_ms(300);
    where();
    CHECK(g_cur == 0, "a tap at the start puts the cursor there (%d)", (int)g_cur);
    on_ui(find_field_label);
    CHECK(!strcmp(g_label_text, "|hello world"), "where it is shows (%s)", g_label_text);
    commit("a");
    where();
    CHECK(!strcmp(g_val, "ahello world"), "and what comes goes there (%s)", g_val);
    /* a long press selects the word */
    on_ui(field_left);
    hw(HW_PRESS, g_left_x + 30, g_left_y);
    delay_ms(1000);
    hw(HW_RELEASE, g_left_x + 30, g_left_y);
    delay_ms(300);
    where();
    CHECK(!strcmp(g_selected, "ahello"), "a long press selects the word (%s)", g_selected);
    on_ui(find_field_label);
    CHECK(strstr(g_label_text, "[ahello]"), "and it shows (%s)", g_label_text);
    CHECK(press("◀"), "the arrows are there");
    delay_ms(300);
    where();
    CHECK(!strcmp(g_selected, "ahell"), "the end moves back (%s)", g_selected);
    CHECK(press("▶"), "and on");
    delay_ms(300);
    where();
    CHECK(!strcmp(g_selected, "ahello"), "(%s)", g_selected);
    CHECK(press("复制"), "copy");
    delay_ms(300);
    char got[32] = "";
    bool sensitive = false;
    CHECK(ink_clip_get(got, sizeof got, INK_SRC_TOUCH, &sensitive) > 0 && !strcmp(got, "ahello"), "what is selected goes on the clipboard (%s)", got);
    on_ui(do_backspace);
    where();
    CHECK(!strcmp(g_val, " world"), "the selection goes (%s)", g_val);
    on_ui(field_left);
    hw(HW_PRESS, g_left_x + 30, g_left_y);
    delay_ms(1000);
    hw(HW_RELEASE, g_left_x + 30, g_left_y);
    delay_ms(300);
    CHECK(press("撤销"), "undo from the menu");
    delay_ms(300);
    where();
    CHECK(!strcmp(g_val, "ahello world"), "it comes back (%s)", g_val);
    /* the mark is not a letter of the text */
    cursor_to(2);
    delay_ms(300);
    g_letter = 3; /* "ah|ello": the e */
    on_ui(letter_where);
    tap(g_left_x, g_left_y);
    delay_ms(300);
    where();
    CHECK(g_cur == 2, "a tap on the letter after the mark means that letter (%d)", (int)g_cur);
    /* a long text starts so that the cursor is in view */
    cursor_to(999);
    commit("一二三四五六七八九十一二三四五六七八九十一二三四五六七八九十");
    delay_ms(300);
    on_ui(find_field_label);
    CHECK(!strncmp(g_label_text, "一二三", strlen("一二三")) && g_label_text[0] && !strstr(g_label_text, "hello"), "the end of it shows (%s)", g_label_text);
    on_ui(field_left);
    tap(g_left_x, g_left_y);
    delay_ms(300);
    where();
    CHECK(g_cur == 12, "a tap on the first letter shown means that one (%d)", (int)g_cur);
    on_ui(close_field_page);
    delay_ms(500);
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    const char *phase = getenv("INK_TEXT_PHASE");
    if (phase && !strcmp(phase, "nocard")) without_dictionary();
    else if (phase && !strcmp(phase, "baddict")) bad_dictionaries();
    else {
        engine();
        types();
        for_the_ai();
        editing();
        clipboard();
        keyboard();
        field_editing();
        memory_short();
    }
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "texttest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
