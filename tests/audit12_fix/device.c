/* batch12 audit: settings.set goes through a setting's own capability (M1), a child in airplane mode is sent to a parent
 * and a refused button says why (M2), turning the radio on does not hold the background or the modem (M3), reader.last
 * is written only when it changes, after the book opened (L7). Synthetic data only. */
#include <stdlib.h>
#include "ink/sys/ai.h"
#include "ink/sys/members.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char which[16], page[4096];
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 12000), "display settles"); }
static int64_t state(const char *id) { int64_t v = 0; ink_state_get_int(ink_state_find(id), &v); return v; }
static void read_page(void) { if (ink_ui_snapshot(page, sizeof page, NULL) <= 0) page[0] = 0; }
static const char *top(void) { on_ui(read_screen); on_ui(read_page); return g_top; }
static void read_env(void *arg) { (void)arg; const char *v = getenv("INK_AUDIT12_CASE"); snprintf(which, sizeof which, "%s", v ? v : ""); }

/* ------------------------------------------------------------ M1 */
static void via(void) {
    CHECK(WAIT_FOR(ink_setting_get("wifi.enabled") == 1 && !ink_setting_get("net.airplane"), 3000), "Wi-Fi on, airplane mode off");
    CHECK(AI("settings.set", "{\"id\":\"wifi.enabled\",\"value\":0}") && g_rc.status == INK_RC_DONE && strstr(g_rc.say, "已关闭 Wi-Fi"),
        "the AI's settings.set answers with wifi.set_enabled's words: %.120s", g_rc.say);
    CHECK(ink_setting_get("wifi.enabled") == 1, "Wi-Fi stays until the receipt is out");
    CHECK(WAIT_FOR(ink_setting_get("wifi.enabled") == 0, 4000), "then Wi-Fi goes off");
    CHECK(AI("settings.set", "{\"id\":\"wifi.enabled\",\"value\":true}") && g_rc.status == INK_RC_DONE && ink_setting_get("wifi.enabled") == 1,
        "turning it on again is at once");
    CHECK(AI("settings.set", "{\"id\":\"net.airplane\",\"value\":1}") && g_rc.status == INK_RC_DONE && strstr(g_rc.say, "已打开飞行模式"),
        "the AI's settings.set answers with net.set_airplane's words: %.120s", g_rc.say);
    CHECK(!ink_setting_get("net.airplane") && ink_setting_get("wifi.enabled") == 1, "the link stays until the receipt is out");
    /* Wi-Fi goes off in the airplane setting's apply, a background job right after the setting itself */
    CHECK(WAIT_FOR(ink_setting_get("net.airplane") == 1 && !ink_setting_get("wifi.enabled"), 4000), "then airplane mode is on");
    CHECK(AI("settings.set", "{\"id\":\"net.airplane\",\"value\":2}") && g_rc.status == INK_RC_INVALID, "a value out of range is still refused");
    CHECK(AI("settings.list", NULL) && strstr(g_result, "\"via\":\"net.set_airplane\"") && strstr(g_result, "\"via\":\"wifi.set_enabled\""),
        "settings.list names the capability: %.150s", g_result);
    CHECK(BY_HAND("settings.set", "{\"id\":\"net.airplane\",\"value\":false}") && g_rc.status == INK_RC_DONE && strstr(g_rc.say, "已关闭飞行模式"),
        "by hand it is at once, with the same words");
    CHECK(WAIT_FOR(!ink_setting_get("net.airplane") && ink_setting_get("wifi.enabled") == 1, 3000), "airplane mode off, Wi-Fi back");
    /* audit13 low 7: an expected_revision is checked before the setting's own capability; no undo is offered */
    CHECK(AI("settings.set", "{\"id\":\"wifi.enabled\",\"value\":0,\"expected_revision\":4000000000}") && g_rc.status == INK_RC_STALE &&
        ink_setting_get("wifi.enabled") == 1, "a stale expected_revision is refused, Wi-Fi stays (%s)", ink_rc_name(g_rc.status));
    CHECK(BY_HAND("settings.set", "{\"id\":\"wifi.enabled\",\"value\":0}") && g_rc.status == INK_RC_DONE && !g_rc.undo_ms && !ink_setting_get("wifi.enabled"),
        "Wi-Fi off through wifi.set_enabled offers no system.undo, as the capability table says");
    CHECK(BY_HAND("settings.set", "{\"id\":\"wifi.enabled\",\"value\":1}") && ink_setting_get("wifi.enabled") == 1, "and back on");
}

