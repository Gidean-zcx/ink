/*
 * What the AI can reach, on the whole product, against the table in the design (system services design §1.3; R-055):
 * the tool list as the hub gets it, page by page; state, settings; events reaching the hub; the state that goes with a
 * question. Every capability registered must be in one of the two tables here: for the AI, or not with the reason why.
 * More, fewer, or no title and the test fails, so that nothing new can stay out of the AI's sight by accident.
 * Results are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/sys/ai.h"
#include "ink/sys/confirm.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/notify.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

/* What the AI can call. On its own page where it says so. */
static const char *const FOR_AI[] = {
    "caps.domains", "caps.find", "system.undo", "system.home", "system.full_refresh", "system.set_volume", "system.switch_member", "system.shutdown", "system.factory_reset",
    "state.list", "settings.list", "settings.get", "settings.set",
    "data.collections", "data.list", "data.get", "data.put", "data.delete",
    "file.list", "file.stat", "file.read", "file.write", "file.mkdir", "file.copy", "file.move", "file.delete", "file.free", "usb.status", "usb.mount", "usb.unmount",
    "time.now", "time.set", "time.lunar", "alarm.set", "alarm.list", "alarm.cancel", "alarm.dismiss", "alarm.snooze", "reminder.add", "reminder.list", "reminder.cancel",
    "notify.post", "notify.list", "notify.read", "notify.dismiss", "notify.clear", "notify.channels", "notify.set_channel", "interrupt.list", "interrupt.ack",
    "confirm.list", "confirm.withdraw",
    "app.list", "app.open", "app.close", "app.cards",
    "text.state", "text.insert", "text.submit", "clipboard.read", "clipboard.write",
    "media.play", "media.pause", "media.resume", "media.seek", "media.stop", "media.status", "audio.record", "audio.record_stop",
    "bt.scan", "bt.list", "bt.pair", "bt.connect", "bt.disconnect", "bt.forget",
    "wifi.scan", "wifi.list", "wifi.connect", "wifi.forget", "member.list", "member.add", "member.remove", "system.reboot",
    "camera.capture", "message.send", "message.list", "call.dial", "call.answer", "call.hangup", "remote.open",
    "reader.list_books", "reader.open_book", "reader.next_page", "reader.prev_page", "reader.goto_page", "reader.read_page",
    NULL,
};

/* What it cannot, and why. */
static const struct {
    const char *id, *why;
} NOT_FOR_AI[] = {
    {"confirm.accept", "only a person on the device can say yes"},
    {"confirm.reject", "only a person on the device answers"},
    {"member.unlock", "a PIN is entered on the device, it goes through nobody else"},
    {"member.set_pin", "a PIN is entered on the device, it goes through nobody else"},
    {NULL, NULL},
};

static const char *const STATES[] = {
    "storage.user_free", "storage.free", "storage.card", "storage.mode", "usb.connected", "time.synced", "time.zone", "time.minute", "alarm.ringing", "notify.unread", "interrupt.active",
    "confirm.pending", "app.foreground", "app.cards_rev", "power.state", "battery.level", "battery.charging", "battery.low", "member.current", "text.focused", "audio.now_playing", "audio.route", "audio.listening", "audio.recording", "media.state", "bt.connected", "bt.device", "bt.available", "wifi.state", "wifi.ssid", "member.authed", NULL,
};

static const char *const SETTINGS[] = {"notify.dnd", "time.zone", "time.auto", "power.lock_after_s", "audio.volume", "audio.child_max", "voice.handsfree", "ai.read_clipboard", "member.idle_return_s", NULL};

/* Events the design promises the AI hears about (the rest of the table by the same rule: registered is forwarded). */
static const char *const EVENTS[] = {"data.write_failed", "file.changed", "file.job_done", "usb.changed", "alarm.fired", "reminder.fired", "time.changed", "notify.posted", "confirm.result", "app.state", "battery.low", "media.finished", "audio.record_done", "bt.changed", "wifi.changed", "member.changed", NULL};

static bool in_list(const char *const *list, const char *id) {
    for (; *list; list++)
        if (!strcmp(*list, id)) return true;
    return false;
}

static const char *not_for_ai(const char *id) {
    for (int i = 0; NOT_FOR_AI[i].id; i++)
        if (!strcmp(NOT_FOR_AI[i].id, id)) return NOT_FOR_AI[i].why;
    return NULL;
}

