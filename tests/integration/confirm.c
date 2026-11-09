/*
 * The confirmation card (system services design §7), on the whole product: what the AI asks for that has consequences
 * waits for a person, who answers with their finger; only then it runs, through the gate again. Results are the probe
 * counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/sys/confirm.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

/* What it asks for. */
static volatile int g_runs, g_page_runs;
static volatile int g_last_src;
static char g_last_to[32];
static void external_run(ink_call_t *c) {
    g_runs++;
    g_last_src = c->src;
    ink_args_get_str(&c->args, "to", g_last_to, sizeof g_last_to);
    snprintf(c->rc.say, sizeof c->rc.say, "办好了");
}
static void page_run(ink_call_t *c) {
    (void)c;
    g_page_runs++;
}
static const ink_cap_t TEST_CAPS[] = {
    {.id = "test.external", .title = "对外发东西", .ask = "发给 {to}：{text}（{count} 份）",
     .input = "{\"type\":\"object\",\"properties\":{\"to\":{\"type\":\"string\"},\"text\":{\"type\":\"string\"},\"count\":{\"type\":\"integer\"}},\"required\":[\"to\"]}",
     .effect = INK_FX_EXTERNAL, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = external_run},
    {.id = "test.plain", .title = "连网络", .input = "{\"type\":\"object\",\"properties\":{\"ssid\":{\"type\":\"string\"},\"password\":{\"type\":\"string\"},\"hidden\":{\"type\":\"boolean\"}}}",
     .effect = INK_FX_SYSTEM, .ask_by_hand = true, .exec = INK_EXEC_DIRECT, .run = external_run},
    {.id = "test.child_no", .title = "孩子不能做的", .effect = INK_FX_EXTERNAL, .exec = INK_EXEC_DIRECT, .run = external_run},
    {.id = "test.page_del", .title = "删这一页的东西", .activity = "testpage", .effect = INK_FX_LOCAL_DESTRUCTIVE, .exec = INK_EXEC_DIRECT, .run = page_run},
};

/* How it ended, as the asker hears it. */
static volatile int g_results;
static char g_decision[24], g_status[24], g_say[64], g_cap[48];
static int64_t g_result_token;
static void on_result(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    (void)id;
    g_decision[0] = g_status[0] = g_say[0] = g_cap[0] = 0;
    ink_args_get_int(a, "token", &g_result_token);
    ink_args_get_str(a, "decision", g_decision, sizeof g_decision);
    ink_args_get_str(a, "status", g_status, sizeof g_status);
    ink_args_get_str(a, "say", g_say, sizeof g_say);
    ink_args_get_str(a, "cap", g_cap, sizeof g_cap);
    g_results++;
}

static int pending_state(void) {
    int64_t v = -1;
    ink_state_get_int(ink_state_find("confirm.pending"), &v);
    return (int)v;
}

static uint32_t token_of_receipt(void) {
    unsigned t = 0;
    sscanf(g_rc.object, "confirm:%u", &t);
    return t;
}

static void page_create(lv_obj_t *scr, void *arg) {
    (void)scr;
    (void)arg;
}
static const ink_activity_t PAGE = {.name = "testpage", .create = page_create};
static void open_page(void) { ink_ui_push(&PAGE, NULL); }
static void close_page(void) { ink_ui_pop(); }
static void turn_page(void) { ink_ui_content_changed(); }

#define ASK_OK(args) (AI("test.external", args) && g_rc.status == INK_RC_PENDING_CONFIRMATION)
#define CARD_UP() (on_ui(read_screen), !strcmp(g_top, "confirm"))
#define CARD_GONE() (on_ui(read_screen), strcmp(g_top, "confirm") != 0)

