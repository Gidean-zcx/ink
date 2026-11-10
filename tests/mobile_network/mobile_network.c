/* Synthetic frames, registration, credentials and local dates on the real product. */
#include <stdlib.h>
#include "ink/hal_net.h"
#include "ink/sys/time.h"
#include "ink/sys/textfield.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main
#include "../integration/whole_product.h"

static char page[8192], phase[16];
static const char *target, *typing;
static int x, y, ordinal, seen;
static bool roaming;
static size_t rx_frame, tx_frame;
static uint64_t current_bytes, previous_bytes;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "display settles"); }
static void read_page(void) {
    CHECK(ink_ui_snapshot(page, sizeof page, NULL) > 0, "page exports a snapshot");
    CHECK(ink_ui_context_snapshot(g_snap, sizeof g_snap, NULL) > 0, "page fits AI context");
}
static bool page_has(const char *text) { on_ui(read_page); return strstr(page, text) != NULL; }
static void find(lv_obj_t *o) {
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), target) && seen++ == ordinal) {
        lv_area_t a; lv_obj_get_coords(o, &a); x = (a.x1 + a.x2) / 2; y = (a.y1 + a.y2) / 2;
        CHECK(a.x1 >= 0 && a.x2 < ink_ui_px(INK_CANVAS_W) && a.y1 >= 0 && a.y2 < ink_ui_px(INK_CANVAS_H), "target fits screen");
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) find(lv_obj_get_child(o, (int32_t)i));
}
static void locate(void) {
    seen = 0; x = y = -1; lv_obj_update_layout(lv_screen_active()); find(lv_screen_active());
    if (x >= 0 && !strcmp(target, "密码") && ordinal == 0) y += ink_ui_px((ink_ui_line_height(INK_FONT_NOTE) + INK_CONTROL) / 2);
}
static void press(const char *label, int nth) {
    quiet(); target = label; ordinal = nth; on_ui(locate);
    CHECK(x >= 0 && y >= 0, "touch target exists: %s", label);
    if (x >= 0) tap(x, y);
    quiet();
}
static void insert(void) {
    CHECK(ink_text_focused(), "touch reached the text input driver");
    ink_text_commit_utf8(typing);
    char hidden[2048]; CHECK(ink_text_snapshot(hidden, sizeof hidden) > 0, "text input has a snapshot");
    CHECK(!strstr(hidden, "SYNTH_APN_SECRET_42"), "typing snapshot hides the password");
    ink_text_enter();
}
static void fill(const char *label, const char *text) { press(label, !strcmp(label, "密码") ? 0 : 1); typing = text; on_ui(insert); quiet(); }
static void capture(void *arg) { sim_display_snapshot(arg); }
static void save_page(void *arg) {
    char path[600]; int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, (const char *)arg);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *f = fopen(path, "wb");
    if (!f || fputs(page, f) == EOF || fclose(f)) abort();
}
static void frame(const char *name) {
    quiet(); on_ui(read_page); vtk_host_call(save_page, (void *)name);
    vtk_event_at(vtk_now() + SIM_MS(1), capture, (void *)name); delay_ms(5);
}
static void set(const char *id, int value) {
    char args[96]; snprintf(args, sizeof args, "{\"id\":\"%s\",\"value\":%d}", id, value);
    CHECK(BY_HAND("settings.set", args) && g_rc.status == INK_RC_DONE, "set %s (%s)", id, g_rc.detail);
}
static void clock_at(int month, int day, int hour, int minute) {
    ink_tm_t t = {.year = 2026, .month = month, .day = day, .hour = hour, .min = minute};
    char args[64]; snprintf(args, sizeof args, "{\"utc_s\":%lld}", (long long)ink_time_from_local(&t));
    CHECK(BY_HAND("time.set", args) && g_rc.status == INK_RC_DONE, "set synthetic local clock");
}
static void usage(void) {
    CHECK(AI("net.usage", NULL) && g_rc.status == INK_RC_DONE, "usage capability answers");
    CHECK(!strstr(g_result, "SYNTH_APN_SECRET_42"), "capability never returns password contents");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL); int64_t now = -1, last = -1;
    CHECK(j && !ink_json_parse(j, g_result, strlen(g_result)), "usage is JSON");
    if (j) {
        CHECK(ink_json_int(j, ink_json_find(j, 0, "current"), &now) && ink_json_int(j, ink_json_find(j, 0, "previous"), &last), "usage has both months");
        CHECK(ink_json_eq(j, ink_json_find(j, 0, "available"), "true"), "usage has an actual interface source");
    }
    osal_free(j); current_bytes = (uint64_t)now; previous_bytes = (uint64_t)last;
}
static void world(void *arg) {
    (void)arg; sim_net_roaming(roaming);
}
static void roam(bool on) { roaming = on; vtk_event_at(vtk_now() + SIM_MS(1), world, NULL); delay_ms(5); }
static void frames(void *arg) { (void)arg; sim_net_lte_frame(false, rx_frame); sim_net_lte_frame(true, tx_frame); }
static void traffic(size_t rx, size_t tx) { rx_frame = rx; tx_frame = tx; vtk_event_at(vtk_now() + SIM_MS(1), frames, NULL); delay_ms(5); }
static bool link(hal_link_type_t type) { hal_link_status_t st; hal_net_link_status(&st); return st.type == type && (type == HAL_LINK_NONE || st.up); }
static bool state(const char *id) { int64_t v = 0; return ink_state_get_int(ink_state_find(id), &v) && v; }
static void query_apn(bool automatic, bool secret) {
    char reply[300];
    CHECK(hal_modem_at("AT+CGDCONT?", reply, sizeof reply, 5000) == HAL_AT_OK, "model supports context query");
    CHECK(!strcmp(reply, automatic ? "+CGDCONT: 1,\"IP\",\"\"" : "+CGDCONT: 1,\"IP\",\"example.com\""), "model received the expected APN command");
    CHECK(hal_modem_at("AT+CGAUTH?", reply, sizeof reply, 5000) == HAL_AT_OK, "model supports authentication query");
    CHECK(!strcmp(reply, automatic ? "+CGAUTH: 1,0,\"\",\"\"" : secret ? "+CGAUTH: 1,1,\"SYNTH_USER\",\"SYNTH_APN_SECRET_42\"" : "+CGAUTH: 1,1,\"SYNTH_USER\",\"\""), "model received authentication without logging its contents");
}
static void apn(void) {
    CHECK(AI("shell.network", NULL) && g_rc.status == INK_RC_DONE, "open Network settings"); quiet();
    target = "移动网络"; ordinal = 0;
    for (int i = 0; i < 8; i++) {
        on_ui(locate);
        if (x >= 0) break;
        CHECK(BY_HAND("shell.page_next", NULL) && g_rc.status == INK_RC_DONE, "page through existing network controls"); quiet();
    }
    press("移动网络", 0); CHECK(!strcmp(ink_ui_page_name(), "SetMobile"), "Network touch opens Mobile Network");
    CHECK(WAIT_FOR(page_has("net.apn_set"), 5000), "mobile snapshot exports the atomic capability");
    frame("mobile-zh"); press("手动", 0);
    fill("接入点名称", "example.com"); fill("用户名", "SYNTH_USER"); fill("密码", "SYNTH_APN_SECRET_42");
    CHECK(!page_has("SYNTH_APN_SECRET_42") && !strstr(g_snap, "SYNTH_APN_SECRET_42"), "page and production AI context hide password");
    CHECK(AI("text.state", NULL) && !strstr(g_result, "SYNTH_APN_SECRET_42"), "AI input query hides password");
    frame("apn-zh"); press("保存接入点", 0);
    CHECK(WAIT_FOR(page_has("net.usage_reset") && page_has("example.com"), 6000), "touch save returns to mobile overview");
    query_apn(false, true);
    CHECK(AI("activity.list", NULL) && !strstr(g_result, "SYNTH_APN_SECRET_42"), "activity log hides password");
    CHECK(AI("data.collections", NULL) && g_rc.status == INK_RC_DONE && !strstr(g_result, "net.mobile"), "private credential collection is not offered to AI");
    CHECK(AI("data.get", "{\"collection\":\"net.mobile\",\"id\":1}") && g_rc.status == INK_RC_REJECTED, "AI cannot directly read private credentials");
    set("system.language", 1); quiet();
    CHECK(page_has("Mobile Network") && page_has("Data Roaming") && page_has("This Month"), "mobile overview is English"); frame("mobile-en");
    press("Manual", 0); CHECK(page_has("APN Name") && page_has("Username") && page_has("Password"), "APN editor is English"); frame("apn-en");
    CHECK(page_has("\"value\":\"有值\""), "saved password is represented only by its presence");
    CHECK(AI("shell.mobile_save", NULL) && g_rc.status == INK_RC_INVALID, "AI cannot submit local password draft");
    CHECK(AI("shell.mobile", NULL) && g_rc.status == INK_RC_DONE, "return to overview");
    CHECK(AI("net.apn_set", "{\"automatic\":true}") && g_rc.status == INK_RC_DONE, "AI restores automatic APN without a confirmation");
    query_apn(true, false);
    CHECK(AI("net.apn_set", "{\"automatic\":false,\"name\":\"example.com\",\"user\":\"SYNTH_USER\",\"password\":\"SYNTH_APN_SECRET_42\"}") && g_rc.status == INK_RC_REJECTED, "AI cannot supply a password");
    query_apn(true, false);
    CHECK(AI("net.apn_set", "{\"automatic\":false,\"name\":\"example.com\",\"user\":\"SYNTH_USER\"}") && g_rc.status == INK_RC_DONE, "AI changes nonsecret APN fields through the same capability");
    query_apn(false, false);
    CHECK(BY_HAND("net.apn_set", "{\"automatic\":false,\"name\":\"example.com\",\"user\":\"SYNTH_USER\",\"password\":\"SYNTH_APN_SECRET_42\"}") && g_rc.status == INK_RC_DONE, "person saves APN credentials for restart");
    set("system.language", 0);
}
static void roaming_checks(void) {
    CHECK(!ink_setting_get("net.roaming"), "data roaming is off by default");
    roam(true);
    CHECK(WAIT_FOR(state("cell.roaming") && link(HAL_LINK_NONE), 5000), "roaming registration disables mobile data");
    CHECK(state("cell.registered"), "blocking data leaves calls and SMS registered");
    uint64_t rx0, tx0, rx1, tx1; hal_net_lte_bytes(&rx0, &tx0); traffic(300, 700); hal_net_lte_bytes(&rx1, &tx1);
    CHECK(rx0 == rx1 && tx0 == tx1, "blocked interface transfers no bytes");
    CHECK(AI("net.roaming_set", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "AI enables roaming directly");
    CHECK(WAIT_FOR(link(HAL_LINK_LTE), 5000), "enabling roaming restores mobile data");
    CHECK(AI("net.roaming_set", "{\"enabled\":false}") && g_rc.status == INK_RC_DONE, "disable roaming again");
    CHECK(WAIT_FOR(link(HAL_LINK_NONE), 5000), "disabling roaming drops data again");
    roam(false); CHECK(WAIT_FOR(!state("cell.roaming") && link(HAL_LINK_LTE), 5000), "home registration restores mobile data");
}
static void children(void) {
    CHECK(BY_HAND("system.switch_member", "{\"id\":\"kid\"}") && g_rc.status == INK_RC_DONE, "switch to child");
    CHECK(WAIT_FOR(!strcmp(ink_member_current()->id, "kid") && ink_member_space_open(), 5000), "child space opens");
    const char *caps[] = {"net.apn_set", "net.roaming_set", "net.usage_reset"};
    const char *args[] = {"{\"automatic\":true}", "{\"enabled\":true}", "{}"};
    for (unsigned i = 0; i < sizeof caps / sizeof *caps; i++) {
        CHECK(BY_HAND(caps[i], args[i]) && g_rc.status == INK_RC_REJECTED, "child touch cannot call %s (%d, %.90s)", caps[i], (int)g_rc.status, g_rc.detail);
        CHECK(AI(caps[i], args[i]) && g_rc.status == INK_RC_REJECTED, "child AI cannot call %s", caps[i]);
    }
    CHECK(BY_HAND("settings.set", "{\"id\":\"net.roaming\",\"value\":1}") && g_rc.status == INK_RC_REJECTED, "settings alias does not bypass child restriction");
    CHECK(BY_HAND("system.switch_member", "{\"id\":\"owner\"}") && g_rc.status == INK_RC_DONE, "return to adult");
    CHECK(WAIT_FOR(!strcmp(ink_member_current()->id, "owner") && ink_member_space_open(), 5000), "adult space opens");
}
static void first(void) {
    apn(); CHECK(WAIT_FOR(link(HAL_LINK_LTE), 5000), "data reconnects after APN change"); roaming_checks();
    clock_at(10, 31, 23, 58); usage(); uint64_t before = current_bytes;
    traffic(300, 700); usage();
    CHECK(current_bytes == before + 1000, "month counts exact receive and transmit interface bytes");
    CHECK(WAIT_FOR(page_has("1000 B"), 5000), "visible mobile page follows the actual interface total");
    uint64_t october = current_bytes;
    clock_at(11, 1, 0, 0); usage();
    CHECK(current_bytes == 0 && previous_bytes == october, "local midnight rotates October to previous month");
    CHECK(WAIT_FOR(page_has("1000 B") && page_has("0 B"), 5000), "visible mobile page updates across a month boundary");
    traffic(40, 60); usage(); CHECK(current_bytes == 100, "new month's traffic stays separate");
    clock_at(11, 2, 0, 0); usage(); CHECK(current_bytes == 100, "next local day checkpoints the real total");
    traffic(20, 30); usage(); CHECK(current_bytes == 150, "same-day increments are visible before checkpoint");
    children(); query_apn(false, true);
}
static void restart(void) {
    query_apn(false, true); usage();
    CHECK(current_bytes == 100 && previous_bytes == 1000, "restart restores both months and loses only uncheckpointed day's 50 bytes");
    CHECK(AI("net.usage_reset", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "irreversible reset requires a person");
    unsigned token = 0; sscanf(g_rc.object, "confirm:%u", &token);
    usage(); CHECK(current_bytes == 100 && previous_bytes == 1000, "reset does not act before confirmation");
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "person confirms reset");
    CHECK(WAIT_FOR((usage(), current_bytes == 0 && previous_bytes == 0), 5000), "confirmed reset clears both totals");
}
static void load_phase(void *arg) { (void)arg; const char *v = getenv("INK_MOBILE_PHASE"); snprintf(phase, sizeof phase, "%s", v ? v : "main"); }
static void run(void *arg) {
    (void)arg; CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member space opens"); test_start_home();
    ink_power_hold(INK_NEED_NO_LOCK); vtk_host_call(load_phase, NULL);
    set("net.prefer", 2); set("time.auto", 0); set("time.zone_auto", 0); set("time.zone", 1);
    CHECK(WAIT_FOR(state("cell.registered") && link(HAL_LINK_LTE), 10000), "LTE registered before scenario");
    if (!strcmp(phase, "main")) first();
    else if (!strcmp(phase, "restart")) restart();
    else { usage(); CHECK(current_bytes == 0 && previous_bytes == 0, "cleared totals survive another restart"); }
    CHECK(WAIT_FOR(ink_storage_pending() == 0, 10000), "all writes settle"); ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main(); osal_task_cfg_t cfg = {.name = "mobile-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("mobile test");
}
