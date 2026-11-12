/* Synthetic notification service contracts, using the whole simulated product. */
#include "ink/ui_shell.h"
#include "ink/sys/time.h"
#include "ink/sys/storage.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#include <stdlib.h>

static char phase[16];
static volatile int dnd_events;
static void dnd_changed(void *ctx, const char *id, const ink_args_t *args) { (void)ctx; (void)id; (void)args; dnd_events++; }
static ink_notify_view_t notice;
static const ink_db_def_t LEGACY = {.name = "notify.inbox", .title = "通知", .scope = INK_DB_MEMBER, .schema = 1, .json = true, .ai_read = true};
static const char CONTACT[] = "00000000991", STRANGER[] = "00000000992";
static void get_phase(void *arg) { (void)arg; const char *v = getenv("INK_NOTIFY_MODEL_PHASE"); snprintf(phase, sizeof phase, "%s", v ? v : "main"); }
static void setting(const char *id, int64_t v) {
    ink_receipt_t rc; CHECK(ink_setting_set(id, v, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "set %s: %.80s", id, rc.detail);
}
static void model_set(const char *json) { CHECK(BY_HAND("notify.dnd_set", json) && g_rc.status == INK_RC_DONE, "set preferences: %.100s", g_rc.detail); }
static void clock_at(int day, int hour, int minute) {
    ink_tm_t tm = {.year = 2026, .month = 11, .day = day, .hour = hour, .min = minute};
    ink_time_set(ink_time_from_local(&tm), false);
}
static void state(bool active, const char *reason, int hour, int minute) {
    ink_notify_dnd_t d; ink_notify_dnd_state(&d);
    CHECK(d.active == active && !strcmp(d.reason, reason), "dnd active=%d reason=%s, wanted %d %s", d.active, d.reason, active, reason);
    int64_t published = -1;
    CHECK(WAIT_FOR(ink_state_get_int(ink_state_find("notify.dnd_active"), &published) && published == active, 2000), "minute/settings events publish current DND state");
    if (active && strcmp(reason, "manual")) { ink_tm_t end; ink_time_to_local(d.until_s, &end); CHECK(end.hour == hour && end.min == minute, "ends at %02d:%02d", hour, minute); }
}
static bool find_notice(const char *title) {
    ink_db_cursor_t c = {0};
    while (ink_notify_next(&c, &notice) == 1) if (!strcmp(notice.title, title)) return true;
    return false;
}
static bool ringing(void) {
    return AI("interrupt.list", NULL) && strstr(g_result, "\"ringing\":true") != NULL;
}
static void hangup(void) {
    hw(HW_HANGUP, 0, 0); CHECK(WAIT_FOR(!strcmp(active_state(), "none"), 15000), "call ends");
}
static void call_number(const char *number, bool ring) {
    snprintf(g_hw.num, sizeof g_hw.num, "%s", number); hw(HW_CALL, 0, 0);
    CHECK(WAIT_FOR(!strcmp(active_state(), "call"), 15000), "incoming card precedes contact lookup");
    uint64_t card = osal_now_us();
    if (ring) { CHECK(WAIT_FOR(ringing(), 5000), "allowed call rings"); ink_probe_value("test.notify_ring_delay_ms", (double)(osal_now_us() - card) / 1000.0); }
    else { CHECK(AI("notify.dnd_get", NULL), "BG lookup completes before quiet assertion"); CHECK(!ringing(), "stranger call remains silent"); }
    hangup(); CHECK(WAIT_FOR(find_notice("未接来电"), 5000), "missed call remains in inbox");
    CHECK(notice.nactions == 2 && !strcmp(notice.actions[0].cap, "call.dial") && !strcmp(notice.actions[1].cap, "sms.compose"), "missed-call action targets");
}
static void seed(void) {
    ink_db_t *db = ink_db_open_member(&LEGACY, "owner");
    const char raw[] = "{\"read\":false,\"prio\":1,\"ch\":\"message\",\"title\":\"SYNTH_V1\",\"body\":\"SYNTH body\",\"group\":\"SYNTH\",\"tag\":\"v1\",\"action\":\"sms.thread\",\"args\":{\"peer\":\"00000000992\"}}";
    ink_db_write_t w = {.data = raw, .len = sizeof raw - 1, .schema = 1, .src = INK_SRC_SYSTEM, .sort = 1};
    CHECK(db && ink_db_put(db, &w, NULL) == INK_DB_OK, "write original schema-1 record"); ink_db_close(db);
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 15000), "legacy inbox durable");
    const char old[] = "{\"notify.dnd\":1,\"time.auto\":0}";
    CHECK(ink_storage_write_atomic(INK_SETTINGS_PATH, old, sizeof old - 1) == 0, "write old unscoped manual setting");
}
static void schedules(void) {
    CHECK(find_notice("SYNTH_V1") && !notice.read && notice.prio == 1 && !strcmp(notice.body, "SYNTH body") && !strcmp(notice.group, "SYNTH") && !strcmp(notice.tag, "v1") && !strcmp(notice.action, "sms.thread") && strstr(notice.action_args, STRANGER), "legacy record fields preserved");
    CHECK(notice.nactions == 1 && !strcmp(notice.actions[0].cap, "sms.thread"), "old action also appears in new action array");
    CHECK(ink_setting_get("notify.dnd") == 1, "legacy notify.dnd remains enabled"); state(true, "manual", 0, 0);
    setting("notify.dnd", 0); setting("time.auto", 0); setting("power.lock_after_s", 3600);
    model_set("{\"slot\":0,\"days\":62,\"start\":1320,\"end\":420,\"calls\":2,\"repeat\":true}");
    clock_at(11, 23, 30); state(true, "schedule", 7, 0);
    CHECK(AI("notify.dnd_get", NULL) && strstr(g_result, "\"reason\":\"schedule\"") && strstr(g_result, "\"slot\":0"), "status identifies the active schedule");
    clock_at(12, 6, 59); state(true, "schedule", 7, 0);
    clock_at(12, 7, 0); state(false, "off", 0, 0);
    clock_at(14, 23, 30); state(false, "off", 0, 0);
    model_set("{\"slot\":1,\"days\":65,\"start\":1380,\"end\":480}"); state(true, "schedule", 8, 0);
    setting("notify.dnd", 1); state(true, "manual", 0, 0); setting("notify.dnd", 0);
    clock_at(11, 23, 30);
    CHECK(BY_HAND("contacts.put", "{\"name\":\"SYNTH contact\",\"number\":\"00000000991\"}") && g_rc.status == INK_RC_DONE, "seed contact through public capability");
    call_number(CONTACT, true); call_number(STRANGER, false);
    /* Advance virtual monotonic time by ten minutes; calendar checks inject wall time. */
    delay_ms(600000); call_number(STRANGER, true);
    model_set("{\"calls\":1,\"repeat\":false}"); call_number("00000000993", true);
    model_set("{\"calls\":0}"); call_number(CONTACT, false);
    model_set("{\"calls\":3}"); call_number(CONTACT, false);
    model_set("{\"calls\":2,\"repeat\":true}");
    ink_alarm_t alarm = {.kind = INK_ALARM, .enabled = true, .at_utc = ink_time_now_s() + 1}; snprintf(alarm.owner, sizeof alarm.owner, "owner");
    CHECK(ink_alarm_add(&alarm) > 0 && WAIT_FOR(!strcmp(active_state(), "alarm"), 5000), "alarm fires during DND");
    CHECK(ringing(), "alarm always rings"); CHECK(BY_HAND("alarm.dismiss", NULL) && g_rc.status == INK_RC_DONE, "dismiss alarm");
    CHECK(WAIT_FOR(!strcmp(active_state(), "none"), 3000), "alarm closes");
    clock_at(12, 12, 0); model_set("{\"calendar\":true}");
    ink_alarm_t busy = {.kind = INK_REMINDER, .enabled = true, .at_utc = ink_time_now_s() + 60, .duration_s = 3600, .silent = true}; snprintf(busy.owner, sizeof busy.owner, "owner");
    CHECK(ink_alarm_add(&busy) > 0, "add bounded reminder");
    clock_at(12, 12, 30); state(true, "calendar", 13, 1);
    clock_at(12, 13, 1); state(false, "off", 0, 0); CHECK(ink_alarm_cancel(busy.id) == 0, "remove busy reminder");
    busy = (ink_alarm_t){.kind = INK_REMINDER, .enabled = true, .at_utc = ink_time_now_s() + 60, .duration_s = 3600, .all_day = true, .silent = true}; snprintf(busy.owner, sizeof busy.owner, "owner");
    CHECK(ink_alarm_add(&busy) > 0, "add all-day reminder"); clock_at(12, 13, 30); state(false, "off", 0, 0); CHECK(ink_alarm_cancel(busy.id) == 0, "all-day reminder never activates DND");
    model_set("{\"calendar\":false}"); clock_at(11, 23, 30);
    ink_notify_t high = {.title = "SYNTH_HIGH", .prio = INK_NOTIFY_HIGH}; double alerts = sim_metric_value("notify.alerted");
    CHECK(ink_notify_post(&high, NULL) == 0 && sim_metric_value("notify.alerted") == alerts, "scheduled DND suppresses high-priority notification sound");
    clock_at(12, 12, 0); state(false, "off", 0, 0); CHECK(AI("notify.dnd_get", NULL), "drain clock events"); int before = dnd_events;
    clock_at(12, 12, 1); CHECK(AI("notify.dnd_get", NULL), "drain unchanged minute");
    CHECK(dnd_events == before, "an unchanged minute emits no DND event and cannot keep the hub awake");
}
static void members(bool restart) {
    clock_at(11, 23, 30); state(true, "schedule", 7, 0);
    if (!restart) CHECK(BY_HAND("notify.replies_set", "{\"index\":1,\"text\":\"合成大人回复\"}") && g_rc.status == INK_RC_DONE, "adult custom reply");
    CHECK(ink_member_switch("kid") == 0 && WAIT_FOR(!strcmp(ink_member_current()->id, "kid"), 3000), "switch to child");
    CHECK(AI("notify.dnd_get", NULL), "load child's independent preferences");
    if (!restart) {
        state(false, "off", 0, 0); CHECK(!ink_setting_get("notify.dnd"), "child does not inherit old adult manual switch");
        model_set("{\"slot\":0,\"days\":62,\"start\":1200,\"end\":360,\"calls\":0}");
        CHECK(BY_HAND("notify.replies_set", "{\"index\":1,\"text\":\"合成孩子回复\"}") && g_rc.status == INK_RC_DONE, "child custom reply");
        setting("notify.dnd", 1);
    }
    CHECK(AI("notify.dnd_get", NULL) && strstr(g_result, "合成孩子回复") && strstr(g_result, "\"start\":1200") && !strstr(g_result, "合成大人回复"), "child settings isolated and durable"); state(true, "manual", 0, 0);
    ink_member_test_reset(); CHECK(WAIT_FOR(!strcmp(ink_member_current()->id, "owner"), 3000), "return to adult");
    CHECK(AI("notify.dnd_get", NULL) && strstr(g_result, "合成大人回复") && strstr(g_result, "\"start\":1320") && !strstr(g_result, "合成孩子回复"), "adult settings isolated and durable"); state(true, "schedule", 7, 0);
}
static void replies(void) {
    clock_at(12, 12, 0); setting("notify.dnd", 0);
    CHECK(BY_HAND("notify.replies_set", "{\"index\":1,\"text\":\"好的\"}") && g_rc.status == INK_RC_DONE, "restore first Pebble default for send test");
    snprintf(g_hw.num, sizeof g_hw.num, "%s", STRANGER); snprintf(g_hw.text, sizeof g_hw.text, "SYNTH inbound"); hw(HW_SMS, 0, 0);
    CHECK(WAIT_FOR(find_notice(STRANGER), 10000) && !strcmp(notice.reply_cap, "sms.send") && strstr(notice.reply_args, STRANGER), "received SMS has exact reply target");
    uint32_t id = notice.id; char args[96]; snprintf(args, sizeof args, "{\"id\":%u,\"index\":1}", (unsigned)id);
    int before = sim_net_sms_sent(), logical = sim_net_sent_sms(-1, NULL, 0, NULL, 0);
    CHECK(BY_HAND("notify.reply", args) && g_rc.status == INK_RC_DONE, "touch reply returns actual sms.send receipt: %s %.80s", ink_rc_name(g_rc.status), g_rc.detail);
    char to[24], text[192];
    CHECK(sim_net_sms_sent() == before + 1 && sim_net_sent_sms(logical, to, sizeof to, text, sizeof text) == logical + 1 && !strcmp(to, STRANGER) && !strcmp(text, "好的"), "SMS provider receives exact peer and first quick reply");
    ink_confirm_view_t confirms[1]; CHECK(ink_confirm_list(confirms, 1) == 0, "touch creates no confirmation card");
    CHECK(AI("notify.reply", args) && g_rc.status == INK_RC_PENDING_CONFIRMATION && sim_net_sms_sent() == before + 1, "unsolicited AI reply stops at original SMS policy gate");
    CHECK(ink_confirm_list(confirms, 1) == 1 && !strcmp(confirms[0].cap, "sms.send"), "confirmation names the real external capability");
    if (ink_confirm_list(confirms, 1) == 1) CHECK(ink_confirm_decide(confirms[0].token, false, INK_SRC_TOUCH) == 0, "reject AI send");
    snprintf(args, sizeof args, "{\"id\":%u,\"text\":\"合成自由回复\"}", (unsigned)id);
    CHECK(BY_HAND("notify.reply", args) && g_rc.status == INK_RC_DONE, "free text also forwards");
    CHECK(BY_HAND("notify.dismiss", args) && g_rc.status == INK_RC_DONE && !find_notice(STRANGER), "dismiss deletes only notification");
    CHECK(AI("message.list", NULL) && strstr(g_rc.say, "SYNTH inbound"), "original SMS remains after notification deletion");
    CHECK(find_notice("未接来电"), "a missed call is available for action");
    snprintf(args, sizeof args, "{\"id\":%u,\"index\":1}", (unsigned)notice.id);
    CHECK(BY_HAND("notify.act", args) && g_rc.status == INK_RC_DONE, "missed call action forwards to call.dial");
    CHECK(BY_HAND("call.hangup", NULL), "hang up synthetic outbound call");
    ink_notify_action_t acts[3] = {{.label = "SYNTH 1", .cap = "notify.dnd_get"}, {.label = "SYNTH 2", .cap = "notify.dnd_get"}, {.label = "SYNTH 3", .cap = "notify.dnd_get"}};
    ink_notify_t n = {.title = "SYNTH_THREE", .actions = acts, .nactions = 3, .prio = INK_NOTIFY_LOW};
    CHECK(ink_notify_post(&n, &id) == 0 && find_notice("SYNTH_THREE") && notice.nactions == 3 && !strcmp(notice.actions[2].label, "SYNTH 3"), "all three actions round-trip");
    snprintf(args, sizeof args, "{\"id\":%u,\"index\":3}", (unsigned)id);
    CHECK(AI("notify.act", args) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"active\":"), "AI receives real result of third action");
    CHECK(BY_HAND("notify.act", args) && g_rc.status == INK_RC_DONE && strstr(g_result, "\"active\":"), "touch uses identical third action path");
    CHECK(AI("notify.list", NULL) && strstr(g_result, "SYNTH 3"), "AI notification list exposes multiple actions");
    CHECK(BY_HAND("notify.replies_set", "{\"index\":1,\"text\":\"合成大人回复\"}") && g_rc.status == INK_RC_DONE, "retain custom reply for restart");
}
static void run(void *arg) {
    (void)arg; vtk_host_call(get_phase, NULL);
    CHECK(WAIT_FOR(ink_member_space_open(), 10000), "member opens"); test_start_home();
    CHECK(WAIT_FOR(!ink_ui_busy() && !ink_display_busy(), 10000), "boot display settles");
    CHECK(ink_cap_subscribe("notify.dnd_changed", dnd_changed, NULL, INK_EXEC_DIRECT) == 0, "observe DND changes");
    if (!strcmp(phase, "cross")) {
        ink_notify_dnd_t d;
        setting("time.auto", 0); setting("notify.dnd", 0); model_set("{\"slot\":0,\"days\":62,\"start\":1320,\"end\":420}");
        clock_at(11, 23, 30); state(true, "schedule", 7, 0);
        ink_notify_dnd_state(&d); CHECK(d.active, "23:30 matches the overnight period");
        clock_at(12, 6, 59); state(true, "schedule", 7, 0);
        ink_notify_dnd_state(&d); CHECK(d.active, "06:59 matches the previous day's overnight period");
    } else if (!strcmp(phase, "seed")) seed();
    else if (!strcmp(phase, "restart")) { setting("time.auto", 0); members(true); CHECK(find_notice("SYNTH_THREE") && notice.nactions == 3, "schema-2 actions persist after reboot"); }
    else { schedules(); members(false); replies(); }
    ink_db_flush_all(); CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 15000), "all writes durable");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "notify-model-test", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("notification model test");
}