static bool accept_enabled;
static void read_accept(lv_obj_t *root) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *child = lv_obj_get_child(root, (int32_t)i);
        if (lv_obj_check_type(child, &lv_button_class)) { accept_enabled = !lv_obj_has_state(child, LV_STATE_DISABLED); return; }
        read_accept(child);
    }
}
static void read_accept_state(void) { accept_enabled = false; read_accept(lv_layer_sys()); read_screen(); }
static void accept_visible_card(void) {
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "the complete confirmation is on the panel before acceptance");
    CHECK(WAIT_FOR(({ on_ui(read_accept_state); accept_enabled; }), 10000), "acceptance becomes available after presentation");
    tap(g_btn_x[0], g_btn_y[0]);
}

static void accepted(void) {
    int results = g_results;
    CHECK(ASK_OK("{\"to\":\"10086\",\"text\":\"查询余额\",\"count\":2}"), "the AI asks: it waits (%s, %s)", ink_rc_name(g_rc.status), g_rc.detail);
    uint32_t token = token_of_receipt();
    CHECK(token > 0 && strstr(g_rc.say, "确认"), "the receipt says what it waits for (%s, %s)", g_rc.object, g_rc.say);
    CHECK(g_runs == 0, "and it did not run");
    CHECK(pending_state() == 1, "state says one waits");
    delay_ms(600);
    on_ui(read_screen);
    CHECK(!strcmp(g_top, "confirm") && g_btn_n == 2, "the card is up (%s, %d)", g_top, g_btn_n);
    CHECK(strstr(g_snap, "发给 10086：查询余额（2 份）") && strstr(g_snap, "确认") && strstr(g_snap, "取消"), "saying exactly what will happen (%s)", g_snap);
    CHECK(AI("confirm.list", NULL) && strstr(g_result, "\"token\":") && strstr(g_result, "\"cap\":\"test.external\"") && strstr(g_result, "\"asked_by\":\"ai\"") && strstr(g_result, "10086"), "the AI can see it waits (%s)", g_result);

    /* the AI cannot answer it, by any way there is */
    char args[48];
    snprintf(args, sizeof args, "{\"token\":%u}", (unsigned)token);
    CHECK(AI("confirm.accept", args) && g_rc.status == INK_RC_INVALID, "not offered to the AI (%s)", ink_rc_name(g_rc.status));
    CHECK(call_as("confirm.accept", args, INK_SRC_REMOTE) && g_rc.status == INK_RC_INVALID, "nor to the hub");
    CHECK(call_as("confirm.accept", args, INK_SRC_APP) && g_rc.status == INK_RC_INVALID, "nor to an app");
    CHECK(ink_confirm_decide(token, true, INK_SRC_AI) != 0 && ink_confirm_decide(token, true, INK_SRC_SYSTEM) != 0 && ink_confirm_decide(token, true, INK_SRC_CONFIRMED) != 0, "nor through the C interface");
    char *list = osal_malloc(32768, OSAL_MEM_EXTERNAL);
    bool listed = false, withdraw = false;
    for (int cursor = 0; list && cursor >= 0;) {
        int next = -1, n = ink_cap_manifest_page(cursor, NULL, list, 32768, &next);
        CHECK(n > 0 && !strstr(list, "confirm.accept") && !strstr(list, "confirm.reject"), "every tool page excludes human confirmation");
        if (n < 0) break;
        listed |= strstr(list, "confirm.list") != NULL; withdraw |= strstr(list, "confirm.withdraw") != NULL;
        cursor = next;
    }
    CHECK(listed && withdraw, "query and withdrawal remain in the complete paginated tool list");
    osal_free(list);
    CHECK(g_runs == 0 && pending_state() == 1, "so it still waits");

    accept_visible_card();
    CHECK(WAIT_FOR(g_results == results + 1, 2000), "a person says yes");
    CHECK(g_runs == 1 && g_last_src == INK_SRC_CONFIRMED && !strcmp(g_last_to, "10086"), "it runs, once, as confirmed, with what was asked (%d, %d, %s)", g_runs, g_last_src, g_last_to);
    CHECK(!strcmp(g_decision, "accepted") && !strcmp(g_status, "done") && !strcmp(g_say, "办好了") && g_result_token == token && !strcmp(g_cap, "test.external"), "the asker hears how it went (%s, %s, %s)", g_decision, g_status, g_say);
    CHECK(pending_state() == 0, "nothing waits");
    delay_ms(600);
    CHECK(CARD_GONE(), "the card goes (%s)", g_top);
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) != 0 && g_runs == 1, "a token is answered once");
    delay_ms(1200);
}

