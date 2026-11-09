/* Calls use the existing modem capabilities and member data store. */
#include <stdio.h>
#include <string.h>
#include "ink/sys/app.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

extern void ink_contacts_open(void);
const ink_activity_t ink_phone_page;
static const ink_activity_t RECENTS, CALL;
static const ink_db_def_t HISTORY = {.name = "phone.calls", .title = "通话记录", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_records = 200, .drop_oldest = true};
static struct {
    char member[16], number[24];
    bool incoming, answered, active;
    uint64_t started;
    int64_t at;
} H INK_PSRAM_BSS; /* BG executor only. */
static struct {
    char member[16], number[24], call_member[16], calling[24], status[96], dial_args[64];
    char ids[4][32], numbers[4][24], details[4][96], args[4][64];
    bool selecting;
    ink_db_t *db;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    lv_obj_t *field, *notice;
    ink_ui_field_spec_t fields[2];
    ink_ui_action_spec_t actions[5];
    ink_ui_page_spec_t model;
} P INK_PSRAM_BSS; /* UI executor only. */

static void show(const ink_activity_t *page) {
    ink_ui_drop_app("phone");
    ink_ui_push(page, NULL);
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
        ink_db_put(db, &wr, NULL); ink_db_flush(db); ink_db_close(db);
    }
}
static void changed(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    char member[16] = ""; ink_args_get_str(a, "member", member, sizeof member);
    if (!strcmp(id, "contacts.selected")) {
        if (!P.selecting || strcmp(P.member, member) || strcmp(member, ink_member_current()->id)) return;
        ink_args_get_str(a, "number", P.number, sizeof P.number); P.selecting = false; show(&ink_phone_page); return;
    }
    if (!strcmp(id, "call.incoming") || !strcmp(id, "call.dialed")) {
        snprintf(P.call_member, sizeof P.call_member, "%s", member);
        ink_args_get_str(a, !strcmp(id, "call.incoming") ? "from" : "number", P.calling, sizeof P.calling);
        snprintf(P.status, sizeof P.status, "%s", !strcmp(id, "call.incoming") ? "来电" : "正在拨号");
    } else if (!strcmp(id, "call.answered") || !strcmp(id, "call.connected")) snprintf(P.status, sizeof P.status, "通话中");
    else if (!strcmp(id, "call.ended")) snprintf(P.status, sizeof P.status, "通话已结束");
    else return;
    if (!strcmp(P.call_member, ink_member_current()->id) && ink_member_space_open()) {
        if (!strcmp(id, "call.dialed")) show(&CALL);
        else if (!strcmp(ink_ui_page_name(), CALL.name)) ink_ui_redraw();
    }
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || !ink_ui_page_current()) return;
    if (P.notice && rc->status != INK_RC_DONE) lv_label_set_text(P.notice, "没有完成，请检查号码后重试");
    if (rc->status == INK_RC_DONE && !strcmp(ink_ui_page_name(), RECENTS.name)) ink_ui_redraw();
}
static void request(const char *cap, const ink_args_t *a) {
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .member = P.member, .reply_exec = INK_EXEC_UI,
        .done = reply, .ctx = (void *)(uintptr_t)ink_ui_generation()};
    if (ink_cap_call(cap, a, &o) != 0 && P.notice) lv_label_set_text(P.notice, "没有完成，请重试");
}
static void dial(lv_event_t *e) {
    (void)e;
    ink_args_t *a = ink_app_alloc(sizeof *a, true);
    if (a) { ink_args_init(a); ink_args_str(a, "number", ink_textfield_text(P.field)); request("call.dial", a); ink_app_free(a); }
}
static void navigate(ink_call_t *c) {
    const char *id = c->cap->id;
    if (strcmp(P.member, ink_member_current()->id)) { P.number[0] = 0; memset(&P.position, 0, sizeof P.position); snprintf(P.member, sizeof P.member, "%s", ink_member_current()->id); }
    if (!strcmp(id, "phone.contacts") || !strcmp(id, "contacts.open")) { P.selecting = !strcmp(id, "phone.contacts"); ink_contacts_open(); }
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
    snprintf(P.args[at], sizeof P.args[at], "{\"number\":\"%s\"}", P.numbers[at]);
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
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    if (strcmp(P.member, ink_member_current()->id)) {
        P.number[0] = 0; P.selecting = false; memset(&P.position, 0, sizeof P.position);
        snprintf(P.member, sizeof P.member, "%s", ink_member_current()->id);
    }
    bool recent = !strcmp(ink_ui_page_name(), RECENTS.name), call = !strcmp(ink_ui_page_name(), CALL.name);
    ink_shell_statusbar(screen, true);
    if (!call) { ink_shell_topbar(screen, ""); tabs(screen, recent); }
    P.model = (ink_ui_page_spec_t){.id = call ? "PhoneCall" : "Phone", .title = call ? "通话" : "拨号", .object = {"phone", 1}, .fields = P.fields, .field_count = 1, .actions = P.actions};
    P.notice = ink_ui_notice(screen, "", INK_CANVAS_H - INK_PRIMARY - INK_CONTROL - INK_GAP_L);
    if (call) {
        bool owned = !strcmp(P.call_member, P.member);
        ink_ui_circle(screen, "person", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_STATUS_BAR + INK_BLOCK, INK_CIRCLE_LARGE, INK_ICON_STATE, INK_SURFACE_STATIC, NULL, NULL);
        lv_obj_t *name = ink_ui_line(screen, owned ? P.calling : "没有通话", INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_STATUS_BAR + INK_BLOCK + INK_CIRCLE_LARGE + INK_MARGIN, INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
        ink_ui_notice(screen, owned ? P.status : "", INK_STATUS_BAR + INK_BLOCK + INK_CIRCLE_LARGE + INK_MARGIN + INK_LINE_TITLE);
        ink_ui_notice(screen, "静音、通话键盘、耳机和录音暂不可用", INK_CANVAS_H - 2 * INK_CIRCLE_LARGE - INK_MARGIN);
        lv_obj_t *hang = ink_ui_circle(screen, "callEnd", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_CANVAS_H - INK_MARGIN - INK_CIRCLE_LARGE, INK_CIRCLE_LARGE, INK_ICON_APP, INK_SURFACE_BLACK, NULL, NULL);
        ink_shell_bind(hang, "call.hangup", "{}");
        P.fields[0] = (ink_ui_field_spec_t){.id = "number", .role = "text", .label = "号码", .value = owned ? P.calling : ""};
        P.fields[1] = (ink_ui_field_spec_t){.id = "status", .role = "status", .label = "通话状态", .value = owned ? P.status : "没有通话"}; P.model.field_count = 2;
        P.actions[0] = (ink_ui_action_spec_t){.id = "hangup", .label = "挂断", .capability = "call.hangup", .enabled = owned}; P.model.action_count = 1;
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
        static const char *const keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"};
        int cell = (INK_CANVAS_W - 2 * INK_MARGIN) / 3, y = INK_CANVAS_H - INK_MARGIN - INK_DIGIT - 4 * (INK_DIGIT + INK_GAP_M);
        for (int i = 0; i < 12; i++) {
            lv_obj_t *b = ink_ui_pill(screen, keys[i], INK_MARGIN + (i % 3) * cell + (cell - INK_DIGIT) / 2, y + (i / 3) * (INK_DIGIT + INK_GAP_M), INK_DIGIT, INK_DIGIT, false, digit, (void *)keys[i]);
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
        P.actions[1] = (ink_ui_action_spec_t){.id = "dial", .label = "拨号", .capability = "call.dial", .args_json = P.dial_args, .enabled = P.fields[0].value && P.fields[0].value[0]};
        P.actions[2] = (ink_ui_action_spec_t){.id = "recents", .label = "最近", .capability = "phone.recents", .enabled = true};
        P.actions[3] = (ink_ui_action_spec_t){.id = "contacts", .label = "联系人", .capability = "phone.contacts", .enabled = true}; P.model.action_count = 4;
        return ink_textfield_short_snapshot(P.field, out, cap);
    }
    return ink_ui_semantics_snapshot(&P.model, out, cap);
}
static bool page_key(ink_key_t k, bool lng) { return P.list && ink_ui_list_key(P.list, k, lng); }
const ink_activity_t ink_phone_page = {.name = "Phone", .app = "phone", .create = create, .destroy = destroy, .snapshot = snapshot};
static const ink_activity_t RECENTS = {.name = "PhoneRecents", .app = "phone", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t CALL = {.name = "PhoneCall", .app = "phone", .create = create, .destroy = destroy, .snapshot = snapshot};
static const ink_cap_t CAPS[] = {
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
        ink_cap_subscribe("contacts.selected", changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
