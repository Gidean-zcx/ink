/*
 * What the AI can reach, on the whole product, against the table in the design (system services design §1.3; R-061):
 * the tool list as the hub gets it, page by page; state, settings; events reaching the hub; the state that goes with a
 * question. Every capability registered must be in one of the two tables here: for the AI, or not with the reason why.
 * More, fewer, or no title and the test fails, so that nothing new can stay out of the AI's sight by accident.
 * Results are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "ink/sys/ai.h"
#include "ink/sys/confirm.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/notify.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

/* What the AI can call, on its own page where it says so, and what each must be given: the names it cannot do without,
 * "" when all it takes is optional, NULL when it takes nothing. Without these the model has only a name to go on. */
static const struct {
    const char *id, *args;
} FOR_AI[] = {
    {"games.start", ""}, {"games.state", NULL}, {"games.move", "row,col"}, {"games.undo", NULL}, {"games.reset", NULL},
    {"system.report_problem", ""},
    {"kid.policy_get", ""}, {"kid.app_set", "id,app,mode"}, {"kid.period_set", "id,index,start,end,days"},
    {"family.kid_apps", NULL}, {"family.kid_periods", ""}, {"family.kid_member", NULL},
    {"phone.return", NULL}, {"shell.tasks", NULL}, {"tasks.prev", NULL}, {"tasks.next", NULL},
    {"backup.open", NULL}, {"backup.status", ""}, {"backup.manifest", ""}, {"backup.now", NULL}, {"restore.preview", ""}, {"restore.apply", "preview"},
    {"factory.open", NULL}, {"factory.media", "clear_media"}, {"factory.finish", NULL},
    {"pairing.status", NULL}, {"pairing.configure", "url"}, {"pairing.reject", "txid"},
    {"pairing.restart", NULL}, {"pairing.import", NULL}, {"pairing.hotspot_start", NULL}, {"pairing.hotspot_stop", NULL},
    {"caps.domains", NULL}, {"caps.find", "query"}, {"system.undo", "object"}, {"system.home", NULL}, {"system.full_refresh", NULL},
    {"system.set_volume", "percent"}, {"system.switch_member", "id"}, {"system.shutdown", NULL}, {"system.factory_reset", ""},
    {"system.reboot", NULL}, {"state.list", NULL}, {"settings.list", NULL}, {"settings.get", "id"}, {"settings.set", "id,value"},
    {"power.set_saving", "enabled"}, {"net.set_airplane", "enabled"}, {"wifi.set_enabled", "enabled"},
    {"net.apn_set", "automatic"}, {"net.roaming_set", "enabled"}, {"net.usage", NULL}, {"net.usage_reset", NULL}, {"shell.mobile", NULL},
    {"data.collections", NULL}, {"data.list", "collection"}, {"data.get", "collection,id"}, {"data.put", "collection,value"},
    {"data.delete", "collection,id"}, {"data.status", "collection"}, {"job.list", NULL}, {"job.cancel", "id"}, {"file.list", "path"}, {"file.stat", "path"}, {"file.read", "path"}, {"file.write", "path,text"},
    {"file.mkdir", "path"}, {"file.copy", "from,to"}, {"file.move", "from,to"}, {"file.delete", "path"}, {"file.free", NULL}, {"usb.status", NULL},
    {"file.index", "path"}, {"file.lookup", "id"}, {"file.thumb", "id"},
    {"file.batch", "op,to"}, {"file.delete_many", ""},
    {"files.new", ""}, {"files.edit", NULL}, {"files.multi", NULL}, {"files.mark", "id"}, {"files.all", NULL},
    {"transfer.start", NULL}, {"transfer.stop", NULL}, {"transfer.status", NULL},
    {"usb.mount", NULL}, {"usb.unmount", NULL}, {"time.now", NULL}, {"time.set", "utc_s"}, {"time.lunar", ""}, {"alarm.set", "hour,minute"},
    {"alarm.list", NULL}, {"alarm.cancel", "id"}, {"alarm.dismiss", ""}, {"alarm.snooze", ""}, {"reminder.add", "at,title"}, {"reminder.list", NULL},
    {"reminder.cancel", "id"}, {"notify.post", "title"}, {"notify.list", ""}, {"notify.read", ""}, {"notify.dismiss", "id"}, {"notify.clear", NULL},
    {"reminder.update", "id,at,title,repeat"}, {"reminder.new", NULL}, {"reminder.open", "id"},
    {"reminder.time", ""}, {"calendar.list", "from,until"}, {"reminder.focus", NULL}, {"reminder.draft", ""}, {"reminder.save", NULL},
    {"notify.channels", NULL}, {"notify.set_channel", "id,enabled"}, {"interrupt.list", NULL}, {"interrupt.ack", NULL}, {"confirm.list", NULL},
    {"confirm.withdraw", "token"}, {"confirm.status", ""}, {"app.list", NULL}, {"app.open", "id"}, {"app.close", "id"}, {"app.cards", NULL}, {"text.state", NULL},
    {"app.page_action", "id"}, {"app.page_turn", "direction"},
    {"text.insert", "text"}, {"text.select", "start"}, {"text.undo", ""}, {"text.redo", ""}, {"text.submit", ""}, {"clipboard.read", NULL}, {"clipboard.write", "text"}, {"media.play", "path"},
    {"text.page", "direction"}, {"text.ask_ai", ""}, {"text.add_word", ""},
    {"media.pause", NULL}, {"media.resume", NULL}, {"media.seek", "ms"}, {"media.stop", NULL}, {"media.status", NULL}, {"audio.record", ""},
    {"audio.record_stop", NULL}, {"bt.scan", NULL}, {"bt.list", NULL}, {"bt.pair", "addr"}, {"bt.connect", "addr"}, {"bt.disconnect", NULL},
    {"bt.forget", "addr"}, {"shell.bluetooth", NULL}, {"shell.bluetooth_prev", NULL}, {"shell.bluetooth_next", NULL},
    {"wifi.scan", NULL}, {"wifi.list", NULL}, {"wifi.connect", "ssid"}, {"wifi.forget", "ssid"}, {"member.list", NULL},
    {"member.add", "id"}, {"member.remove", "id"}, {"camera.capture", NULL}, {"message.send", "to"}, {"message.list", NULL},
    {"call.dial", "number"}, {"call.answer", NULL}, {"call.hangup", NULL}, {"remote.open", ""}, {"reader.list_books", NULL},
    {"phone.emergency_open", NULL},
    {"call.mute", "on"}, {"call.dtmf", "key"}, {"call.route", "headset"}, {"call.record", NULL}, {"phone.keypad", NULL},
    {"reader.open_sample", NULL}, {"shell.update_notes_page", "delta"},
    {"reader.open_book", "title"}, {"reader.next_page", NULL}, {"reader.prev_page", NULL}, {"reader.goto_page", "page"}, {"reader.read_page", NULL}, {"reader.continue", NULL},
    {"reader.get_layout", NULL}, {"reader.set_layout", ""}, {"reader.menu", ""},
    {"reader.chapters", NULL}, {"reader.show_chapters", NULL}, {"reader.goto_chapter", ""}, {"reader.list_next", NULL}, {"reader.list_prev", NULL},
    {"activity.list", ""}, {"activity.clear", "member"}, {"update.status", NULL}, {"update.check", NULL}, {"update.download", NULL},
    {"text.voice_start", ""}, {"text.voice_finish", ""}, {"text.voice_cancel", ""},
    {"shell.control", NULL}, {"shell.close", NULL}, {"shell.back", NULL}, {"shell.members", NULL}, {"shell.lock", NULL},
    {"shell.more", NULL}, {"shell.settings", NULL}, {"shell.display", NULL}, {"shell.sound", NULL},
    {"shell.network", NULL}, {"shell.known_networks", NULL}, {"shell.notifications", NULL}, {"shell.notify_settings", NULL}, {"home.next", NULL}, {"home.prev", NULL},
    {"shell.today_edit", NULL}, {"shell.icon_menu", "id"}, {"shell.today_card", "card"},
    {"shell.today_prev", NULL}, {"shell.today_next", NULL},
    {"shell.voice", NULL}, {"shell.storage", NULL}, {"shell.about", NULL}, {"shell.privacy", NULL},
    {"shell.settings_prev", NULL}, {"shell.settings_next", NULL},
    {"shell.device", NULL}, {"shell.licenses", NULL}, {"shell.license", "index"},
    {"shell.time", NULL}, {"shell.time_edit", NULL}, {"shell.time_zones", NULL}, {"shell.return", NULL},
    {"shell.time_step", "field,delta"}, {"shell.time_prev", NULL}, {"shell.time_next", NULL},
    {"shell.launch", "id"}, {"shell.resume", NULL}, {"ai.panel.hide", NULL}, {"ai.panel.listen", NULL},
    {"ai.panel.next", NULL}, {"ai.panel.prev", NULL},
    {"ai.panel.replay", NULL}, {"ai.history.list", ""}, {"ai.history.open", "id"}, {"ai.history.delete", "id"},
    {"ai.history.clear", NULL}, {"ai.history.prev", NULL}, {"ai.history.next", NULL}, {"photo.ask", NULL},
    {"keyboard.key", "code"}, {"keyboard.pick", "index"}, {"keyboard.candidates", ""}, {"keyboard.edit", "action"},
    {"shell.search", "query"}, {"shell.page_next", NULL}, {"shell.page_prev", NULL}, {"shell.notify_next", NULL}, {"shell.notify_prev", NULL},
    {"search.query", "query"}, {"search.show", "query"}, {"search.open", "id,version"},
    {"search.focus", NULL}, {"search.next", NULL}, {"search.prev", NULL},
    {"shell.wifi_next", NULL}, {"shell.wifi_prev", NULL}, {"shell.wifi_pick", "index"},
    {"file_picker.prev", NULL}, {"file_picker.next", NULL}, {"file_picker.cancel", NULL}, {"file_picker.select", "id"},
    {"contacts.put", "name,number"}, {"contacts.delete", "id,if_revision,name,number"}, {"contacts.select", "id"}, {"contacts.edit", ""},
    {"contacts.next", NULL}, {"contacts.prev", NULL}, {"contacts.name", NULL}, {"contacts.number", NULL}, {"contacts.pick", NULL}, {"contacts.search", NULL}, {"contacts.view", "id"}, {"contacts.note", NULL},
    {"calc.eval", "expr"}, {"calc.key", "key"},
    {"clock.prev", NULL}, {"clock.next", NULL}, {"clock.new", NULL}, {"clock.open", "id"},
    {"clock.draft", ""}, {"clock.focus", NULL}, {"clock.save", NULL},
    {"clock.timer", NULL}, {"clock.timer_set", "seconds"}, {"clock.timer_start", NULL},
    {"clock.timer_pause", NULL}, {"clock.timer_cancel", NULL}, {"clock.timer_dismiss", "id"},
    {"clock.stopwatch", NULL}, {"clock.stopwatch_start", NULL}, {"clock.stopwatch_pause", NULL},
    {"clock.stopwatch_lap", NULL}, {"clock.stopwatch_reset", NULL}, {"clock.extra_prev", NULL}, {"clock.extra_next", NULL},
    {"clock.world", NULL}, {"clock.world_pick", NULL}, {"clock.world_add", "zone"}, {"clock.world_remove", "zone"},
    {"alarm.update", "id"},
    {"shell.ai_home", NULL}, {"shell.ai_ask", NULL}, {"shell.ai_send", NULL}, {"shell.ai_stop", NULL}, {"shell.ai_key", ""},
    {"phone.open", ""}, {"phone.contacts", NULL}, {"contacts.open", NULL}, {"phone.recents", NULL},
    {"phone.number", NULL}, {"phone.key", "key"}, {"phone.prev", NULL}, {"phone.next", NULL},
    {"sms.send", "to"}, {"sms.thread", "peer"}, {"sms.compose", ""}, {"sms.to", NULL}, {"sms.pick", NULL}, {"sms.retry", "id"}, {"sms.recipient", "id"},
    {"sms.text", NULL}, {"sms.long", NULL}, {"sms.prev", NULL}, {"sms.next", NULL},
    {"memo.save", "id,draft,page"}, {"memo.delete", "id"}, {"memo.undo", "id"}, {"memo.convert", "id"}, {"memo.check", "id,index"},
    {"memo.open", "id"}, {"memo.edit", ""}, {"memo.next", NULL}, {"memo.prev", NULL}, {"memo.retry", NULL},
    {"memo.record", NULL}, {"memo.play", "id"}, {"audio.record_cancel", NULL}, {"audio.transcribe", "path"},
    {"shell.family", NULL}, {"shell.kid_settings", NULL}, {"shell.wallpaper", NULL}, {"shell.records", ""}, {"shell.ai_memory", NULL},
    {"family.edit", ""}, {"family.focus", NULL}, {"family.save", NULL}, {"family.role", NULL},
    {"family.prev", NULL}, {"family.next", NULL}, {"wallpaper.choose", NULL}, {"member.rename", "id,name"},
    {"calendar.open", NULL}, {"calendar.prev", NULL}, {"calendar.next", NULL}, {"calendar.month_prev", NULL},
    {"calendar.month_next", NULL}, {"calendar.select", "day"},
    {"weather.get", NULL}, {"weather.open", NULL}, {"weather.city", NULL}, {"weather.focus", NULL}, {"weather.find", "name"}, {"weather.set_city", "id"},
    {"shell.member_choose", NULL}, {"shell.pin_set", NULL},
    {"shell.pin_forgot", NULL}, {"shell.pin_parent", NULL}, {"shell.pin_cancel", NULL},
    {"shell.update", NULL}, {"shell.update_close", NULL},
    {"files.browse", "path"}, {"files.select", "id"}, {"files.move", ""}, {"files.name", NULL},
    {"files.preview", "id"}, {"files.next", NULL}, {"files.prev", NULL}, {"files.up", NULL},
    {"camera.preview", NULL}, {"camera.photo", NULL}, {"camera.review", NULL}, {"gallery.prev", NULL}, {"gallery.next", NULL}, {"gallery.reload", NULL}, {"photo.open", "id"},
    {"camera.scan", NULL}, {"camera.scan_apply", NULL},
    {"shell.ai_log", ""}, {"shell.ai_receipts", ""}, {"ai.pages.close", NULL}, {"ai.pages.retry", NULL},
    {"ai.pages.next", NULL}, {"ai.pages.prev", NULL},
    {"remote.address", NULL}, {"remote.focus", NULL}, {"remote.submit", NULL}, {"remote.retry", NULL},
    {"remote.close", NULL}, {"remote.prev", NULL}, {"remote.next", NULL}, {"remote.pointer", "x,y"},
    {"member.time_get", ""}, {"member.time_set", "minutes"}, {"member.time_add", "minutes"},
    {"shell.notify_open", "id"}, {"shell.kid_allow", "id"}, {"shell.kid_parent", NULL},
    {"shell.kid_add", NULL}, {"shell.kid_sms", NULL}, {"shell.kid_phone", NULL}, {"shell.kid_cancel", NULL},
    {"file.usage", NULL}, {"file.cleanup_preview", NULL}, {"file.cache_clear", "revision"}, {"file.cleanup", "revision"},
    {"transfer.open", NULL}, {"transfer.usb", NULL},
    {"shell.runtime_records", NULL},
    {NULL, NULL},
};