/* The tool list, as the hub gets it. */
static char *g_tools;
static size_t g_tools_len;
static const char *g_method;
static char g_params[64];

static void ask_event(void *arg) {
    (void)arg;
    sim_hub_request_device(g_method, g_params);
}

static const char *ask_device(const char *method, const char *params) {
    g_method = method;
    snprintf(g_params, sizeof g_params, "%s", params);
    vtk_event_at(vtk_now() + SIM_MS(1), ask_event, NULL);
    delay_ms(5); /* it has forgotten the last answer by now */
    for (int i = 0; i < 500 && !sim_hub_device_reply(); i++) delay_ms(10);
    return sim_hub_device_reply();
}

static void fetch_tools(void) {
    g_tools = malloc(128 * 1024);
    g_tools[0] = 0;
    g_tools_len = 0;
    char cursor[64] = "{}";
    int pages = 0;
    for (;;) {
        const char *reply = ask_device("tools/list", cursor);
        CHECK(reply != NULL, "the device answers tools/list");
        if (!reply) return;
        size_t n = strlen(reply);
        if (g_tools_len + n < 128 * 1024 - 1) {
            memcpy(g_tools + g_tools_len, reply, n + 1);
            g_tools_len += n;
        }
        pages++;
        const char *next = strstr(reply, "\"nextCursor\":\"");
        if (!next) break;
        int c = atoi(next + 14);
        snprintf(cursor, sizeof cursor, "{\"cursor\":\"%d\"}", c);
    }
    CHECK(pages >= 2, "more than fits one page, so it comes in pages (%d)", pages);
}

static bool tool_listed(const char *id) {
    char needle[64];
    snprintf(needle, sizeof needle, "\"name\":\"%s\"", id);
    return strstr(g_tools, needle) != NULL;
}

static void the_tools(void) {
    fetch_tools();
    /* every capability is accounted for */
    int callable = 0;
    for (int i = 0; ink_cap_at(i); i++) {
        const ink_cap_t *c = ink_cap_at(i);
        if (c->form == INK_FORM_EVENT) continue;
        bool want = in_list(FOR_AI, c->id);
        const char *why = not_for_ai(c->id);
        CHECK(want != (why != NULL), "%s must be in exactly one table: for the AI, or not with the reason", c->id);
        CHECK(c->title && c->title[0], "%s has a title", c->id);
        if (c->input) CHECK(strstr(c->input, "\"type\":\"object\""), "%s: its arguments are a schema", c->id);
        bool here = !c->activity; /* what belongs to a page is listed on that page */
        if (want && here) CHECK(tool_listed(c->id), "%s is in the tool list the hub gets", c->id);
        if (why) CHECK(!tool_listed(c->id), "%s must not be in it (%s)", c->id, why);
        if (want) callable++;
    }
    /* nothing in the tables that is not there */
    for (int i = 0; FOR_AI[i]; i++) CHECK(ink_cap_find(FOR_AI[i]) != NULL, "%s is in the table but not registered", FOR_AI[i]);
    for (int i = 0; NOT_FOR_AI[i].id; i++) CHECK(ink_cap_find(NOT_FOR_AI[i].id) != NULL, "%s is in the table but not registered", NOT_FOR_AI[i].id);
    CHECK(callable > 50, "the table is not empty (%d)", callable);
    /* what the design promises for finding things */
    CHECK(AI("caps.domains", NULL) && strstr(g_result, "notify") && strstr(g_result, "file") && strstr(g_result, "app"), "a directory of domains (%s)", g_result);
    CHECK(AI("caps.find", "{\"query\":\"通知\"}") && strstr(g_result, "notify.list"), "and a search (%s)", g_result);
    free(g_tools);
    g_tools = NULL;
}

static void state_and_settings(void) {
    CHECK(AI("state.list", NULL) && g_rc.status == INK_RC_DONE, "state.list (%s)", g_rc.detail);
    for (int i = 0; STATES[i]; i++) {
        char needle[64];
        snprintf(needle, sizeof needle, "\"%s\":", STATES[i]);
        CHECK(strstr(g_result, needle), "state %s is there", STATES[i]);
    }
    CHECK(AI("settings.list", NULL) && g_rc.status == INK_RC_DONE, "settings.list (%s)", g_rc.detail);
    for (int i = 0; SETTINGS[i]; i++) {
        char needle[64];
        snprintf(needle, sizeof needle, "\"id\":\"%s\"", SETTINGS[i]);
        CHECK(strstr(g_result, needle), "setting %s is there", SETTINGS[i]);
    }
    for (int i = 0; EVENTS[i]; i++) {
        const ink_cap_t *c = ink_cap_find(EVENTS[i]);
        CHECK(c && c->form == INK_FORM_EVENT, "event %s is registered", EVENTS[i]);
    }
    int64_t user_free = 0;
    CHECK(ink_state_get_int(ink_state_find("storage.user_free"), &user_free) && user_free > 0, "the internal storage's free space was measured (%lld)", (long long)user_free);
}