static void rejected(void) {
    int results = g_results, runs = g_runs;
    CHECK(ASK_OK("{\"to\":\"a\"}"), "asked");
    uint32_t token = token_of_receipt();
    delay_ms(600);
    on_ui(read_screen);
    tap(g_btn_x[1], g_btn_y[1]);
    CHECK(WAIT_FOR(g_results == results + 1, 2000) && !strcmp(g_decision, "rejected") && g_result_token == token && !g_status[0], "no: the asker hears (%s)", g_decision);
    CHECK(g_runs == runs, "it does not run");
    delay_ms(1200);

    /* the key that leaves is a no */
    results = g_results;
    CHECK(ASK_OK("{\"to\":\"b\"}"), "asked");
    delay_ms(600);
    CHECK(CARD_UP(), "the card");
    key(HAL_KEY_NEXT);
    CHECK(CARD_UP() && g_results == results, "other keys do nothing");
    key(HAL_KEY_HOME);
    CHECK(WAIT_FOR(g_results == results + 1, 2000) && !strcmp(g_decision, "rejected") && g_runs == runs, "home says no (%s)", g_decision);
    delay_ms(1200);

    /* by hand there is no card */
    results = g_results;
    CHECK(BY_HAND("test.external", "{\"to\":\"me\"}") && g_rc.status == INK_RC_DONE && g_runs == runs + 1 && pending_state() == 0 && g_results == results, "what a person does by hand needs no yes");
    runs = g_runs;

    /* arguments that could not work: no card for them */
    CHECK(AI("test.external", "{\"text\":\"没有收件人\"}") && g_rc.status == INK_RC_INVALID && pending_state() == 0, "bad arguments are refused at once (%s)", ink_rc_name(g_rc.status));
    /* the same request twice: one card, the same answer */
    ink_call_opts_t o = {.src = INK_SRC_AI, .args_json = "{\"to\":\"c\"}", .request_id = "rid-1", .reply_exec = INK_EXEC_DIRECT, .done = rc_done};
    g_done = 0;
    CHECK(ink_cap_call("test.external", NULL, &o) == 0 && WAIT_FOR(g_done, 1000) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "with a request id");
    char first[48];
    snprintf(first, sizeof first, "%s", g_rc.object);
    g_done = 0;
    CHECK(ink_cap_call("test.external", NULL, &o) == 0 && WAIT_FOR(g_done, 1000) && !strcmp(g_rc.object, first) && pending_state() == 1, "again: the same receipt, no second card (%s, %d)", g_rc.object, pending_state());
    CHECK(ink_confirm_withdraw(token_of_receipt(), INK_SRC_AI) == 0, "taken back");
    delay_ms(1200);
}

static void expired(void) {
    int results = g_results, runs = g_runs;
    CHECK(ASK_OK("{\"to\":\"d\"}"), "asked");
    uint32_t token = token_of_receipt();
    delay_ms(600);
    CHECK(CARD_UP(), "the card");
    delay_ms(50000);
    CHECK(g_results == results && CARD_UP(), "it waits for a while");
    CHECK(WAIT_FOR(g_results == results + 1, 12000) && !strcmp(g_decision, "expired"), "after a minute it is dropped (%s)", g_decision);
    CHECK(g_runs == runs && pending_state() == 0, "it did not run");
    delay_ms(600);
    CHECK(CARD_GONE(), "the card goes");
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) != 0 && g_runs == runs, "a yes after that is too late");
    delay_ms(1200);
}