/* What it cannot, and why. */
static const struct {
    const char *id, *why, *args;
} NOT_FOR_AI[] = {
    {"system.export_logs", "raw core dumps stay on the device owner's storage card", NULL},
    {"wifi.restore", "credentials are restored only through system.undo", "revision"},
    {"shell.mobile_edit", "only a person edits the local credential draft; AI uses net.apn_set", NULL},
    {"shell.mobile_save", "only a person submits the local credential draft", NULL},
    {"phone.emergency", "D-119: only a person dials; the AI may open the page", "number"},
    {"phone.emergency_key", "only a person enters emergency dialler keys", "key"},
    {"phone.emergency_close", "only a person leaves the emergency dialler", NULL},
    {"phone.emergency_hangup", "only a person ends an emergency call", NULL},
    {"shell.member_select", "only touch and keys may select an identity", "id"},
    {"shell.members_page", "member selection stays on the device before adult authentication", "delta"},
    {"pairing.accept", "only a person confirms joining a household", "txid"},
    {"confirm.accept", "only a person on the device can say yes", "token"},
    {"confirm.reject", "only a person on the device answers", "token"},
    {"confirm.page", "only a person reads the mandatory confirmation pages", "token,delta"},
    {"member.unlock", "a PIN is entered on the device, it goes through nobody else", "id,pin"},
    {"member.set_pin", "a PIN is entered on the device, it goes through nobody else", "id,new_pin"},
    {"system.upgrade", "a person installs: it may fetch and tell, the device is gone for minutes (design §12.7)", NULL},
    {"system.upgrade_from_card", "a person installs: it may fetch and tell, the device is gone for minutes (design §12.7)", ""},
    {"shell.pin_submit", "only the person submits the local PIN buffer", NULL},
    {"memo.voice", "the memo app's own step: a finished recording or the hub's words, never typed in by the model", ""},
    {NULL, NULL, NULL},
};