/* ------------------------------------------------------------ M2 */
static ink_receipt_t g_plane;
static void plane_card(void) { ink_shell_airplane_card(&g_plane); }
static void ask(void) { ink_ai_ask_text("合成测试问题"); }
static lv_obj_t *g_button;
static void press_refused(void) {
    g_button = lv_obj_create(lv_screen_active());
    ink_shell_bind(g_button, "net.set_airplane", "{\"enabled\":false}");
    lv_obj_send_event(g_button, LV_EVENT_CLICKED, NULL);
}
static void child(void) {
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "an adult turns airplane mode on");
    CHECK(WAIT_FOR(ink_setting_get("net.airplane") == 1, 3000), "airplane mode is on");
    CHECK(BY_HAND("call.dial", "{\"number\":\"10086\"}") && g_rc.status == INK_RC_REJECTED, "a call is refused in airplane mode");
    g_plane = g_rc;
    CHECK(ink_member_switch("kid") == 0, "the child uses the device"); quiet();
    on_ui(plane_card); quiet();
    CHECK(!strcmp(top(), "AirplaneOn"), "the airplane panel shows: %.31s", g_top);
    CHECK(strstr(page, "请家长关闭飞行模式") && !strstr(page, "net.set_airplane") && strstr(page, "知道了"),
        "it sends the child to a parent, without the switch: %.150s", page);
    CHECK(g_btn_n == 1, "one button (%d)", g_btn_n);
    if (g_btn_n) tap(g_btn_x[0], g_btn_y[0]);
    quiet();
    CHECK(strcmp(top(), "AirplaneOn") && ink_setting_get("net.airplane") == 1, "知道了 closes it, airplane mode stays");

    CHECK(WAIT_FOR(!state("net.online"), 5000), "offline");
    on_ui(ask);
    CHECK(WAIT_FOR(!strcmp(top(), "AiOffline"), 3000), "the child's question ends at once: %.31s", g_top);
    CHECK(strstr(page, "请家长关闭飞行模式") && !strstr(page, "net.set_airplane"), "the AI page sends the child to a parent: %.150s", page);
    CHECK(AI("ai.pages.close", NULL), "close it"); quiet();

    CHECK(strcmp(top(), "Refused"), "nothing refused so far: %.31s", g_top);
    on_ui(press_refused); quiet();
    CHECK(WAIT_FOR(!strcmp(top(), "Refused"), 3000), "a button the gate refuses is not silent: %.31s", g_top);
    CHECK(strstr(page, "请家长来操作"), "it says why: %.150s", page);
    CHECK(ink_setting_get("net.airplane") == 1, "and nothing changed");
}