static void withdrawn(void) {
    int results = g_results, runs = g_runs;
    CHECK(ASK_OK("{\"to\":\"e\"}"), "asked");
    uint32_t token = token_of_receipt();
    char args[48];
    snprintf(args, sizeof args, "{\"token\":%u}", (unsigned)token);
    CHECK(call_as("confirm.withdraw", args, INK_SRC_REMOTE) && g_rc.status == INK_RC_FAILED, "someone else cannot take it back (%s)", ink_rc_name(g_rc.status));
    CHECK(pending_state() == 1, "it still waits");
    CHECK(AI("confirm.withdraw", args) && g_rc.status == INK_RC_DONE, "the AI changes its mind");
    CHECK(WAIT_FOR(g_results == results + 1, 500) && !strcmp(g_decision, "withdrawn") && g_runs == runs, "it is dropped, told (%s)", g_decision);
    CHECK(AI("confirm.withdraw", args) && g_rc.status == INK_RC_FAILED, "not there any more");
    delay_ms(1200);
}

static void queue(void) {
    uint32_t tokens[4];
    int runs = g_runs, results = g_results;
    for (int i = 0; i < 4; i++) {
        char a[32];
        snprintf(a, sizeof a, "{\"to\":\"q%d\"}", i);
        CHECK(ASK_OK(a), "asked %d", i);
        tokens[i] = token_of_receipt();
    }
    CHECK(pending_state() == 4, "four wait");
    CHECK(AI("test.external", "{\"to\":\"q4\"}") && g_rc.status == INK_RC_REJECTED && strstr(g_rc.detail, "full"), "a fifth is refused (%s, %s)", ink_rc_name(g_rc.status), g_rc.detail);
    delay_ms(600);
    on_ui(read_screen);
    CHECK(strstr(g_snap, "q0"), "the oldest is on the card (%s)", g_snap);
    tap(g_btn_x[1], g_btn_y[1]);
    delay_ms(600);
    on_ui(read_screen);
    CHECK(strstr(g_snap, "q1") && g_btn_n == 2, "then the next (%s, %d)", g_snap, g_btn_n);
    accept_visible_card();
    CHECK(WAIT_FOR(g_runs == runs + 1, 1000) && !strcmp(g_last_to, "q1"), "that one runs (%s)", g_last_to);
    delay_ms(600);
    on_ui(read_screen);
    CHECK(strstr(g_snap, "q2"), "the next (%s)", g_snap);
    ink_confirm_withdraw(tokens[2], INK_SRC_AI);
    ink_confirm_withdraw(tokens[3], INK_SRC_AI);
    CHECK(WAIT_FOR(g_results == results + 4, 1000), "each told (%d)", g_results - results);
    delay_ms(1200);
    CHECK(CARD_GONE(), "the card goes");
}

static void member_changes(void) {
    int runs = g_runs, results = g_results;
    CHECK(ASK_OK("{\"to\":\"f\"}"), "asked");
    uint32_t token = token_of_receipt();
    delay_ms(300);
    CHECK(ink_member_switch("kid") == 0, "someone else takes the device");
    CHECK(WAIT_FOR(g_results == results + 1, 1000) && !strcmp(g_decision, "member_changed"), "it ends with the one who asked (%s)", g_decision);
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) != 0 && g_runs == runs, "the next person cannot say yes to it");
    delay_ms(1200);
    CHECK(CARD_GONE(), "the card goes");

    /* a child's AI: what is not on its whitelist is refused, no card */
    CHECK(AI("test.child_no", NULL) && g_rc.status == INK_RC_REJECTED && pending_state() == 0, "not for children: refused before any card (%s)", ink_rc_name(g_rc.status));
    ink_member_test_reset();
    delay_ms(300);
    /* and it is looked at again when the answer comes: one asked as an adult, whoever answers */
    results = g_results;
    CHECK(AI("test.child_no", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "an adult's AI asks (%s)", ink_rc_name(g_rc.status));
    delay_ms(600);
    on_ui(read_screen);
    CHECK(g_btn_n == 2, "the card");
    accept_visible_card();
    CHECK(WAIT_FOR(g_results == results + 1, 1000) && !strcmp(g_status, "done"), "and it runs (%s)", g_status);
    delay_ms(1200);
}