static const char *const STATES[] = {
    "hub.online",
    "cell.roaming", "net.mobile_rev",
    "power.saving",
    "pairing.state",
    "file.index_state",
    "storage.user_free", "storage.free", "storage.card", "storage.mode", "usb.connected", "time.synced", "time.zone", "time.minute", "alarm.ringing", "notify.unread", "interrupt.active",
    "confirm.pending", "app.foreground", "app.cards_rev", "power.state", "battery.level", "battery.charging", "battery.low", "member.current", "text.focused", "audio.now_playing", "audio.route", "audio.listening", "audio.mic", "audio.recording", "job.running", "media.state", "bt.connected", "bt.device", "bt.available", "wifi.state", "wifi.ssid", "member.authed", "update.state", "update.version", "update.progress", "text.voice", NULL,
};

static const char *const SETTINGS[] = {"notify.dnd", "time.zone", "time.auto", "power.lock_after_s", "audio.volume", "audio.child_max", "voice.handsfree", "ai.read_clipboard", "member.idle_return_s", "update.auto_download", NULL};

/* Events the design promises the AI hears about (the rest of the table by the same rule: registered is forwarded). */
static const char *const EVENTS[] = {"backup.changed", "backup.failed", "pairing.changed", "data.write_failed", "file.changed", "file.job_done", "job.finished", "usb.changed", "alarm.fired", "reminder.fired", "time.changed", "notify.posted", "confirm.result", "app.state", "battery.low", "media.finished", "audio.record_done", "bt.changed", "wifi.changed", "member.changed", "update.available", "update.result", "text.voice_ended", "call.dialed", "call.connected", "contacts.selected", NULL};

