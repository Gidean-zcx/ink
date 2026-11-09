/*
 * The app directory and lifecycle (system services design §8), on the whole product, with apps of the test's own: what
 * they are told and when, what they keep, memory pressure, another member, permissions and service cards. Results are
 * the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/sys/appreg.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/storage.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"
#include "ink/ui_shell.h"

/* What the apps were told, in order. */
static char g_log[2048];
static void log_word(const char *w) {
    size_t n = strlen(g_log);
    snprintf(g_log + n, sizeof g_log - n, "%s ", w);
}
#define SEEN(w) (strstr(g_log, w) != NULL)
#define LOG_CLEAR() (g_log[0] = 0)

#define OPS(NAME)                                                                       \
    static char NAME##_kept[64];                                                        \
    static char NAME##_memory[64];                                                      \
    static void *NAME##_block;                                                          \
    static size_t NAME##_block_size;                                                    \
    static void NAME##_start(void) {                                                    \
        log_word(#NAME ":start");                                                       \
        NAME##_memory[0] = 0;                                                           \
        if (NAME##_block_size) NAME##_block = malloc_external(NAME##_block_size);       \
    }                                                                                   \
    static void NAME##_fg(void) { log_word(#NAME ":fg"); }                              \
    static void NAME##_bg(void) { log_word(#NAME ":bg"); }                              \
    static void NAME##_freeze(void) { log_word(#NAME ":freeze"); }                      \
    static void NAME##_resume(void) { log_word(#NAME ":resume"); }                      \
    static void NAME##_stop(void) {                                                     \
        log_word(#NAME ":stop");                                                        \
        osal_free(NAME##_block);                                                        \
        NAME##_block = NULL;                                                            \
    }                                                                                   \
    static int NAME##_save(void *buf, size_t cap) {                                     \
        log_word(#NAME ":save");                                                        \
        return snprintf(buf, cap, "%s", NAME##_kept);                                   \
    }                                                                                   \
    static void NAME##_restore(const void *buf, size_t len) {                           \
        log_word(#NAME ":restore");                                                     \
        snprintf(NAME##_memory, sizeof NAME##_memory, "%.*s", (int)len, (const char *)buf); \
    }                                                                                   \
    static const ink_app_ops_t NAME##_OPS = {NAME##_start, NAME##_fg, NAME##_bg, NAME##_freeze, NAME##_resume, NAME##_stop, NAME##_save, NAME##_restore};

static void *malloc_external(size_t n) {
    void *p = osal_malloc(n, OSAL_MEM_EXTERNAL);
    if (p) memset(p, 1, n);
    return p;
}

OPS(alpha)
OPS(beta)

static void empty_page(lv_obj_t *scr, void *arg) {
    (void)scr;
    (void)arg;
}
static void alpha_page(lv_obj_t *scr, void *arg) {
    (void)scr;
    (void)arg;
    log_word("alpha:page");
}
static const ink_activity_t ALPHA_PAGE = {.name = "alpha_page", .app = "alpha", .create = alpha_page};
static const ink_activity_t ALPHA_PAGE2 = {.name = "alpha_page2", .app = "alpha", .create = empty_page};
static const ink_activity_t BETA_PAGE = {.name = "beta_page", .app = "beta", .create = empty_page};
static const ink_activity_t OTHER_PAGE = {.name = "gamma_page", .app = "gamma", .create = empty_page};
static const ink_activity_t ADULT_PAGE = {.name = "delta_page", .app = "delta", .create = empty_page};
static const ink_activity_t NOBODY_PAGE = {.name = "nobody", .create = empty_page};

static int g_card_value = 7;
static int beta_card(int index, ink_card_t *out) {
    if (index > 1) return 0;
    snprintf(out->id, sizeof out->id, "c%d", index);
    snprintf(out->title, sizeof out->title, "卡片 %d", index);
    snprintf(out->text, sizeof out->text, "现在是 %d", g_card_value);
    snprintf(out->action, sizeof out->action, "app.list");
    out->value = g_card_value;
    out->has_value = index == 0;
    return 1;
}

static const char *const NEEDS_NOTHING_THERE[] = {"nothing.here", NULL};
static const char *const BETA_PERMS[] = {"can.do", NULL};
static const ink_app_def_t ALPHA = {.id = "alpha", .title = "甲", .entry = &ALPHA_PAGE, .child_ok = true, .ops = &alpha_OPS};
static const ink_app_def_t BETA = {.id = "beta", .title = "乙", .entry = &BETA_PAGE, .child_ok = true, .background = INK_BG_PLAYBACK | INK_BG_SYNC, .permissions = BETA_PERMS, .ops = &beta_OPS, .card = beta_card};
static const ink_app_def_t GAMMA = {.id = "gamma", .title = "丙", .entry = &OTHER_PAGE, .needs = NEEDS_NOTHING_THERE};
static const ink_app_def_t DELTA = {.id = "delta", .title = "丁", .entry = &ADULT_PAGE, .child_ok = false};

/* What needs a permission. */
static volatile int g_perm_runs;
static void perm_run(ink_call_t *c) {
    (void)c;
    g_perm_runs++;
}
static const ink_cap_t TEST_CAPS[] = {
    {.id = "test.perm", .title = "要权限的", .permission = "can.do", .effect = INK_FX_NAVIGATE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = perm_run},
};

static volatile int g_state_events;
static char g_last_state_id[16], g_last_state[16];
static void on_app_state(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    (void)id;
    ink_args_get_str(a, "id", g_last_state_id, sizeof g_last_state_id);
    ink_args_get_str(a, "state", g_last_state, sizeof g_last_state);
    g_state_events++;
}

static const char *fg_state(void) {
    static char s[16];
    s[0] = 0;
    ink_state_get_str(ink_state_find("app.foreground"), s, sizeof s);
    return s;
}

static char g_on_top[32];
static void read_top(void) { snprintf(g_on_top, sizeof g_on_top, "%s", ink_ui_top_name()); }
static const char *top(void) {
    on_ui(read_top);
    return g_on_top;
}

static void push_alpha2(void) { ink_ui_push(&ALPHA_PAGE2, NULL); }
static void push_nobody(void) { ink_ui_push(&NOBODY_PAGE, NULL); }
static void go_home(void) { ink_ui_home(); }
static void pop_top(void) { ink_ui_pop(); }

static void directory(void) {
    CHECK(ink_app_count() == 10, "home, reader, phone, clock, AI, SMS and the test's (%d)", ink_app_count());
    CHECK(!strcmp(fg_state(), "home") && ink_app_state_of("home") == INK_APP_FOREGROUND, "home is in the foreground (%s)", fg_state());
    CHECK(ink_app_state_of("reader") == INK_APP_STOPPED && ink_app_state_of("clock") == INK_APP_STOPPED && ink_app_state_of("alpha") == INK_APP_STOPPED, "the others have not started");
    CHECK(AI("app.list", NULL) && strstr(g_result, "\"id\":\"alpha\"") && strstr(g_result, "\"title\":\"甲\"") && strstr(g_result, "\"state\":\"stopped\"") && strstr(g_result, "\"id\":\"reader\"") && strstr(g_result, "\"system\":true"),
          "the AI reads the directory (%s)", g_result);
    CHECK(!strstr(g_result, "gamma"), "what this device lacks for is not in it (%s)", g_result);
    CHECK(strstr(g_result, "delta") != NULL, "what is for adults is, for an adult");
    CHECK(AI("app.open", "{\"id\":\"gamma\"}") && g_rc.status == INK_RC_REJECTED, "it cannot be opened (%s)", ink_rc_name(g_rc.status));
    CHECK(AI("app.open", "{\"id\":\"nope\"}") && g_rc.status == INK_RC_INVALID, "no such app");
    CHECK(AI("app.close", "{\"id\":\"home\"}") && g_rc.status == INK_RC_INVALID, "home cannot be closed");
    static const ink_app_def_t BAD = {.id = "../x", .title = "x"};
    CHECK(ink_app_register(&ALPHA) != 0 && ink_app_register(&BAD) != 0, "an id once, and one that can name a file");
    static const ink_db_def_t MINE = {.name = "alpha.notes", .scope = INK_DB_MEMBER, .schema = 1, .json = true};
    static const ink_db_def_t THEIRS = {.name = "beta.notes", .scope = INK_DB_MEMBER, .schema = 1, .json = true};
    ink_db_register(&MINE);
    ink_db_t *db = ink_app_db_open("alpha", &MINE);
    CHECK(db != NULL, "an app's own collection");
    ink_db_close(db);
    CHECK(ink_app_db_open("alpha", &THEIRS) == NULL, "not another's");
}

static void lifecycle(void) {
    LOG_CLEAR();
    int events = g_state_events;
    snprintf(alpha_kept, sizeof alpha_kept, "第一次");
    CHECK(AI("app.open", "{\"id\":\"alpha\"}") && g_rc.status == INK_RC_DONE, "the AI opens it (%s)", g_rc.detail);
    CHECK(!strcmp(top(), "alpha_page") && !strcmp(fg_state(), "alpha"), "its page is on top (%s, %s)", g_on_top, fg_state());
    CHECK(SEEN("alpha:start alpha:fg alpha:page") && !SEEN("alpha:restore"), "it was started, then its page built, nothing to restore (%s)", g_log);
    CHECK(ink_app_state_of("home") == INK_APP_BACKGROUND, "home went behind");
    CHECK(g_state_events >= events + 2 && !strcmp(g_last_state_id, "alpha") && !strcmp(g_last_state, "foreground"), "told as events (%s %s)", g_last_state_id, g_last_state);

    /* more pages of one app: nothing changes for it */
    LOG_CLEAR();
    on_ui(push_alpha2);
    CHECK(!strcmp(top(), "alpha_page2") && !g_log[0], "its next page is not a change (%s)", g_log);
    on_ui(pop_top);
    delay_ms(100);
    CHECK(!strcmp(g_log, "alpha:page "), "nor closing it (%s)", g_log);

    /* covered by another */
    LOG_CLEAR();
    CHECK(BY_HAND("app.open", "{\"id\":\"beta\"}") && g_rc.status == INK_RC_DONE && !strcmp(fg_state(), "beta"), "another on top (%s)", fg_state());
    CHECK(SEEN("alpha:bg beta:start beta:fg"), "the first goes behind, then the next comes (%s)", g_log);
    CHECK(ink_app_state_of("alpha") == INK_APP_BACKGROUND, "in the background");
    delay_ms(9000);
    CHECK(ink_app_state_of("alpha") == INK_APP_BACKGROUND && !SEEN("alpha:freeze"), "for a while");
    delay_ms(1500);
    CHECK(ink_app_state_of("alpha") == INK_APP_FROZEN && SEEN("alpha:save alpha:freeze"), "ten seconds later it is frozen, having saved (%s)", g_log);
    CHECK(ink_app_state_of("home") == INK_APP_FROZEN, "home too");

    /* back to it: its page comes up, the one above closes */
    LOG_CLEAR();
    CHECK(AI("app.open", "{\"id\":\"alpha\"}") && !strcmp(top(), "alpha_page"), "back (%s)", g_on_top);
    CHECK(SEEN("beta:bg alpha:resume alpha:fg") && !SEEN("alpha:start"), "resumed, not started again (%s)", g_log);

    /* background work: not frozen while it lasts, and only what it declared */
    CHECK(ink_app_background("beta", INK_BG_DOWNLOAD, true) != 0, "not declared");
    CHECK(ink_app_background("beta", INK_BG_PLAYBACK, true) == 0, "declared");
    delay_ms(12000);
    CHECK(ink_app_state_of("beta") == INK_APP_BACKGROUND, "playing: it stays in the background (%s)", ink_app_state_name(ink_app_state_of("beta")));
    CHECK(ink_app_background("beta", INK_BG_PLAYBACK, false) == 0, "finished");
    delay_ms(9000);
    CHECK(ink_app_state_of("beta") == INK_APP_BACKGROUND, "it gets the same while as anyone");
    delay_ms(1500);
    CHECK(ink_app_state_of("beta") == INK_APP_FROZEN, "then it is frozen");

    /* a lock is going behind */
    LOG_CLEAR();
    ink_power_lock();
    CHECK(WAIT_FOR(ink_app_state_of("alpha") == INK_APP_BACKGROUND, 2000) && SEEN("alpha:bg") && !strcmp(fg_state(), ""), "locked: nothing is in the foreground (%s, %s)", g_log, fg_state());
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(ink_app_state_of("alpha") == INK_APP_FOREGROUND, 2000) && SEEN("alpha:fg") && !strcmp(fg_state(), "alpha"), "unlocked: it comes back (%s)", g_log);

    /* a page of nobody's on top: nobody is in the foreground */
    on_ui(push_nobody);
    CHECK(!strcmp(fg_state(), "") && ink_app_state_of("alpha") == INK_APP_BACKGROUND, "a system page (%s)", fg_state());
    on_ui(pop_top);
    CHECK(WAIT_FOR(!strcmp(fg_state(), "alpha"), 500), "and back (%s)", fg_state());

    /* closed: what it keeps comes back next time */
    LOG_CLEAR();
    snprintf(alpha_kept, sizeof alpha_kept, "读到第 3 页");
    CHECK(AI("app.close", "{\"id\":\"alpha\"}") && g_rc.status == INK_RC_DONE && !strcmp(top(), "home"), "closed (%s)", g_on_top);
    CHECK(ink_app_state_of("alpha") == INK_APP_STOPPED && SEEN("alpha:bg alpha:save alpha:stop"), "behind, saved, stopped (%s)", g_log);
    LOG_CLEAR();
    alpha_memory[0] = 0;
    CHECK(AI("app.open", "{\"id\":\"alpha\"}") && SEEN("alpha:start alpha:restore alpha:fg alpha:page"), "opened again (%s)", g_log);
    CHECK(!strcmp(alpha_memory, "读到第 3 页"), "with what it kept (%s)", alpha_memory);
    on_ui(go_home);
    delay_ms(200);
    CHECK(!strcmp(fg_state(), "home") && ink_app_state_of("alpha") == INK_APP_BACKGROUND, "home (%s)", fg_state());
}

/* What it keeps cannot be handed to the flash (audit AUD15-010): the only copy is in the app, it is not let go of. */
static int fill_storage_queue(void) {
    int n = 0;
    ink_storage_hold_writes(true);
    for (; n < 32; n++) {
        char path[48];
        snprintf(path, sizeof path, "/user/sys/test-queue-%d", n);
        if (ink_storage_write_atomic(path, "x", 1) != 0) break;
    }
    return n;
}

static void empty_storage_queue(int n) {
    ink_storage_hold_writes(false);
    CHECK(WAIT_FOR(ink_storage_pending() == 0, 3000), "it drains");
    for (int i = 0; i < n; i++) {
        char path[48];
        snprintf(path, sizeof path, "/user/sys/test-queue-%d", i);
        ink_storage_remove(path, NULL, NULL);
    }
    CHECK(WAIT_FOR(ink_storage_pending() == 0, 3000), "and is clean");
}

static void saving_fails(void) {
    snprintf(alpha_kept, sizeof alpha_kept, "旧的");
    CHECK(AI("app.open", "{\"id\":\"alpha\"}") && !strcmp(top(), "alpha_page"), "open (%s)", g_on_top);
    CHECK(AI("app.close", "{\"id\":\"alpha\"}") && g_rc.status == INK_RC_DONE, "closed, kept");
    CHECK(WAIT_FOR(ink_storage_pending() == 0, 3000), "on the flash");
    CHECK(AI("app.open", "{\"id\":\"alpha\"}") && !strcmp(alpha_memory, "旧的"), "comes back (%s)", alpha_memory);

    /* closing */
    snprintf(alpha_kept, sizeof alpha_kept, "新的");
    double not_saved = sim_metric_value("app.state_not_saved");
    int filled = fill_storage_queue();
    CHECK(filled > 0, "the queue to the flash is full (%d)", filled);
    CHECK(AI("app.close", "{\"id\":\"alpha\"}") && g_rc.status == INK_RC_FAILED, "closing says it could not (%s, %s)", ink_rc_name(g_rc.status), g_rc.detail);
    CHECK(ink_app_state_of("alpha") == INK_APP_BACKGROUND, "and it stays (%s)", ink_app_state_name(ink_app_state_of("alpha")));
    CHECK(sim_metric_value("app.state_not_saved") == not_saved + 1, "counted");

    /* freezing */
    delay_ms(11000);
    CHECK(ink_app_state_of("alpha") == INK_APP_BACKGROUND, "nor is it frozen (%s)", ink_app_state_name(ink_app_state_of("alpha")));
    empty_storage_queue(filled);
    delay_ms(11000);
    CHECK(ink_app_state_of("alpha") == INK_APP_FROZEN, "tried again after the same while (%s)", ink_app_state_name(ink_app_state_of("alpha")));

    CHECK(AI("app.close", "{\"id\":\"alpha\"}") && g_rc.status == INK_RC_DONE && ink_app_state_of("alpha") == INK_APP_STOPPED, "closed in the end (%s)", g_rc.detail);
    alpha_memory[0] = 0;
    CHECK(AI("app.open", "{\"id\":\"alpha\"}") && !strcmp(alpha_memory, "新的"), "with what it was keeping (%s)", alpha_memory);
    on_ui(go_home);
    delay_ms(200);
}

/* Memory taken from outside until the level is reached. */
static void *g_hog[128];
static int g_nhog;
static bool hog_until(ink_pressure_t level) {
    while (g_nhog < 128 && ink_mem_check() < level) {
        void *p = malloc_external(128 * 1024);
        if (!p) break;
        g_hog[g_nhog++] = p;
    }
    return ink_mem_check() >= level;
}
static void hog_free(void) {
    while (g_nhog) osal_free(g_hog[--g_nhog]);
    ink_mem_check();
}

static void pressure(void) {
    /* alpha is in the background, beta plays */
    CHECK(AI("app.open", "{\"id\":\"beta\"}"), "beta");
    CHECK(ink_app_background("beta", INK_BG_PLAYBACK, true) == 0, "playing");
    on_ui(go_home);
    delay_ms(100);
    CHECK(AI("app.open", "{\"id\":\"alpha\"}") && !strcmp(fg_state(), "alpha"), "alpha");
    on_ui(go_home);
    delay_ms(100);
    CHECK(ink_app_state_of("alpha") == INK_APP_BACKGROUND && ink_app_state_of("beta") == INK_APP_BACKGROUND, "both behind (%s, %s)", ink_app_state_name(ink_app_state_of("alpha")), ink_app_state_name(ink_app_state_of("beta")));
    LOG_CLEAR();
    CHECK(hog_until(INK_PRESSURE_LOW), "memory gets short (%s)", ink_pressure_name(ink_mem_pressure()));
    CHECK(WAIT_FOR(ink_app_state_of("alpha") == INK_APP_FROZEN, 1000), "what is idle is frozen at once (%s)", ink_app_state_name(ink_app_state_of("alpha")));
    CHECK(ink_app_state_of("beta") == INK_APP_BACKGROUND && !SEEN("beta:freeze"), "what plays goes on");
    CHECK(ink_app_state_of("home") == INK_APP_FOREGROUND, "the foreground is not touched");
    hog_free();
    delay_ms(1000);
    CHECK(ink_mem_pressure() < INK_PRESSURE_LOW, "memory is back");
    /* the next time: critical. beta plays and was left earlier, alpha does not: alpha goes first. */
    LOG_CLEAR();
    CHECK(hog_until(INK_PRESSURE_CRITICAL), "memory gets very short (%s)", ink_pressure_name(ink_mem_pressure()));
    CHECK(WAIT_FOR(ink_app_state_of("alpha") == INK_APP_STOPPED, 1000), "stopped (%s)", g_log);
    CHECK(WAIT_FOR(ink_app_state_of("beta") == INK_APP_STOPPED, 1000) && strstr(g_log, "alpha:stop") < strstr(g_log, "beta:stop"), "the player last (%s)", g_log);
    CHECK(SEEN("beta:save beta:stop") && strstr(g_log, "alpha:save") == NULL, "what was frozen had saved already, what was not saves now (%s)", g_log);
    CHECK(ink_app_state_of("home") == INK_APP_FOREGROUND, "home is not stopped");
    hog_free();
    delay_ms(500);
}

static void another_member(void) {
    snprintf(alpha_kept, sizeof alpha_kept, "大人的");
    CHECK(AI("app.open", "{\"id\":\"alpha\"}") && !strcmp(fg_state(), "alpha"), "the adult's app");
    CHECK(BY_HAND("app.open", "{\"id\":\"beta\"}"), "and another");
    LOG_CLEAR();
    CHECK(ink_member_switch("kid") == 0, "someone else");
    CHECK(WAIT_FOR(!strcmp(top(), "home"), 1000) && !strcmp(fg_state(), "home"), "home (%s, %s)", g_on_top, fg_state());
    CHECK(ink_app_state_of("alpha") == INK_APP_STOPPED && ink_app_state_of("beta") == INK_APP_STOPPED, "everything stopped");
    CHECK(SEEN("alpha:save alpha:stop") || SEEN("alpha:stop"), "saved and told (%s)", g_log);

    CHECK(AI("app.list", NULL) && !strstr(g_result, "delta") && strstr(g_result, "alpha"), "what is not for children is not listed (%s)", g_result);
    CHECK(BY_HAND("app.open", "{\"id\":\"delta\"}") && g_rc.status == INK_RC_REJECTED, "nor opened, even by hand (%s)", ink_rc_name(g_rc.status));
    alpha_memory[0] = 0;
    LOG_CLEAR();
    snprintf(alpha_kept, sizeof alpha_kept, "孩子的");
    CHECK(BY_HAND("app.open", "{\"id\":\"alpha\"}") && g_rc.status == INK_RC_DONE, "the same app for them (%s, %s)", ink_rc_name(g_rc.status), g_rc.detail);
    CHECK(SEEN("alpha:start") && !SEEN("alpha:restore") && !alpha_memory[0], "does not see what the adult left (%s, %s)", g_log, alpha_memory);
    CHECK(BY_HAND("app.close", "{\"id\":\"alpha\"}") && g_rc.status == INK_RC_DONE, "closed");

    ink_member_test_reset();
    CHECK(WAIT_FOR(ink_app_state_of("alpha") == INK_APP_STOPPED && !strcmp(top(), "home"), 1000), "back to the adult");
    delay_ms(500);
    LOG_CLEAR();
    alpha_memory[0] = 0;
    CHECK(AI("app.open", "{\"id\":\"alpha\"}") && !strcmp(alpha_memory, "大人的"), "whose it was is what comes back (%s)", alpha_memory);
    CHECK(AI("app.close", "{\"id\":\"alpha\"}"), "closed");
}

static void permissions(void) {
    /* an adult's alpha has not declared it */
    CHECK(AI("app.open", "{\"id\":\"alpha\"}"), "alpha in front");
    CHECK(call_as("test.perm", NULL, INK_SRC_APP) && g_rc.status == INK_RC_REJECTED && g_perm_runs == 0, "it may not use what it did not declare (%s, %s)", ink_rc_name(g_rc.status), g_rc.detail);
    CHECK(BY_HAND("test.perm", NULL) && g_rc.status == INK_RC_DONE && g_perm_runs == 1, "the person may");
    CHECK(AI("test.perm", NULL) && g_rc.status == INK_RC_DONE && g_perm_runs == 2, "the AI is not an app");
    CHECK(AI("app.open", "{\"id\":\"beta\"}"), "beta in front");
    CHECK(call_as("test.perm", NULL, INK_SRC_APP) && g_rc.status == INK_RC_DONE && g_perm_runs == 3, "beta declared it (%s)", ink_rc_name(g_rc.status));
    on_ui(go_home);
    delay_ms(100);
    CHECK(call_as("test.perm", NULL, INK_SRC_APP) && g_rc.status == INK_RC_REJECTED, "home did not");
    CHECK(AI("app.close", "{\"id\":\"alpha\"}") && AI("app.close", "{\"id\":\"beta\"}"), "closed");
}

static void cards(void) {
    int64_t rev0 = -1, rev1 = -1;
    ink_state_get_int(ink_state_find("app.cards_rev"), &rev0);
    CHECK(ink_app_state_of("beta") == INK_APP_STOPPED, "stopped");
    CHECK(AI("app.cards", NULL) && strstr(g_result, "\"app\":\"beta\"") && strstr(g_result, "\"title\":\"卡片 0\"") && strstr(g_result, "\"text\":\"现在是 7\"") && strstr(g_result, "\"value\":7") && strstr(g_result, "\"id\":\"c1\"") && strstr(g_result, "\"action\":\"app.list\""),
          "the AI reads the cards of an app that is not running (%s)", g_result);
    g_card_value = 8;
    ink_app_card_changed("beta");
    ink_state_get_int(ink_state_find("app.cards_rev"), &rev1);
    CHECK(rev1 == rev0 + 1, "a change is state (%lld, %lld)", (long long)rev0, (long long)rev1);
    CHECK(AI("app.cards", NULL) && strstr(g_result, "现在是 8"), "read again: the new one (%s)", g_result);
    CHECK(!strstr(g_result, "\"app\":\"reader\""), "nothing is being read yet");
    CHECK(AI("reader.open_book", "{\"title\":\"测试\"}") && g_rc.status == INK_RC_DONE, "a book (%s)", g_rc.detail);
    CHECK(!strcmp(fg_state(), "reader"), "the reader is in the foreground (%s)", fg_state());
    delay_ms(1500);
    CHECK(AI("app.cards", NULL) && strstr(g_result, "\"app\":\"reader\"") && strstr(g_result, "第 "), "and has a card (%s)", g_result);
    on_ui(go_home);
    delay_ms(1500);
}

static int g_home_x, g_home_y, g_home_buttons;
static void home_labels(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_label_class)) {
            const char *text = lv_label_get_text(c);
            if (!strcmp(text, "甲") || !strcmp(text, "乙") || !strcmp(text, "丁") || !strcmp(text, "阅读")) g_home_buttons++;
            if (!strcmp(text, "甲")) {
                lv_area_t a; lv_obj_update_layout(c); lv_obj_get_coords(c, &a);
                g_home_x = (a.x1 + a.x2) / 2;
                g_home_y = a.y1 - ink_ui_px(INK_CIRCLE / 2 + INK_GAP_S);
            }
            CHECK(strcmp(text, "丙"), "an app with missing capabilities stays hidden");
        }
        home_labels(c);
    }
}
static void look_at_home(void) {
    if (strcmp(ink_shell_board(), "Apps")) ink_shell_home_swipe(INK_SWIPE_LEFT);
    g_home_buttons = 0; home_labels(lv_screen_active());
}