static void not_while(void) {
    /* locked: nobody is shown what is asked, the screen does not come on for it */
    int results = g_results;
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000), "locked");
    CHECK(ASK_OK("{\"to\":\"g\"}"), "asked while locked");
    delay_ms(600);
    CHECK(!strcmp(power_state(), "locked") && CARD_GONE(), "it stays locked, no card (%s, %s)", power_state(), g_top);
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR((on_ui(read_screen), !strcmp(g_top, "confirm")), 3000), "unlocked: there it is (%s)", g_top);
    ink_power_lock();
    CHECK(WAIT_FOR(CARD_GONE(), 3000), "locked again: it goes (%s)", g_top);
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(CARD_UP(), 3000), "and comes back");

    /* an interruption comes first */
    ink_interrupt_t it = {.kind = INK_INT_LOW_BATTERY, .nactions = 0};
    snprintf(it.title, sizeof it.title, "低电量");
    int id = ink_interrupt_raise(&it);
    CHECK(WAIT_FOR((on_ui(read_screen), !strcmp(g_top, "interrupt")), 2000), "an interruption takes the screen (%s)", g_top);
    CHECK(g_btn_n == 0, "and the card's buttons are gone (%d)", g_btn_n);
    ink_interrupt_end(id);
    CHECK(WAIT_FOR(CARD_UP(), 3000), "it ends: the card is back (%s)", g_top);
    on_ui(read_screen);
    tap(g_btn_x[1], g_btn_y[1]);
    CHECK(WAIT_FOR(g_results == results + 1, 1000) && !strcmp(g_decision, "rejected"), "answered");
    delay_ms(1200);
}

static void page_bound(void) {
    on_ui(open_page);
    delay_ms(1200);
    uint32_t gen = ink_ui_content_version();
    ink_receipt_t rc;
    CHECK(ink_cap_invoke("test.page_del", NULL, INK_SRC_AI, gen, &rc) == INK_RC_PENDING_CONFIRMATION, "the AI on a page (%s)", rc.detail);
    delay_ms(600);
    on_ui(read_screen);
    CHECK(!strcmp(g_top, "confirm") && g_btn_n == 2, "the card");
    CHECK(ink_ui_content_version() == gen, "the card does not change the page's version");
    int results = g_results;
    accept_visible_card();
    CHECK(WAIT_FOR(g_results == results + 1, 1000) && g_page_runs == 1 && !strcmp(g_status, "done"), "it runs on the page under the card (%s, %d)", g_status, g_page_runs);
    delay_ms(1200);

    /* the page changed while the card waited */
    gen = ink_ui_content_version();
    CHECK(ink_cap_invoke("test.page_del", NULL, INK_SRC_AI, gen, &rc) == INK_RC_PENDING_CONFIRMATION, "again");
    delay_ms(600);
    on_ui(turn_page);
    on_ui(read_screen);
    results = g_results;
    accept_visible_card();
    CHECK(WAIT_FOR(g_results == results + 1, 1000) && !strcmp(g_decision, "accepted") && !strcmp(g_status, "stale") && g_page_runs == 1, "what it was about is gone: not run, and said (%s, %d)", g_status, g_page_runs);
    delay_ms(1200);

    /* the page was left */
    gen = ink_ui_content_version();
    CHECK(ink_cap_invoke("test.page_del", NULL, INK_SRC_AI, gen, &rc) == INK_RC_PENDING_CONFIRMATION, "once more");
    delay_ms(600);
    on_ui(close_page);
    delay_ms(300);
    on_ui(read_screen);
    results = g_results;
    accept_visible_card();
    CHECK(WAIT_FOR(g_results == results + 1, 1000) && !strcmp(g_status, "invalid") && g_page_runs == 1, "its page is gone: not run (%s, %d)", g_status, g_page_runs);
    delay_ms(1200);
}