static bool event_seen_since(int from, const char *text) {
    for (int i = from; i < sim_hub_events_seen(); i++) {
        const char *e = sim_hub_event_at(i);
        if (e && strstr(e, text)) return true;
    }
    return false;
}

static void network_event(const char *which) {
    /* the script's commands, from here */
    extern void sim_net_set_link(const char *);
    sim_net_set_link(which);
}
static void net_none(void *arg) {
    (void)arg;
    network_event("none");
}
static void net_wifi(void *arg) {
    (void)arg;
    network_event("wifi");
}

static void events_reach_the_hub(void) {
    int from = sim_hub_events_seen();
    ink_notify_t n = {.title = "给中枢看", .prio = INK_NOTIFY_LOW};
    ink_notify_post(&n, NULL);
    CHECK(WAIT_FOR(event_seen_since(from, "\"notify.posted\"") && event_seen_since(from, "给中枢看"), 2000), "an event reaches the hub with what it says (%d)", sim_hub_events_seen() - from);
    const char *first = sim_hub_event_at(from);
    CHECK(first && strstr(first, "\"at_s\":"), "and when it happened (%s)", first ? first : "");

    /* a confirmation's end is how the AI hears */
    from = sim_hub_events_seen();
    CHECK(AI("notify.clear", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "the AI asks");
    unsigned token = 0;
    sscanf(g_rc.object, "confirm:%u", &token);
    CHECK(ink_confirm_decide(token, false, INK_SRC_TOUCH) == 0, "a person says no");
    CHECK(WAIT_FOR(event_seen_since(from, "\"confirm.result\"") && event_seen_since(from, "\"rejected\""), 2000), "the AI is told (%d)", sim_hub_events_seen() - from);
    delay_ms(1200);

    /* offline: the latest eight wait */
    vtk_event_at(vtk_now() + SIM_MS(1), net_none, NULL);
    CHECK(WAIT_FOR(!ink_hub_online(), 40000), "the network goes");
    from = sim_hub_events_seen();
    for (int i = 0; i < 12; i++) {
        char title[16];
        snprintf(title, sizeof title, "离线%02d", i);
        ink_notify_t off = {.title = title, .prio = INK_NOTIFY_LOW};
        ink_notify_post(&off, NULL);
    }
    delay_ms(1000);
    CHECK(sim_hub_events_seen() == from, "nothing can go out");
    vtk_event_at(vtk_now() + SIM_MS(1), net_wifi, NULL);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 60000), "the network is back");
    CHECK(WAIT_FOR(event_seen_since(from, "离线11"), 3000), "the last went out");
    CHECK(sim_hub_events_seen() - from <= 12 && !event_seen_since(from, "离线03") && event_seen_since(from, "离线04"), "the latest eight (%d)", sim_hub_events_seen() - from);
    ink_notify_clear();
}

static ink_exec_job_t g_key_job;
static void key_job(void *pressed) { ink_ai_key(pressed != NULL, osal_now_us()); }

static void what_goes_with_a_question(void) {
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    ink_exec_post(INK_EXEC_UI, &g_key_job, key_job, (void *)1);
    delay_ms(1200);
    ink_exec_post(INK_EXEC_UI, &g_key_job, key_job, NULL);
    CHECK(WAIT_FOR(sim_hub_last_ask() != NULL, 5000), "a question");
    const char *ask = sim_hub_last_ask();
    CHECK(ask && strstr(ask, "\"state\":{") && strstr(ask, "\"battery.level\":") && strstr(ask, "\"power.state\":") && strstr(ask, "\"app.foreground\":"), "the system's state goes with it (%.400s)", ask ? ask : "");
    CHECK(ask && strstr(ask, "\"page\":{"), "and the page");
    delay_ms(6000);
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    the_tools();
    state_and_settings();
    events_reach_the_hub();
    what_goes_with_a_question();
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "surfacetest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