static int for_ai(const char *id) {
    for (int i = 0; FOR_AI[i].id; i++)
        if (!strcmp(FOR_AI[i].id, id)) return i;
    return -1;
}

static int not_for_ai(const char *id) {
    for (int i = 0; NOT_FOR_AI[i].id; i++)
        if (!strcmp(NOT_FOR_AI[i].id, id)) return i;
    return -1;
}

/* The tool list, as the hub gets it: every page parsed, every entry kept. */
static cJSON *g_tools;
static const char *g_method;
static char g_params[128];

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
    g_tools = cJSON_CreateObject();
    char cursor[64] = "{}";
    int pages = 0;
    for (;;) {
        const char *reply = ask_device("tools/list", cursor);
        CHECK(reply != NULL, "the device answers tools/list");
        if (!reply) return;
        cJSON *page = cJSON_Parse(reply);
        const cJSON *result = cJSON_GetObjectItem(page, "result"), *tool;
        CHECK(cJSON_IsArray(cJSON_GetObjectItem(result, "tools")), "page %d is whole JSON with its tools", pages);
        cJSON_ArrayForEach(tool, cJSON_GetObjectItem(result, "tools")) {
            const cJSON *name = cJSON_GetObjectItem(tool, "name");
            CHECK(cJSON_IsString(name) && !cJSON_GetObjectItem(g_tools, name->valuestring), "a tool once, with a name");
            cJSON_AddItemToObject(g_tools, cJSON_IsString(name) ? name->valuestring : "?", cJSON_Duplicate(tool, 1));
        }
        const cJSON *next = cJSON_GetObjectItem(result, "nextCursor");
        pages++;
        if (cJSON_IsString(next)) snprintf(cursor, sizeof cursor, "{\"cursor\":\"%s\"}", next->valuestring);
        cJSON_Delete(page);
        if (!next) break;
    }
    CHECK(pages >= 2, "more than fits one page, so it comes in pages (%d)", pages);
}