static void home_by_hand(void) {
    on_ui(look_at_home);
    CHECK(g_home_buttons == 4, "Apps lists alpha, beta, delta and reader (%d)", g_home_buttons);
    LOG_CLEAR();
    tap(g_home_x, g_home_y);
    CHECK(WAIT_FOR(!strcmp(fg_state(), "alpha"), 2000), "a finger opens it the same way (%s)", fg_state());
    CHECK(SEEN("alpha:start"), "with the same lifecycle (%s)", g_log);
    on_ui(go_home);
    delay_ms(1500);
    CHECK(AI("app.close", "{\"id\":\"alpha\"}"), "closed");
}

static volatile int pin_done, pin_result;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx;
    (void)wait_s;
    pin_result = result;
    pin_done++;
}

static bool click_label(lv_obj_t *root, const char *text) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_check_type(child, &lv_label_class) && !strcmp(lv_label_get_text(child), text) && lv_obj_check_type(root, &lv_button_class)) {
            lv_obj_send_event(root, LV_EVENT_CLICKED, NULL);
            return true;
        }
        if (click_label(child, text)) return true;
    }
    return false;
}

static const char *gate_name;
static bool gate_pin;
static void choose_in_gate(void) {
    CHECK(click_label(lv_layer_sys(), gate_name), "choose %s in the expired member gate", gate_name);
    if (gate_pin) {
        for (int i = 0; i < 4; i++) CHECK(click_label(lv_layer_sys(), "2"), "enter a synthetic PIN by hand");
        CHECK(click_label(lv_layer_sys(), "确定"), "submit the other adult's PIN by hand");
    }
}

