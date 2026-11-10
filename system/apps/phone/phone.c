/* Calls use the existing modem capabilities and member data store. */
#include <stdio.h>
#include <string.h>
#include "ink/sys/app.h"
#include "ink/sys/comm.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

extern void ink_contacts_open(void);
const ink_activity_t ink_phone_page;
static const ink_activity_t RECENTS, CALL, KEYPAD;
static const ink_db_def_t HISTORY = {.name = "phone.calls", .title = "通话记录", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true};
static struct {
    char member[16], number[24];
    bool incoming, answered, active;
    uint64_t started;
    int64_t at;
} H INK_PSRAM_BSS; /* BG executor only. */
static struct {
    char member[16], number[24], call_member[16], calling[24], status[96], dial_args[64];
    char ids[4][32], numbers[4][24], details[4][96], args[4][64];
    char tones[65], tone_count[12], route[80], tone_args[12][16];
    bool selecting, active, muted;
    ink_exec_job_t refresh;
    ink_db_t *db;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    lv_obj_t *field, *notice;
    ink_ui_field_spec_t fields[6];
    ink_ui_action_spec_t actions[14];
    ink_ui_page_spec_t model;
} P INK_PSRAM_BSS; /* UI executor only. */

static struct { lv_obj_t *root, *number_label, *notice; char number[24]; bool active; } E;
static const ink_ui_overlay_t EMERGENCY;
static void emergency_draw(void);
static void emergency_close(void) {
    if (!E.root || E.active) return;
    ink_ui_overlay_remove(&EMERGENCY); lv_obj_delete(E.root); memset(&E, 0, sizeof E);
    ink_power_release(INK_NEED_AWAKE); ink_power_lock(); ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
}
static bool emergency_key(ink_key_t key, bool lng) {
    if (!lng && (key == INK_KEY_HOME || key == INK_KEY_BACK)) emergency_close();
    return true;
}
static int emergency_snapshot(char *out, size_t cap) {
    return snprintf(out, cap, "%s；%s：%.20s；%s；110、120、119、112；%s",
        TR("紧急呼叫"), TR("号码"), E.number, lv_label_get_text(E.notice), TR("请在紧急呼叫页面亲自拨号"));
}
static const ink_ui_overlay_t EMERGENCY = {.name = "PhoneEmergency", .level = 3, .snapshot = emergency_snapshot, .on_key = emergency_key};
static void emergency_open(void) {
    if (E.root) return;
    if (!E.root) {
        ink_power_hold(INK_NEED_AWAKE);
        E.number[0] = 0;
        E.root = ink_ui_box(lv_layer_sys(), 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
        lv_obj_add_flag(E.root, LV_OBJ_FLAG_CLICKABLE);
        ink_ui_overlay_add(&EMERGENCY, E.root);
    }
    emergency_draw();
}
static void emergency_reply(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    if (E.root && rc->status != INK_RC_DONE) { lv_label_set_text(E.notice, rc->say[0] ? rc->say : TR("没有完成，请重试")); ink_ui_hint_obj(E.notice, INK_REFRESH_UI); }
}
static void emergency_dial(lv_event_t *e) {
    const char *number = lv_event_get_user_data(e);
    char args[64]; snprintf(args, sizeof args, "{\"number\":\"%.20s\"}", number ? number : E.number);
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .args_json = args, .reply_exec = INK_EXEC_UI, .done = emergency_reply};
    ink_cap_call("phone.emergency", NULL, &o);
}
static void emergency_draw(void) {
    lv_obj_clean(E.root);
    ink_ui_line(E.root, TR("紧急呼叫"), INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_STATUS_BAR, INK_CANVAS_W - 2 * INK_MARGIN);
    E.number_label = ink_ui_line(E.root, E.number, INK_FONT_TITLE, INK_WEIGHT_MEDIUM, INK_MARGIN, 140, INK_CANVAS_W - 2 * INK_MARGIN);
    E.notice = ink_ui_notice(E.root, E.active ? TR("正在拨号") : TR("仅限紧急号码"), 218);
    if (E.active) {
        ink_shell_bind(ink_ui_pill(E.root, TR("挂断"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY,
            INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, NULL, NULL), "call.hangup", NULL);
    } else {
        static const char *const keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
        int cell = (INK_CANVAS_W - 2 * INK_MARGIN) / 3;
        for (unsigned i = 0; i < sizeof keys / sizeof *keys; i++) {
            char args[24]; snprintf(args, sizeof args, "{\"key\":\"%s\"}", keys[i]);
            lv_obj_t *b = ink_ui_pill(E.root, keys[i], INK_MARGIN + (i % 3) * cell + (cell - INK_DIGIT) / 2, 270 + (i / 3) * (INK_DIGIT + INK_GAP_S),
                INK_DIGIT, INK_DIGIT, false, NULL, NULL);
            ink_ui_pill_text(b, INK_FONT_HEADING, INK_WEIGHT_MEDIUM); ink_shell_bind(b, "phone.emergency_key", args);
        }
        ink_ui_pill(E.root, TR("拨号"), INK_MARGIN, 964, cell - INK_GAP, INK_CONTROL, true, emergency_dial, NULL);
        ink_shell_bind(ink_ui_pill(E.root, TR("删除"), INK_MARGIN + cell, 964, cell - INK_GAP, INK_CONTROL, false, NULL, NULL), "phone.emergency_key", "{\"key\":\"delete\"}");
        ink_shell_bind(ink_ui_pill(E.root, TR("返回"), INK_MARGIN + 2 * cell, 964, cell - INK_GAP, INK_CONTROL, false, NULL, NULL), "phone.emergency_close", NULL);
        static const char *const numbers[] = {"110", "120", "119", "112"};
        int w = (INK_CANVAS_W - 2 * INK_MARGIN - 3 * INK_GAP_S) / 4;
        for (unsigned i = 0; i < sizeof numbers / sizeof *numbers; i++)
            ink_ui_pill(E.root, numbers[i], INK_MARGIN + i * (w + INK_GAP_S), INK_CANVAS_H - INK_MARGIN - INK_CONTROL, w, INK_CONTROL, false, emergency_dial, (void *)numbers[i]);
    }
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
}
static void emergency_action(ink_call_t *c) {
    if (!strcmp(c->cap->id, "phone.emergency_open")) { emergency_open(); return; }
    if (!E.root || strcmp(ink_ui_top_name(), "PhoneEmergency") || E.active) { c->rc.status = INK_RC_REJECTED; return; }
    if (!strcmp(c->cap->id, "phone.emergency_close")) { emergency_close(); return; }
    char key[8] = ""; ink_args_get_str(&c->args, "key", key, sizeof key);
    size_t n = strlen(E.number);
    if (!strcmp(key, "delete")) { if (n) E.number[n - 1] = 0; }
    else if (strlen(key) == 1 && strchr("0123456789*#", key[0]) && n < 20) { E.number[n] = key[0]; E.number[n + 1] = 0; }
    else { c->rc.status = INK_RC_INVALID; return; }
    lv_label_set_text(E.number_label, E.number); ink_ui_content_changed(); ink_ui_hint_obj(E.number_label, INK_REFRESH_TEXT);
}
static bool emergency_changed(const char *id, const ink_args_t *a) {
    bool emergency = false; ink_args_get_bool(a, "emergency", &emergency);
    if (!strcmp(id, "call.dialed") && emergency) {
        emergency_open(); ink_args_get_str(a, "number", E.number, sizeof E.number); E.active = true; emergency_draw(); return true;
    }
    if (!E.active) return false;
    if (!strcmp(id, "call.connected")) { lv_label_set_text(E.notice, TR("通话中")); ink_ui_content_changed(); ink_ui_hint_obj(E.notice, INK_REFRESH_TEXT); }
    if (!strcmp(id, "call.ended")) { E.active = false; emergency_close(); }
    return true;
}
void ink_shell_emergency_button(lv_obj_t *root, bool strip) {
    lv_obj_t *b = ink_ui_pill(root, TR("紧急呼叫"), strip ? 0 : INK_MARGIN,
        strip ? 0 : INK_STATUS_BAR + INK_GAP, 2 * INK_CONTROL + 2 * INK_GAP, INK_CONTROL, false, NULL, NULL);
    ink_ui_pill_text(b, INK_FONT_NOTE, INK_WEIGHT_MEDIUM); ink_shell_bind(b, "phone.emergency_open", NULL);
}

static void show(const ink_activity_t *page) {
    ink_ui_drop_app("phone");
    ink_ui_push(page, NULL);
}
static void resume_call(void) {
    if (P.active && !strcmp(P.call_member, ink_member_current()->id) && ink_member_space_open()) show(&CALL);
}
static void refresh_history(void *db) {
    if (P.db == db && !strcmp(P.member, ink_member_current()->id) && ink_member_space_open() &&
        !strcmp(ink_ui_page_name(), RECENTS.name)) ink_ui_redraw();
}
static void on_call(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    if (!strcmp(id, "call.incoming") || !strcmp(id, "call.dialed")) {
        H.incoming = !strcmp(id, "call.incoming"); H.answered = false; H.active = true;
        ink_args_get_str(a, "member", H.member, sizeof H.member);
        ink_args_get_str(a, H.incoming ? "from" : "number", H.number, sizeof H.number);
        H.at = ink_time_now_s(); H.started = ink_now_us();
    } else if (!strcmp(id, "call.answered") || !strcmp(id, "call.connected")) {
        H.answered = true; H.started = ink_now_us();
    } else if (!strcmp(id, "call.ended") && H.active) {
        H.active = false;
        ink_db_t *db = ink_db_open_member(&HISTORY, H.member);
        if (!db) return;
        char raw[256]; ink_jw_t w; ink_jw_init(&w, raw, sizeof raw);
        ink_jw_raw(&w, "{\"number\":"); ink_jw_str(&w, H.number);
        ink_jw_raw(&w, ",\"kind\":"); ink_jw_str(&w, H.incoming ? H.answered ? "打来" : "未接" : "打出");
        ink_jw_raw(&w, ",\"minutes\":"); ink_jw_int(&w, H.answered ? (int64_t)((ink_now_us() - H.started) / 60000000) : 0);
        ink_jw_raw(&w, ",\"at\":"); ink_jw_int(&w, H.at); ink_jw_raw(&w, "}");
        ink_db_write_t wr = {.sort = H.at, .data = raw, .len = w.len, .src = INK_SRC_SYSTEM};
        if (ink_db_put(db, &wr, NULL) == INK_DB_OK) {
            ink_db_flush(db); ink_exec_post(INK_EXEC_UI, &P.refresh, refresh_history, db);
        }
        ink_db_close(db);
    }
}
static void changed(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    if (emergency_changed(id, a)) return;
    char member[16] = ""; ink_args_get_str(a, "member", member, sizeof member);
    if (!strcmp(id, "call.controls")) {
        if (!P.active || strcmp(member, P.call_member)) return;
        ink_args_get_bool(a, "muted", &P.muted);
        ink_comm_call_tones(P.tones, sizeof P.tones);
    }
    if (!strcmp(id, "contacts.selected")) {
        if (!P.selecting || strcmp(P.member, member) || strcmp(member, ink_member_current()->id)) return;
        ink_args_get_str(a, "number", P.number, sizeof P.number); P.selecting = false; show(&ink_phone_page); return;
    }
    if (!strcmp(id, "call.incoming") || !strcmp(id, "call.dialed")) {
        P.tones[0] = 0; P.muted = false;
        P.active = true;
        snprintf(P.call_member, sizeof P.call_member, "%s", member);
        ink_args_get_str(a, !strcmp(id, "call.incoming") ? "from" : "number", P.calling, sizeof P.calling);
        snprintf(P.status, sizeof P.status, "%s", !strcmp(id, "call.incoming") ? "来电" : "正在拨号");
    } else if (!strcmp(id, "call.answered") || !strcmp(id, "call.connected")) snprintf(P.status, sizeof P.status, "通话中");
    else if (!strcmp(id, "call.ended")) { P.active = false; P.tones[0] = 0; P.muted = false; snprintf(P.status, sizeof P.status, "通话已结束"); }
    else if (strcmp(id, "call.controls")) return;
    if (!strcmp(P.call_member, ink_member_current()->id) && ink_member_space_open()) {
        if (!strcmp(id, "call.dialed")) show(&CALL);
        else if (!strcmp(ink_ui_page_name(), CALL.name) || !strcmp(ink_ui_page_name(), KEYPAD.name)) ink_ui_redraw();
    }
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || !ink_ui_page_current()) return;
    if (P.notice && rc->status != INK_RC_DONE) lv_label_set_text(P.notice, rc->say[0] ? rc->say : "没有完成，请重试");
    ink_shell_airplane_card(rc);
    if (rc->status == INK_RC_DONE && !strcmp(ink_ui_page_name(), RECENTS.name)) ink_ui_redraw();
}
static void request(const char *cap, const ink_args_t *a) {
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .member = P.member, .reply_exec = INK_EXEC_UI,
        .done = reply, .ctx = (void *)(uintptr_t)ink_ui_generation()};
    if (ink_cap_call(cap, a, &o) != 0 && P.notice) lv_label_set_text(P.notice, "没有完成，请重试");
}
static void control(lv_event_t *e) {
    const ink_ui_action_spec_t *a = &P.actions[(intptr_t)lv_event_get_user_data(e)];
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .member = P.member, .args_json = a->args_json, .reply_exec = INK_EXEC_UI,
        .done = reply, .ctx = (void *)(uintptr_t)ink_ui_generation()};
    ink_cap_call(a->capability, NULL, &o);
}
static void dial(lv_event_t *e) {
    (void)e;
    ink_args_t *a = ink_app_alloc(sizeof *a, true);
    if (a) { ink_args_init(a); ink_args_str(a, "number", ink_textfield_text(P.field)); request("call.dial", a); ink_app_free(a); }
}
static void navigate(ink_call_t *c) {
    const char *id = c->cap->id;
    if (strcmp(P.member, ink_member_current()->id)) { P.number[0] = 0; memset(&P.position, 0, sizeof P.position); snprintf(P.member, sizeof P.member, "%s", ink_member_current()->id); }
    if (!strcmp(id, "phone.return")) { ink_shell_close(); resume_call(); }
    else if (!strcmp(id, "phone.keypad")) {
        if (!P.active || strcmp(P.call_member, P.member)) { c->rc.status = INK_RC_STALE; return; }
        show(!strcmp(ink_ui_page_name(), KEYPAD.name) ? &CALL : &KEYPAD);
    }
    else if (!strcmp(id, "phone.contacts") || !strcmp(id, "contacts.open")) { P.selecting = !strcmp(id, "phone.contacts"); ink_contacts_open(); }
    else if (!strcmp(id, "phone.recents")) show(&RECENTS);
    else if (!strcmp(id, "phone.open")) {
        char number[24] = ""; ink_args_get_str(&c->args, "number", number, sizeof number);
        snprintf(P.number, sizeof P.number, "%s", number); show(&ink_phone_page);
    } else if (!strcmp(id, "phone.number")) { if (!P.field || ink_textfield_short_focus(P.field)) c->rc.status = INK_RC_STALE; }
    else if (P.list) ink_ui_list_turn(P.list, !strcmp(id, "phone.next") ? 1 : -1);
}
static void digit(lv_event_t *e) {
    const char *value = lv_event_get_user_data(e);
    ink_args_t *a = ink_app_alloc(sizeof *a, true);
    if (!a) return;
    ink_args_init(a); ink_args_str(a, "key", value); request("phone.key", a); ink_app_free(a);
}
static void key_cap(ink_call_t *c) {
    char key[8] = ""; ink_args_get_str(&c->args, "key", key, sizeof key);
    size_t n = strlen(P.number);
    if (!strcmp(key, "delete")) { if (n) P.number[n - 1] = 0; }
    else if (strlen(key) == 1 && strchr("0123456789*#+", key[0]) && n < 20) { P.number[n] = key[0]; P.number[n + 1] = 0; }
    else { c->rc.status = INK_RC_INVALID; return; }
    ink_ui_redraw();
}
static const ink_ui_action_spec_t FOCUS = {.id = "number", .label = "输入号码", .capability = "phone.number", .enabled = true};
static const ink_ui_action_spec_t PAGES[] = {
    {.id = "prev", .label = "上一页", .capability = "phone.prev"},
    {.id = "next", .label = "下一页", .capability = "phone.next"},
    {.id = "dial", .label = "拨号", .capability = "phone.open", .enabled = true}
};
static const ink_ui_page_spec_t LIST_MODEL = {.id = "PhoneRecents", .title = "最近通话", .object = {"phone.calls", 1}, .actions = PAGES, .action_count = 3};
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    ink_db_cursor_t cur = {.desc = true}; ink_rec_t rec = {0}; char raw[256] = {0}, kind[16] = "";
    for (size_t k = 0; k <= index; k++) if (ink_db_next(P.db, &cur, &rec, raw, sizeof raw - 1) != 1) return;
    unsigned at = index % 4; int64_t minutes = 0, time = 0;
    ink_json_t *j = ink_app_alloc(sizeof *j, true);
    if (!j || ink_json_parse(j, raw, strlen(raw))) { ink_app_free(j); return; }
    ink_json_str(j, ink_json_find(j, 0, "number"), P.numbers[at], sizeof P.numbers[at]);
    ink_json_str(j, ink_json_find(j, 0, "kind"), kind, sizeof kind);
    ink_json_int(j, ink_json_find(j, 0, "minutes"), &minutes); ink_json_int(j, ink_json_find(j, 0, "at"), &time); ink_app_free(j);
    ink_tm_t tm; ink_time_to_local(time, &tm);
    snprintf(P.details[at], sizeof P.details[at], "%s · %lld 分钟  %02d:%02d", kind, (long long)minutes, tm.hour, tm.min);
    snprintf(P.ids[at], sizeof P.ids[at], "call:%u", (unsigned)rec.id);
    char number[sizeof P.numbers[0]]; /* both live in P: copy the number out before writing into P */
    snprintf(number, sizeof number, "%s", P.numbers[at]);
    snprintf(P.args[at], sizeof P.args[at], "{\"number\":\"%s\"}", number);
    lv_obj_set_y(slot, ink_ui_px(at * INK_ROW_DOUBLE));
    lv_obj_set_style_bg_opa(slot, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(slot, 0, 0);
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP, INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP);
        ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP, INK_LINE_LIST + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP);
    }
    lv_label_set_text(lv_obj_get_child(slot, 0), P.numbers[at]); lv_label_set_text(lv_obj_get_child(slot, 1), P.details[at]);
    *field = (ink_ui_field_spec_t){.id = P.ids[at], .role = "listitem", .label = P.numbers[at], .value = P.details[at], .object = {P.ids[at], rec.ver}};
    *action = (ink_ui_action_spec_t){.id = P.ids[at], .label = "用这个号码拨号", .capability = "phone.open", .args_json = P.args[at], .object = field->object, .enabled = true};
}
static void tabs(lv_obj_t *screen, bool recent) {
    static const char *const labels[] = {"拨号", "最近", "联系人"};
    lv_obj_t *seg = ink_ui_segments(screen, labels, 3, recent ? 1 : 0, INK_MARGIN + INK_CONTROL + INK_GAP,
        INK_STATUS_BAR + INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP, NULL, NULL);
    const char *caps[] = {"phone.open", "phone.recents", "phone.contacts"};
    for (int i = 0; i < 3; i++) ink_shell_bind(lv_obj_get_child(seg, i), caps[i], "{}");
}
static void entered(void *ctx, const char *value) { (void)ctx; snprintf(P.number, sizeof P.number, "%s", value); }
static void edited(lv_event_t *e) { entered(NULL, ink_textfield_text(lv_event_get_target_obj(e))); }
static void call_route_changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (!strcmp(P.call_member, ink_member_current()->id) && ink_member_space_open() &&
        (!strcmp(ink_ui_page_name(), CALL.name) || !strcmp(ink_ui_page_name(), KEYPAD.name))) ink_ui_redraw();
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    if (strcmp(P.member, ink_member_current()->id)) {
        P.number[0] = 0; P.selecting = false; memset(&P.position, 0, sizeof P.position);
        snprintf(P.member, sizeof P.member, "%s", ink_member_current()->id);
    }
    bool keypad = !strcmp(ink_ui_page_name(), KEYPAD.name);
    bool recent = !strcmp(ink_ui_page_name(), RECENTS.name), call = keypad || !strcmp(ink_ui_page_name(), CALL.name);
    ink_shell_statusbar(screen, true);
    if (!call) { ink_shell_topbar(screen, ""); tabs(screen, recent); }
    P.fields[0] = (ink_ui_field_spec_t){.id = "status", .role = "status", .label = "通话记录", .value = "正在读取通话记录"};
    P.model = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = call ? "通话" : recent ? "最近通话" : "拨号", .object = {"phone", 1}, .fields = P.fields, .field_count = 1, .actions = P.actions};
    P.notice = ink_ui_notice(screen, "", INK_CANVAS_H - INK_PRIMARY - INK_CONTROL - INK_GAP_L);
    if (call) {
        bool owned = !strcmp(P.call_member, P.member), enabled = owned && P.active;
        char route[32] = ""; int64_t connected = 0;
        ink_state_get_str(ink_state_find("audio.route"), route, sizeof route);
        ink_state_get_int(ink_state_find("bt.connected"), &connected);
        bool headset = !strcmp(route, "bt_headset");
        snprintf(P.route, sizeof P.route, "%s", headset ? "声音走耳机" : "声音走喇叭");
        lv_obj_set_y(P.notice, ink_ui_px(keypad ? 800 : 500));
        if (!keypad) ink_ui_circle(screen, "person", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_STATUS_BAR + INK_BLOCK, INK_CIRCLE_LARGE, INK_ICON_STATE, INK_SURFACE_STATIC, NULL, NULL);
        lv_obj_t *name = ink_ui_line(screen, owned ? P.calling : "没有通话", INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN, keypad ? 90 : INK_STATUS_BAR + INK_BLOCK + INK_CIRCLE_LARGE + INK_MARGIN, INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_t *status = ink_ui_notice(screen, owned ? P.status : "", keypad ? 165 : INK_STATUS_BAR + INK_BLOCK + INK_CIRCLE_LARGE + INK_MARGIN + INK_LINE_TITLE);
        lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_t *hang = ink_ui_circle(screen, "callEnd", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_CANVAS_H - INK_MARGIN - INK_CIRCLE_LARGE - INK_LINE_NOTE, INK_CIRCLE_LARGE, INK_ICON_APP, INK_SURFACE_BLACK, NULL, NULL);
        ink_shell_bind(hang, "call.hangup", "{}");
        lv_obj_t *end = ink_ui_notice(screen, "挂断", INK_CANVAS_H - INK_MARGIN - INK_LINE_NOTE); lv_obj_set_style_text_align(end, LV_TEXT_ALIGN_CENTER, 0);
        P.fields[0] = (ink_ui_field_spec_t){.id = "number", .role = "text", .label = "号码", .value = owned ? P.calling : ""};
        P.fields[1] = (ink_ui_field_spec_t){.id = "status", .role = "status", .label = "通话状态", .value = owned ? P.status : "没有通话"}; P.model.field_count = 2;
        P.fields[2] = (ink_ui_field_spec_t){.id = "mute", .role = "status", .label = "麦克风", .value = !owned ? "没有通话" : P.muted ? "已静音" : "未静音"};
        P.fields[3] = (ink_ui_field_spec_t){.id = "route", .role = "status", .label = "声音去向", .value = P.route};
        P.fields[4] = (ink_ui_field_spec_t){.id = "tones", .role = "text", .label = "已发送的按键", .value = owned ? P.tones : "", .secret = true};
        snprintf(P.tone_count, sizeof P.tone_count, "%u", owned ? (unsigned)strlen(P.tones) : 0);
        P.fields[5] = (ink_ui_field_spec_t){.id = "tone_count", .role = "text", .label = "已发送的按键位数", .value = P.tone_count}; P.model.field_count = 6;
        P.actions[0] = (ink_ui_action_spec_t){.id = "hangup", .label = "挂断", .capability = "call.hangup", .enabled = enabled};
        P.actions[1] = (ink_ui_action_spec_t){.id = "keypad", .label = keypad ? "收起键盘" : "键盘", .capability = "phone.keypad", .enabled = enabled}; P.model.action_count = 2;
        if (keypad) {
            static const char *const keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
            lv_obj_t *typed = ink_ui_notice(screen, owned ? P.tones : "", 270); lv_obj_set_style_text_align(typed, LV_TEXT_ALIGN_CENTER, 0);
            for (int i = 0; i < 12; i++) {
                snprintf(P.tone_args[i], sizeof P.tone_args[i], "{\"key\":\"%s\"}", keys[i]);
                ink_ui_pill(screen, keys[i], INK_MARGIN + (i % 3) * 210 + 50, 350 + (i / 3) * 110, INK_CONTROL, INK_CONTROL, false, control, (void *)(intptr_t)(i + 2));
                P.actions[i + 2] = (ink_ui_action_spec_t){.id = keys[i], .label = keys[i], .capability = "call.dtmf", .args_json = P.tone_args[i], .enabled = enabled};
            }
            P.model.action_count = 14;
            ink_shell_bind(ink_ui_pill(screen, "收起键盘", INK_MARGIN, 945, 200, INK_CONTROL, false, NULL, NULL), "phone.keypad", "{}");
        } else {
            const char *labels[] = {P.muted ? "已静音" : "静音", "键盘", connected ? "耳机" : "耳机未连", "录音"};
            const char *icons[] = {"mute", "keypad", "headphones", "mic"};
            const char *caps[] = {"call.mute", "phone.keypad", connected ? "call.route" : "shell.bluetooth", "call.record"};
            const char *args[] = {P.muted ? "{\"on\":false}" : "{\"on\":true}", "{}", headset ? "{\"headset\":false}" : "{\"headset\":true}", "{}"};
            for (int i = 0; i < 4; i++) {
                int x = INK_MARGIN + i * 160;
                ink_ui_circle(screen, icons[i], x, 653, 125, INK_ICON_LIST, (i == 0 && P.muted) || (i == 2 && headset) ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, control, (void *)(intptr_t)(i + 1));
                lv_obj_t *text = ink_ui_line(screen, labels[i], INK_FONT_NOTE, INK_WEIGHT_MEDIUM, x, 790, 125); lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_add_flag(text, LV_OBJ_FLAG_CLICKABLE); lv_obj_add_event_cb(text, control, LV_EVENT_CLICKED, (void *)(intptr_t)(i + 1));
                P.actions[i + 1] = (ink_ui_action_spec_t){.id = icons[i], .label = labels[i], .capability = caps[i], .args_json = args[i], .enabled = enabled};
            }
            P.model.action_count = 5;
            lv_obj_t *route_label = ink_ui_notice(screen, P.route, 850); lv_obj_set_style_text_align(route_label, LV_TEXT_ALIGN_CENTER, 0);
            P.fields[4] = (ink_ui_field_spec_t){.id = "recording", .role = "status", .label = "录音", .value = "通话音频接线未核实，暂不能录音"};
        }
    } else if (recent) {
        P.db = ink_db_open_cached_member(&HISTORY, P.member);
        if (!P.db) {
            lv_label_set_text(P.notice, "正在读取通话记录");
            ink_args_t *a = ink_app_alloc(sizeof *a, true);
            if (a) { ink_args_init(a); ink_args_str(a, "collection", HISTORY.name); request("data.list", a); ink_app_free(a); }
            return;
        }
        lv_obj_t *body = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L, INK_CANVAS_W, INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_L, 0, INK_SURFACE_WHITE, false);
        int count = ink_db_count(P.db);
        if (count) ink_ui_card(body, INK_MARGIN, 0, INK_CANVAS_W - 2 * INK_MARGIN, (count < 4 ? count : 4) * INK_ROW_DOUBLE, INK_SURFACE_STATIC);
        P.list = ink_ui_list_create(body, &LIST_MODEL, &P.position, 4, 1, INK_ROW_DOUBLE, "还没有通话记录", bind_item, NULL);
        if (P.list) ink_ui_list_refresh(P.list, (size_t)count);
    } else {
        P.actions[0] = FOCUS;
        P.fields[0] = (ink_ui_field_spec_t){.id = "number", .role = "textbox", .label = "号码"};
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .initial = P.number, .hint = "输入号码", .max = 20};
        P.field = ink_textfield_create(screen, &cfg, entered, NULL, false);
        lv_obj_set_pos(P.field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L));
        lv_obj_set_width(P.field, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
        ink_textfield_bind_short(P.field, &P.model, P.fields, &FOCUS);
        lv_obj_add_event_cb(P.field, edited, LV_EVENT_VALUE_CHANGED, NULL);
        static const char *const keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
        int cell = (INK_CANVAS_W - 2 * INK_MARGIN) / 3, y = INK_CANVAS_H - INK_MARGIN - INK_DIGIT - 4 * (INK_DIGIT + INK_GAP_S);
        for (int i = 0; i < 12; i++) {
            lv_obj_t *b = ink_ui_pill(screen, keys[i], INK_MARGIN + (i % 3) * cell + (cell - INK_DIGIT) / 2, y + (i / 3) * (INK_DIGIT + INK_GAP_S), INK_DIGIT, INK_DIGIT, false, digit, (void *)keys[i]);
            ink_ui_pill_text(b, INK_FONT_HEADING, INK_WEIGHT_MEDIUM);
        }
        ink_ui_circle(screen, "phone", (INK_CANVAS_W - INK_DIGIT) / 2, INK_CANVAS_H - INK_MARGIN - INK_DIGIT, INK_DIGIT, INK_ICON_APP, INK_SURFACE_BLACK, dial, NULL);
        ink_ui_pill(screen, "删除", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, INK_CANVAS_H - INK_MARGIN - INK_DIGIT + INK_GAP_L, INK_CONTROL, INK_CONTROL, false, digit, "delete");
    }
}
static void destroy(void) { ink_db_close(P.db); P.db = NULL; P.list = NULL; P.field = P.notice = NULL; }
static int snapshot(char *out, size_t cap) {
    if (P.list) return ink_ui_list_snapshot(P.list, out, cap);
    if (P.field) {
        ink_jw_t w; ink_jw_init(&w, P.dial_args, sizeof P.dial_args);
        ink_jw_raw(&w, "{\"number\":"); ink_jw_str(&w, ink_textfield_text(P.field)); ink_jw_raw(&w, "}");
        P.actions[1] = (ink_ui_action_spec_t){.id = "dial", .label = "拨号", .capability = "call.dial", .args_json = P.dial_args, .enabled = !w.overflow && P.fields[0].value && P.fields[0].value[0]};
        P.actions[2] = (ink_ui_action_spec_t){.id = "recents", .label = "最近", .capability = "phone.recents", .enabled = true};
        P.actions[3] = (ink_ui_action_spec_t){.id = "contacts", .label = "联系人", .capability = "phone.contacts", .enabled = true};
        P.actions[4] = (ink_ui_action_spec_t){.id = "delete", .label = "删除一位", .capability = "phone.key", .args_json = "{\"key\":\"delete\"}", .enabled = true}; P.model.action_count = 5;
        return ink_textfield_short_snapshot(P.field, out, cap);
    }
    return ink_ui_semantics_snapshot(&P.model, out, cap);
}
static bool page_key(ink_key_t k, bool lng) { return P.list && ink_ui_list_key(P.list, k, lng); }
const ink_activity_t ink_phone_page = {.name = "Phone", .app = "phone", .create = create, .destroy = destroy, .snapshot = snapshot, .settle = resume_call};
static const ink_activity_t RECENTS = {.name = "PhoneRecents", .app = "phone", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t CALL = {.name = "PhoneCall", .app = "phone", .create = create, .destroy = destroy, .snapshot = snapshot};
static const ink_activity_t KEYPAD = {.name = "PhoneKeypad", .app = "phone", .create = create, .destroy = destroy, .snapshot = snapshot};
static const ink_cap_t CAPS[] = {
    {.id = "phone.emergency_open", .title = "打开紧急呼叫页面", .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .run = emergency_action},
    {.id = "phone.emergency_close", .title = "返回锁屏", .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .expose = INK_EXPOSE_OWNER, .run = emergency_action},
    {.id = "phone.emergency_key", .title = "按紧急拨号键", .input = "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\",\"maxLength\":6}},\"required\":[\"key\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .expose = INK_EXPOSE_OWNER, .run = emergency_action},
    {.id = "phone.keypad", .title = "打开或收起通话键盘", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.return", .title = "回到通话", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.open", .title = "打开拨号盘，可填入号码", .input = "{\"type\":\"object\",\"properties\":{\"number\":{\"type\":\"string\",\"maxLength\":20}}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.contacts", .title = "从联系人选择号码", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "contacts.open", .title = "打开联系人", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.recents", .title = "打开最近通话", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.number", .title = "输入电话号码", .activity = "Phone", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.key", .title = "按拨号键或删除一位", .input = "{\"type\":\"object\",\"properties\":{\"key\":{\"type\":\"string\",\"maxLength\":6}},\"required\":[\"key\"]}", .activity = "Phone", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = key_cap},
    {.id = "phone.prev", .title = "通话记录上一页", .activity = "PhoneRecents", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "phone.next", .title = "通话记录下一页", .activity = "PhoneRecents", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "contacts.selected", .title = "联系人号码已选好", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true}
};
int ink_phone_init(void) {
    return ink_db_register(&HISTORY) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) ||
        ink_cap_subscribe("call.", on_call, NULL, INK_EXEC_BG) || ink_cap_subscribe("call.", changed, NULL, INK_EXEC_UI) ||
        ink_cap_subscribe("contacts.selected", changed, NULL, INK_EXEC_UI) || ink_state_watch("audio.route", call_route_changed, NULL, INK_EXEC_UI) || ink_state_watch("bt.connected", call_route_changed, NULL, INK_EXEC_UI) ? -1 : 0;
}

bool ink_phone_activity(char *title, size_t cap) {
    if (!P.active || strcmp(P.status, "通话中") || strcmp(P.call_member, ink_member_current()->id) || !ink_member_space_open()) return false;
    snprintf(title, cap, "和%s通话", P.calling);
    return true;
}