/* What it says it takes against what the table says it must be given (audit AUD15-009: without this a capability could lose
 * its description and the test would not notice). */
static void takes(const char *id, const cJSON *schema, const char *args) {
    CHECK(cJSON_IsObject(schema), "%s: there is a schema", id);
    CHECK(cJSON_IsString(cJSON_GetObjectItem(schema, "type")) && !strcmp(cJSON_GetObjectItem(schema, "type")->valuestring, "object"), "%s: its arguments are an object", id);
    const cJSON *props = cJSON_GetObjectItem(schema, "properties"), *req = cJSON_GetObjectItem(schema, "required"), *r;
    if (!args) {
        CHECK(!cJSON_GetArraySize(props), "%s takes nothing: it says nothing else", id);
        return;
    }
    CHECK(cJSON_GetArraySize(props) > 0, "%s takes something: it says what", id);
    int want = 0;
    char name[32];
    for (const char *p = args; *p; want++) {
        const char *e = strchr(p, ',');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        snprintf(name, sizeof name, "%.*s", (int)n, p);
        bool listed = false;
        cJSON_ArrayForEach(r, req) listed = listed || !strcmp(r->valuestring, name);
        CHECK(listed && cJSON_GetObjectItem(props, name), "%s cannot do without %s: it says so", id, name);
        p += n + (e != NULL);
    }
    CHECK(cJSON_GetArraySize(req) == want, "%s: nothing else is required (%d, %d)", id, cJSON_GetArraySize(req), want);
}