static void what_the_card_says(void) {
    CHECK(AI("test.plain", "{\"ssid\":\"home\",\"password\":\"hunter2\",\"hidden\":true}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "with no sentence of its own");
    uint32_t token = token_of_receipt();
    delay_ms(600);
    on_ui(read_screen);
    CHECK(strstr(g_snap, "连网络") && strstr(g_snap, "ssid：home") && strstr(g_snap, "hidden：是"), "its title and arguments (%s)", g_snap);
    CHECK(!strstr(g_snap, "hunter2") && strstr(g_snap, "***"), "a password is not on the screen (%s)", g_snap);
    CHECK(AI("confirm.list", NULL) && !strstr(g_result, "hunter2"), "nor in what the AI reads (%s)", g_result);
    char longer[800] = "{\"to\":\"";
    size_t n = strlen(longer);
    for (int i = 0; i < 90; i++) n += (size_t)snprintf(longer + n, sizeof longer - n, "长");
    snprintf(longer + n, sizeof longer - n, "\"}");
    ink_confirm_withdraw(token, INK_SRC_AI);
    delay_ms(1200);
    int runs = g_runs;
    CHECK(AI("test.external", longer) && g_rc.status == INK_RC_REJECTED, "an over-capacity description is refused without hiding any object");
    ink_confirm_view_t v;
    CHECK(ink_confirm_list(&v, 1) == 0 && g_runs == runs, "no truncated transaction is queued or executed");
    delay_ms(1200);
}

static void settings_system(void) {
    static const ink_setting_def_t SYS = {.id = "test.sys", .title = "系统级", .type = INK_SET_BOOL, .def = 0, .system = true};
    CHECK(ink_settings_define(&SYS, 1) == 0, "defined");
    int results = g_results;
    CHECK(AI("settings.set", "{\"id\":\"test.sys\",\"value\":true}") && g_rc.status == INK_RC_DONE && ink_setting_get("test.sys") && g_rc.undo_ms,
          "D-105: a non-risk system setting is done and reversible (%s)", ink_rc_name(g_rc.status));
    CHECK(g_results == results && pending_state() == 0, "no confirmation card");
    char args[100];
    snprintf(args, sizeof args, "{\"object\":\"%s\"}", g_rc.object);
    CHECK(AI("system.undo", args) && g_rc.status == INK_RC_UNDONE && !ink_setting_get("test.sys"), "and can be undone");
}

static void a_real_message(void) {
    int results = g_results;
    delay_ms(15000); /* the modem registers */
    int sent = sim_net_sms_sent();
    CHECK(AI("message.send", "{\"to\":\"10086\",\"text\":\"查询余额\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "the AI wants to send a text (%s)", ink_rc_name(g_rc.status));
    delay_ms(600);
    on_ui(read_screen);
    CHECK(strstr(g_snap, "发短信给 10086：查询余额"), "the card says to whom and what (%s)", g_snap);
    delay_ms(2000);
    CHECK(sim_net_sms_sent() == sent, "nothing has gone out");
    accept_visible_card();
    CHECK(WAIT_FOR(g_results == results + 1, 40000) && !strcmp(g_status, "done"), "a person says yes: sent (%s)", g_status);
    CHECK(sim_net_sms_sent() == sent + 1, "the modem sent one (%d)", sim_net_sms_sent() - sent);
    delay_ms(1200);
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    CHECK(ink_cap_register(TEST_CAPS, (int)(sizeof TEST_CAPS / sizeof TEST_CAPS[0])) == 0, "register");
    CHECK(ink_cap_subscribe("confirm.result", on_result, NULL, INK_EXEC_DIRECT) == 0, "subscribe");
    accepted();
    rejected();
    expired();
    withdrawn();
    queue();
    member_changes();
    not_while();
    page_bound();
    what_the_card_says();
    settings_system();
    a_real_message();
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "confirmtest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