/* MW-R1: every way out of another member's expired gate must restart the app lifecycle. */
static void expired_gate_members(void) {
    CHECK(BY_HAND("member.add", "{\"id\":\"guest\",\"name\":\"测试成人\",\"role\":\"adult\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "add a synthetic adult");
    unsigned token = 0;
    CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1 && ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0 &&
          WAIT_FOR(ink_member_exists("guest") != 0, 5000), "confirm the synthetic adult");
    int before = pin_done;
    CHECK(ink_member_set_pin("owner", NULL, "1111", pin_told, NULL) == 0 && WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_OK, "set the owner's synthetic PIN");
    ink_power_hold(INK_NEED_AWAKE);
    const char *const ids[] = {"kid", "guest", "guest"};
    const char *const names[] = {"小", "测", "测"}; /* a tile shows the first character */
    const char *const previous[] = {"孩子的", "", "guest gate fixture"};
    for (int i = 0; i < 3; i++) {
        if (i == 2) {
            before = pin_done;
            CHECK(ink_member_set_pin("guest", NULL, "2222", pin_told, NULL) == 0 && WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_OK, "also cover another adult with a PIN");
        }
        CHECK(BY_HAND("app.open", "{\"id\":\"alpha\"}") && g_rc.status == INK_RC_DONE, "the owner's app before locking");
        snprintf(alpha_kept, sizeof alpha_kept, "owner gate fixture");
        snprintf(alpha_memory, sizeof alpha_memory, "owner gate fixture");
        ink_power_lock();
        CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000), "lock the adult's space");
        delay_ms(121000);
        ink_power_unlock(osal_now_us());
        CHECK(WAIT_FOR(!strcmp(top(), "member_gate"), 2000), "wake into the expired adult gate");
        gate_name = names[i];
        gate_pin = i == 2;
        LOG_CLEAR();
        on_ui(choose_in_gate);
        CHECK(WAIT_FOR(!strcmp(ink_member_current()->id, ids[i]) && !strcmp(top(), "home"), 5000), "the selected member gets home");
        CHECK(ink_app_state_of("home") == INK_APP_FOREGROUND && !strcmp(fg_state(), "home"), "home's lifecycle resumes");
        CHECK(ink_app_state_of("alpha") == INK_APP_STOPPED && SEEN("alpha:stop"), "the old member's app is stopped");
        LOG_CLEAR();
        CHECK(BY_HAND("app.open", "{\"id\":\"alpha\"}") && g_rc.status == INK_RC_DONE, "open the new member's app");
        CHECK(SEEN("alpha:start") && !strcmp(alpha_memory, previous[i]), "start and restore only this member's memory (%s, %s)", g_log, alpha_memory);
        snprintf(alpha_kept, sizeof alpha_kept, "%s gate fixture", ids[i]);
        LOG_CLEAR();
        CHECK(BY_HAND("app.close", "{\"id\":\"alpha\"}") && g_rc.status == INK_RC_DONE && SEEN("alpha:save"), "closing saves under the selected member (%s)", g_log);
        char path[INK_LAYOUT_PATH_MAX], saved[64] = "";
        CHECK(ink_layout_member(path, sizeof path, ids[i], "alpha.state") >= 0 && ink_storage_read(path, saved, sizeof saved - 1) > 0 &&
              !strcmp(saved, alpha_kept), "the selected member's state persisted (%s)", saved);
        CHECK(BY_HAND("app.open", "{\"id\":\"beta\"}") && g_rc.status == INK_RC_DONE &&
              call_as("test.perm", NULL, INK_SRC_APP) && g_rc.status == INK_RC_DONE, "the new foreground app gets its declared permission");
        CHECK(BY_HAND("app.close", "{\"id\":\"beta\"}") && g_rc.status == INK_RC_DONE, "close the permission fixture");
        before = pin_done;
        CHECK(ink_member_unlock("owner", "1111", pin_told, NULL) == 0 && WAIT_FOR(pin_done > before, 5000) && pin_result == INK_PIN_OK &&
              WAIT_FOR(!strcmp(top(), "home") && ink_app_state_of("home") == INK_APP_FOREGROUND, 2000), "return to the owner");
        CHECK(BY_HAND("app.open", "{\"id\":\"alpha\"}") && g_rc.status == INK_RC_DONE && !strcmp(alpha_memory, "owner gate fixture"), "the owner's saved memory stays separate");
        CHECK(BY_HAND("app.close", "{\"id\":\"alpha\"}") && g_rc.status == INK_RC_DONE, "close the owner's fixture");
    }
    ink_power_release(INK_NEED_AWAKE);
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    CHECK(ink_cap_register(TEST_CAPS, 1) == 0, "register");
    CHECK(ink_cap_subscribe("app.state", on_app_state, NULL, INK_EXEC_DIRECT) == 0, "subscribe");
    directory();
    lifecycle();
    saving_fails();
    pressure();
    another_member();
    permissions();
    cards();
    home_by_hand();
    expired_gate_members();
    ink_probe_count("test.finished");
}

void ink_main(void) {
    /* before the UI starts, as an app built into the system does */
    CHECK(ink_app_register(&ALPHA) == 0 && ink_app_register(&BETA) == 0 && ink_app_register(&GAMMA) == 0 && ink_app_register(&DELTA) == 0, "the test's apps");
    product_main(); test_start_home();
    osal_task_cfg_t cfg = {.name = "appstest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