static void the_tools(void) {
    fetch_tools();
    /* every capability is accounted for */
    int callable = 0;
    for (int i = 0; ink_cap_at(i); i++) {
        const ink_cap_t *c = ink_cap_at(i);
        if (c->form == INK_FORM_EVENT) {
            CHECK(!cJSON_GetObjectItem(g_tools, c->id), "%s is an event, not a callable tool", c->id);
            continue;
        }
        int yes = for_ai(c->id), no = not_for_ai(c->id);
        CHECK((yes >= 0) != (no >= 0), "%s must be in exactly one table: for the AI, or not with the reason", c->id);
        CHECK(c->title && c->title[0], "%s has a title", c->id);
        const cJSON *tool = cJSON_GetObjectItem(g_tools, c->id);
        bool here = !c->activity; /* what belongs to a page is listed on that page */
        if (yes >= 0 && here) {
            CHECK(tool != NULL, "%s is in the tool list the hub gets", c->id);
            const cJSON *title = cJSON_GetObjectItem(tool, "title");
            CHECK(cJSON_IsString(title) && title->valuestring[0], "%s: with a title there", c->id);
            takes(c->id, cJSON_GetObjectItem(tool, "inputSchema"), FOR_AI[yes].args);
        } else {
            if (no >= 0) CHECK(!tool, "%s must not be in it (%s)", c->id, NOT_FOR_AI[no].why);
            cJSON *own = cJSON_Parse(c->input ? c->input : "{\"type\":\"object\"}"); /* not listed from here: what it declares */
            takes(c->id, own, yes >= 0 ? FOR_AI[yes].args : no >= 0 ? NOT_FOR_AI[no].args : NULL);
            cJSON_Delete(own);
        }
        if (yes >= 0) callable++;
    }
    /* nothing in the tables that is not there */
    for (int i = 0; FOR_AI[i].id; i++) CHECK(ink_cap_find(FOR_AI[i].id) != NULL, "%s is in the table but not registered", FOR_AI[i].id);
    for (int i = 0; NOT_FOR_AI[i].id; i++) CHECK(ink_cap_find(NOT_FOR_AI[i].id) != NULL, "%s is in the table but not registered", NOT_FOR_AI[i].id);
    CHECK(callable > 50, "the table is not empty (%d)", callable);
    /* what the design promises for finding things */
    CHECK(AI("caps.domains", NULL) && strstr(g_result, "notify") && strstr(g_result, "file") && strstr(g_result, "app"), "a directory of domains (%s)", g_result);
    CHECK(AI("caps.find", "{\"query\":\"通知\"}") && strstr(g_result, "notify.list"), "and a search (%s)", g_result);
    cJSON_Delete(g_tools);
    g_tools = NULL;
}