/* ------------------------------------------------------------ M3 */
static sim_cell_state_t cell;
static void read_cell(void *arg) { (void)arg; sim_net_cell_state(&cell); }
static bool radio(bool on) { vtk_event_at(vtk_now() + SIM_MS(1), read_cell, NULL); delay_ms(5); return cell.radio == on; }
static bool coverage_on;
static void coverage(void *arg) {
    (void)arg;
    sim_net_env(1, coverage_on, -90);
    for (int s = 0; s < 2; s++) sim_net_set_card(s, coverage_on, true, true);
}
static void world(bool on) { coverage_on = on; vtk_event_at(vtk_now() + SIM_MS(1), coverage, NULL); delay_ms(5); }
static volatile int g_bg;
static ink_exec_job_t g_bg_job;
static void bg_job(void *arg) { (void)arg; __atomic_store_n(&g_bg, 1, __ATOMIC_RELEASE); }
static void radio_case(void) {
    CHECK(WAIT_FOR(state("cell.registered"), 20000), "the card is registered");
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "airplane mode on");
    CHECK(WAIT_FOR(radio(false), 5000), "the radio is off");
    world(false); /* no card, no signal */
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":false}") && g_rc.status == INK_RC_DONE, "airplane mode off");
    CHECK(WAIT_FOR(radio(true), 5000), "the radio is turned on");
    delay_ms(500);
    uint64_t t0 = osal_now_us();
    g_bg = 0;
    CHECK(ink_exec_post(INK_EXEC_BG, &g_bg_job, bg_job, NULL) == 0, "a background job is queued");
    CHECK(WAIT_FOR(__atomic_load_n(&g_bg, __ATOMIC_ACQUIRE), 1000), "it runs within a second (%llu ms)", (unsigned long long)((osal_now_us() - t0) / 1000));
    t0 = osal_now_us();
    CHECK(BY_HAND("call.dial", "{\"number\":\"10086\"}") && g_rc.status != INK_RC_DONE, "dialling answers: %s %.60s", ink_rc_name(g_rc.status), g_rc.detail);
    uint64_t ms = (osal_now_us() - t0) / 1000;
    CHECK(ms < 3000, "dialling got the modem at once (%llu ms)", (unsigned long long)ms);
    CHECK(!state("cell.registered"), "not registered without a card");
    /* the card back (the modem reads it at the next radio-on): registering is looked at again, not waited for */
    world(true);
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":true}") && WAIT_FOR(radio(false), 5000), "airplane mode on again");
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":false}") && WAIT_FOR(radio(true), 5000), "and off");
    CHECK(WAIT_FOR(state("cell.registered"), 15000), "the later look finds the card registered");
}

/* ------------------------------------------------------------ L7 */
static void reader_case(void) {
    double base = sim_metric_value("reader.last_saved");
    CHECK(AI("reader.open_book", "{\"title\":\"测试长书\"}") && g_rc.status == INK_RC_DONE, "open the book (%s)", g_rc.detail); quiet();
    CHECK(sim_metric_value("reader.last_saved") == base, "opening does not write at once");
    delay_ms(9000);
    CHECK(sim_metric_value("reader.last_saved") == base + 1, "it is written after the pause (%.0f)", sim_metric_value("reader.last_saved") - base);
    CHECK(AI("reader.goto_page", "{\"page\":1}") && g_rc.status == INK_RC_DONE, "the same page again"); quiet();
    delay_ms(9000);
    CHECK(sim_metric_value("reader.last_saved") == base + 1, "unchanged: not written again");
    CHECK(AI("reader.goto_page", "{\"page\":2}") && g_rc.status == INK_RC_DONE, "the next page"); quiet();
    delay_ms(9000);
    CHECK(sim_metric_value("reader.last_saved") == base + 2, "a new page is written");
    CHECK(AI("system.home", NULL) && g_rc.status == INK_RC_DONE, "leave the book"); quiet();
    CHECK(sim_metric_value("reader.last_saved") == base + 2, "leaving without a change writes nothing");
}

static void run(void *arg) {
    (void)arg;
    vtk_host_call(read_env, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens");
    test_start_home(); quiet();
    if (!strcmp(which, "m1")) via();
    else if (!strcmp(which, "m2")) child();
    else if (!strcmp(which, "m3")) radio_case();
    else if (!strcmp(which, "l7")) reader_case();
    else CHECK(false, "unknown case %s", which);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "audit12", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL,
        .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("audit12 test");
}