/* A record as large as there can be, all the way to the hub: a tool's answer as the model gets it (audit AUD15-004). */
static const ink_db_def_t BIG = {.name = "test.big", .title = "大记录", .scope = INK_DB_DEVICE, .schema = 1, .json = true, .ai_read = true};

static void a_large_answer(void) {
    CHECK(ink_db_register(&BIG) == 0, "registered");
    ink_db_t *db = ink_db_open(&BIG, NULL);
    static char v[INK_DB_RECORD_MAX];
    size_t n = (size_t)snprintf(v, sizeof v, "{\"text\":\"");
    while (n + 3 < sizeof v) v[n++] = 'x';
    v[n++] = '"';
    v[n++] = '}';
    uint32_t id = 0;
    ink_db_write_t w = {.data = v, .len = n};
    CHECK(db && ink_db_put(db, &w, &id) == INK_DB_OK, "the largest there can be");
    ink_db_close(db);
    char params[96];
    snprintf(params, sizeof params, "{\"name\":\"data.get\",\"arguments\":{\"collection\":\"test.big\",\"id\":%u}}", (unsigned)id);
    const char *reply = ask_device("tools/call", params);
    cJSON *j = cJSON_Parse(reply ? reply : "");
    const cJSON *receipt = cJSON_GetObjectItem(j, "result"), *value = cJSON_GetObjectItem(cJSON_GetObjectItem(receipt, "result"), "value");
    const cJSON *text = cJSON_GetObjectItem(value, "text");
    CHECK(cJSON_IsString(text) && strlen(text->valuestring) == n - 11, "the hub gets all of it, as JSON (%zu)", reply ? strlen(reply) : 0);
    cJSON_Delete(j);
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
    a_large_answer();
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
