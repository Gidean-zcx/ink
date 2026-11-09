/* SMS pages share the member store, capability bus and F2/F3/F4 hosts. */
#include <stdio.h>
#include <string.h>
#include "ink/probe.h"
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

static const ink_db_def_t MESSAGES = {.name = "sms.messages", .title = "短信会话", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_records = 512};
static const ink_activity_t LIST, THREAD, COMPOSE, LONG;
static struct {
    ink_db_t *db, *contacts;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    lv_obj_t *field, *notice;
    char member[16], peer[24], to[24], draft[355], result[160];
    uint32_t rows[512]; size_t count;
    char peers[512][24], ids[4][32], args[4][64], texts[4][355], labels[4][96];
    ink_ui_field_spec_t field_spec;
    ink_ui_action_spec_t actions[5];
    ink_ui_page_spec_t form;
    ink_exec_job_t refresh;
} S INK_PSRAM_BSS;

static bool read_message(ink_db_t *db, uint32_t id, char *peer, char *text, char *status) {
    char raw[1024] = {0}; ink_json_t *j = ink_app_alloc(sizeof *j, true);
    bool ok = j && ink_db_get(db, id, NULL, raw, sizeof raw - 1) == INK_DB_OK && ink_json_parse(j, raw, strlen(raw)) == 0;
    if (ok) { ink_json_str(j, ink_json_find(j, 0, "peer"), peer, 24); ink_json_str(j, ink_json_find(j, 0, "text"), text, 355); ink_json_str(j, ink_json_find(j, 0, "status"), status, 96); }
    ink_app_free(j); return ok;
}
static bool save_message(ink_db_t *db, uint32_t *id, const char *peer, const char *text, const char *status, uint8_t src) {
    char raw[1024]; ink_jw_t w; ink_jw_init(&w, raw, sizeof raw);
    ink_jw_raw(&w, "{\"peer\":"); ink_jw_str(&w, peer); ink_jw_raw(&w, ",\"text\":"); ink_jw_str(&w, text);
    ink_jw_raw(&w, ",\"status\":"); ink_jw_str(&w, status); ink_jw_raw(&w, "}");
    if (!*id) *id = ink_db_new_id(db);
    uint32_t ver = 0; ink_db_write_t wr = {.id = *id, .sort = *id, .data = raw, .len = w.len, .src = src, .written_ver = &ver};
    return !w.overflow && ink_db_put(db, &wr, NULL) == INK_DB_OK && ink_db_wait(db, ver, 3000000) == 0;
}
static void refresh(void *arg) {
    (void)arg;
    if (ink_ui_page_current() && !strcmp(S.member, ink_member_current()->id) &&
        (!strcmp(ink_ui_page_name(), LIST.name) || !strcmp(ink_ui_page_name(), THREAD.name))) ink_ui_redraw();
}
static void received(void *ctx, const char *event, const ink_args_t *a) {
    (void)ctx; (void)event; char member[16], peer[24], text[355];
    ink_args_get_str(a, "member", member, sizeof member); ink_args_get_str(a, "from", peer, sizeof peer); ink_args_get_str(a, "text", text, sizeof text);
    ink_db_t *db = ink_db_open_member(&MESSAGES, member); uint32_t id = 0;
    if (db) { if (!save_message(db, &id, peer, text, "收到", INK_SRC_SYSTEM)) ink_probe_count("sms.save_failed"); ink_db_close(db); }
    ink_exec_post(INK_EXEC_UI, &S.refresh, refresh, NULL);
}
typedef struct { ink_call_t *call; ink_db_t *db; uint32_t id; } sending_t;
static void sent(void *ctx, const ink_receipt_t *rc) {
    sending_t *s = ctx; ink_call_t *c = s->call;
    char peer[24], text[355], old[96]; read_message(s->db, s->id, peer, text, old);
    const char *status = rc->status == INK_RC_DONE ? "已发出" : rc->status == INK_RC_STALE || rc->status == INK_RC_INVALID || rc->status == INK_RC_REJECTED || strstr(rc->detail, "no time left") ? "发送失败，未发出" : "结果未知，请先核实";
    c->rc.status = rc->status; snprintf(c->rc.say, sizeof c->rc.say, "%s", status);
    if (!save_message(s->db, &s->id, peer, text, status, c->src)) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "发送结果没能存下，请先核实"); }
    snprintf(c->rc.object, sizeof c->rc.object, "sms:%u", (unsigned)s->id);
    ink_db_close(s->db); ink_app_free(s); ink_exec_post(INK_EXEC_UI, &S.refresh, refresh, NULL); ink_cap_done(c);
}
static void send_message(ink_call_t *c) {
    char peer[24], text[355]; ink_args_get_str(&c->args, "to", peer, sizeof peer); ink_args_get_str(&c->args, "text", text, sizeof text);
    bool valid = peer[0] && text[0];
    for (size_t i = 0; peer[i]; i++) if ((peer[i] < '0' || peer[i] > '9') && !(i == 0 && peer[i] == '+' && peer[1])) valid = false;
    if (!valid) { c->rc.status = INK_RC_INVALID; return; }
    sending_t *s = ink_app_alloc(sizeof *s, true);
    if (!s) { c->rc.status = INK_RC_FAILED; return; }
    *s = (sending_t){.call = c, .db = ink_db_open_member(&MESSAGES, ink_cap_call_member(c))};
    int64_t online = 0; ink_state_get_int(ink_state_find("cell.registered"), &online);
    if (!s->db || !save_message(s->db, &s->id, peer, text, online ? "结果未知，请先核实" : "发送失败，未发出", c->src)) {
        ink_db_close(s->db); ink_app_free(s); c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "短信没能存下，未发送"); return;
    }
    ink_cap_defer(c);
    ink_call_opts_t o = {.src = c->src, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_BG, .done = sent, .ctx = s};
    ink_receipt_t failure = {.status = INK_RC_REJECTED};
    if (!online || ink_cap_call("message.send", &c->args, &o) != 0) sent(s, &failure);
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || strcmp(S.member, ink_member_current()->id)) return;
    snprintf(S.result, sizeof S.result, "%s", rc->say[0] ? rc->say : rc->status == INK_RC_DONE ? "" : "没有完成，请重试");
    if (rc->status == INK_RC_DONE) ink_ui_redraw(); else if (S.notice) lv_label_set_text(S.notice, S.result);
}
static void request(const char *cap, const ink_args_t *args) {
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .member = S.member, .reply_exec = INK_EXEC_UI, .done = reply, .ctx = (void *)(uintptr_t)ink_ui_generation()};
    if (ink_cap_call(cap, args, &o) != 0 && S.notice) lv_label_set_text(S.notice, "没有完成，请重试");
}
static void selected(void *ctx, const ink_receipt_t *rc) {
    ink_call_t *c = ctx; c->rc.status = rc->status;
    if (strcmp(ink_cap_call_member(c), ink_member_current()->id) || strcmp(ink_ui_page_name(), COMPOSE.name)) c->rc.status = INK_RC_STALE;
    else if (rc->status == INK_RC_DONE && rc->result) {
        ink_json_t *j = ink_app_alloc(sizeof *j, true);
        if (j && ink_json_parse(j, rc->result, strlen(rc->result)) == 0) ink_json_str(j, ink_json_find(j, 0, "number"), S.to, sizeof S.to);
        ink_app_free(j); ink_ui_redraw();
    }
    ink_cap_done(c);
}
static void navigate(ink_call_t *c) {
    const char *id = c->cap->id;
    if (!strcmp(id, "sms.recipient")) {
        ink_cap_defer(c); ink_call_opts_t o = {.src = c->src, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_UI, .done = selected, .ctx = c};
        if (ink_cap_call("contacts.select", &c->args, &o) != 0) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
    } else if (!strcmp(id, "sms.thread")) {
        ink_args_get_str(&c->args, "peer", S.peer, sizeof S.peer); S.position.page = 0; ink_ui_push(&THREAD, NULL);
    } else if (!strcmp(id, "sms.compose")) {
        snprintf(S.to, sizeof S.to, "%s", !strcmp(ink_ui_page_name(), THREAD.name) ? S.peer : ""); S.draft[0] = S.result[0] = 0; S.position.page = 0; ink_ui_push(&COMPOSE, NULL);
    } else if (!strcmp(id, "sms.next") || !strcmp(id, "sms.prev")) { if (S.list) ink_ui_list_turn(S.list, !strcmp(id, "sms.next") ? 1 : -1); }
    else if (!strcmp(id, "sms.long")) {
        if (!S.field) c->rc.status = INK_RC_STALE;
        else { snprintf(S.draft, sizeof S.draft, "%s", ink_textfield_text(S.field)); ink_ui_push(&LONG, NULL); }
    } else if (!S.field || ink_textfield_short_focus(S.field) != 0) c->rc.status = INK_RC_STALE;
}
static void entered(void *ctx, const char *value) { (void)ctx; snprintf(S.draft, sizeof S.draft, "%s", value); }
static void send_clicked(lv_event_t *e) {
    (void)e; ink_args_t *a = ink_app_alloc(sizeof *a, true); if (!a) return;
    ink_args_init(a); ink_args_str(a, "to", S.to); ink_args_str(a, "text", ink_textfield_text(S.field)); request("sms.send", a); ink_app_free(a);
}
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % 4; char peer[24], status[96]; bool picking = !strcmp(ink_ui_page_name(), COMPOSE.name), thread = !strcmp(ink_ui_page_name(), THREAD.name);
    if (picking) {
        char raw[768] = {0}; ink_db_get(S.contacts, S.rows[index], NULL, raw, sizeof raw - 1); ink_json_t *j = ink_app_alloc(sizeof *j, true);
        if (j && ink_json_parse(j, raw, strlen(raw)) == 0) { ink_json_str(j, ink_json_find(j, 0, "name"), S.labels[at], sizeof S.labels[at]); ink_json_str(j, ink_json_find(j, 0, "number"), S.texts[at], sizeof S.texts[at]); }
        ink_app_free(j); snprintf(S.args[at], sizeof S.args[at], "{\"id\":%u}", (unsigned)S.rows[index]);
    } else {
        read_message(S.db, S.rows[index], peer, S.texts[at], status); snprintf(S.labels[at], sizeof S.labels[at], "%s", thread ? status : peer);
        snprintf(S.args[at], sizeof S.args[at], "{\"peer\":\"%s\"}", peer);
    }
    snprintf(S.ids[at], sizeof S.ids[at], "sms:%u", (unsigned)S.rows[index]);
    if (!lv_obj_get_child_count(slot)) { ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_MEDIUM, INK_GAP, INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP); ink_ui_label(slot, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_GAP, INK_LINE_NOTE + INK_GAP_M, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP); }
    lv_obj_set_style_bg_color(slot, thread && strcmp(S.labels[at], "收到") ? ink_ui_ink() : ink_ui_gray(), 0);
    lv_obj_set_style_text_color(slot, thread && strcmp(S.labels[at], "收到") ? ink_ui_paper() : ink_ui_ink(), 0);
    lv_label_set_text(lv_obj_get_child(slot, 0), S.labels[at]); lv_label_set_text(lv_obj_get_child(slot, 1), S.texts[at]);
    *field = (ink_ui_field_spec_t){.id = S.ids[at], .role = "listitem", .label = S.labels[at], .value = S.texts[at], .object = {S.ids[at], ink_db_last_ver(picking ? S.contacts : S.db)}};
    *action = (ink_ui_action_spec_t){.id = S.ids[at], .label = picking ? "选择这个号码" : "打开会话", .capability = picking ? "sms.recipient" : "sms.thread", .args_json = S.args[at], .object = field->object, .enabled = !thread};
}
static void build_rows(bool picking, bool thread) {
    S.count = 0; ink_db_cursor_t cur = {.desc = true}; ink_rec_t r; char peer[24], text[355], status[96];
    while (ink_db_next(picking ? S.contacts : S.db, &cur, &r, NULL, 0) == 1 && S.count < 512) {
        if (!picking) {
            if (!read_message(S.db, r.id, peer, text, status) || (thread && strcmp(peer, S.peer))) continue;
            bool seen = false; if (!thread) for (size_t i = 0; i < S.count; i++) if (!strcmp(S.peers[i], peer)) seen = true;
            if (seen) continue; snprintf(S.peers[S.count], sizeof S.peers[0], "%s", peer);
        }
        S.rows[S.count++] = r.id;
    }
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    if (strcmp(S.member, ink_member_current()->id)) { memset(&S.position, 0, sizeof S.position); S.to[0] = S.peer[0] = S.draft[0] = S.result[0] = 0; snprintf(S.member, sizeof S.member, "%s", ink_member_current()->id); }
    bool compose = !strcmp(ink_ui_page_name(), COMPOSE.name), lng = !strcmp(ink_ui_page_name(), LONG.name), thread = !strcmp(ink_ui_page_name(), THREAD.name);
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, compose || lng ? "写短信" : thread ? S.peer : "短信");
    S.notice = ink_ui_notice(screen, S.result, INK_CANVAS_H - INK_PRIMARY - INK_CONTROL - INK_GAP_L);
    S.form = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = compose || lng ? "写短信" : "短信", .object = {"sms.messages", 1}, .actions = S.actions, .action_count = 3};
    S.actions[0] = (ink_ui_action_spec_t){.id = "prev", .label = "上一页", .capability = "sms.prev"};
    S.actions[1] = (ink_ui_action_spec_t){.id = "next", .label = "下一页", .capability = "sms.next"};
    S.actions[2] = (ink_ui_action_spec_t){.id = "compose", .label = "写短信", .capability = "sms.compose", .enabled = true};
    if (lng) {
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = 354, .initial = S.draft, .draft_page = LONG.name}; S.field = ink_textfield_long_create(screen, &cfg);
        lv_obj_t *done = ink_ui_pill(screen, "返回", INK_CANVAS_W - INK_MARGIN - INK_PRIMARY, INK_STATUS_BAR, INK_PRIMARY, INK_CONTROL, false, NULL, NULL); ink_shell_bind(done, "shell.back", "{}"); return;
    }
    if (compose && S.to[0]) {
        ink_ui_line(screen, S.to, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L, INK_CANVAS_W - 2 * INK_MARGIN);
        S.actions[0] = (ink_ui_action_spec_t){.id = "text", .label = "输入短信", .capability = "sms.text", .enabled = true};
        S.actions[1] = (ink_ui_action_spec_t){.id = "long", .label = "写长短信", .capability = "sms.long", .enabled = true}; S.form.action_count = 2;
        S.field_spec = (ink_ui_field_spec_t){.id = "text", .role = "textbox", .label = "短信内容"}; S.form.fields = &S.field_spec; S.form.field_count = 1;
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .max = 354, .initial = S.draft, .hint = "写短信"};
        S.field = ink_textfield_create(screen, &cfg, entered, NULL, false); lv_obj_set_pos(S.field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_STATUS_BAR + INK_PRIMARY + INK_CONTROL));
        ink_textfield_bind_short(S.field, &S.form, &S.field_spec, &S.actions[0]);
        lv_obj_t *long_button = ink_ui_pill(screen, "写长短信", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY - INK_CONTROL - INK_GAP_L, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL); ink_shell_bind(long_button, "sms.long", "{}");
        ink_ui_pill(screen, "发送", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, send_clicked, NULL); return;
    }
    S.db = ink_db_open_cached_member(&MESSAGES, S.member);
    S.contacts = compose ? ink_db_open_cached_member(ink_db_find_def("phone.contacts"), S.member) : NULL;
    if (!S.db || (compose && !S.contacts)) {
        ink_args_t *a = ink_app_alloc(sizeof *a, true); if (a) { ink_args_init(a); ink_args_str(a, "collection", !S.db ? MESSAGES.name : "phone.contacts"); ink_args_int(a, "limit", 1); request("data.list", a); ink_app_free(a); } return;
    }
    build_rows(compose, thread); int top = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L, h = thread ? INK_CANVAS_H - top - 2 * INK_PRIMARY - INK_CONTROL - INK_MARGIN : INK_ROW_DOUBLE;
    lv_obj_t *body = ink_ui_box(screen, 0, top, INK_CANVAS_W, INK_CANVAS_H - top - INK_PRIMARY - INK_MARGIN, 0, INK_SURFACE_WHITE, false);
    S.list = ink_ui_list_create(body, &S.form, &S.position, thread ? 1 : 4, 1, h, compose ? "请先在联系人里添加号码" : "还没有短信", bind_item, NULL); if (S.list) ink_ui_list_refresh(S.list, S.count);
    if (!compose) { lv_obj_t *b = ink_ui_pill(screen, thread ? "回复短信" : "写短信", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, false, NULL, NULL); ink_shell_bind(b, "sms.compose", "{}"); }
}
static void destroy(void) {
    if (S.field) snprintf(S.draft, sizeof S.draft, "%s", ink_textfield_text(S.field));
    ink_db_close(S.db); ink_db_close(S.contacts); S.db = S.contacts = NULL; S.list = NULL; S.field = S.notice = NULL;
}
static int snapshot(char *out, size_t cap) {
    if (strcmp(S.member, ink_member_current()->id)) { if (cap) out[0] = 0; return -1; }
    if (S.list) return ink_ui_list_snapshot(S.list, out, cap);
    if (S.field && !strcmp(ink_ui_page_name(), LONG.name)) return ink_textfield_long_snapshot(S.field, out, cap);
    if (S.field) {
        static char args[768]; ink_jw_t w; ink_jw_init(&w, args, sizeof args); ink_jw_raw(&w, "{\"to\":"); ink_jw_str(&w, S.to); ink_jw_raw(&w, ",\"text\":"); ink_jw_str(&w, ink_textfield_text(S.field)); ink_jw_raw(&w, "}");
        S.actions[2] = (ink_ui_action_spec_t){.id = "send", .label = "发送短信", .capability = "sms.send", .args_json = args, .enabled = !w.overflow}; S.form.action_count = 3;
        return ink_textfield_short_snapshot(S.field, out, cap);
    }
    return ink_ui_semantics_snapshot(&S.form, out, cap);
}
static bool page_key(ink_key_t k, bool lng) { return S.list && ink_ui_list_key(S.list, k, lng); }
static const ink_activity_t LIST = {.name = "Sms", .app = "sms", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t THREAD = {.name = "SmsThread", .app = "sms", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t COMPOSE = {.name = "SmsCompose", .app = "sms", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t LONG = {.name = "ImeLong", .app = "sms", .create = create, .destroy = destroy, .snapshot = snapshot};
static const ink_app_def_t APP = {.id = "sms", .title = "短信", .icon = INK_UI_ICON(message), .entry = &LIST, .system_app = true, .child_ok = true};
static const ink_cap_t CAPS[] = {
    {.id = "sms.send", .title = "保存并发送短信", .ask = "发短信给 {to}：{text}", .input = "{\"type\":\"object\",\"properties\":{\"to\":{\"type\":\"string\",\"maxLength\":20},\"text\":{\"type\":\"string\",\"maxLength\":118}},\"required\":[\"to\",\"text\"]}", .effect = INK_FX_EXTERNAL, .child_ok = true, .permission = "message", .exec = INK_EXEC_BG, .timeout_ms = 30000, .run = send_message},
    {.id = "sms.thread", .title = "打开短信会话", .input = "{\"type\":\"object\",\"properties\":{\"peer\":{\"type\":\"string\",\"maxLength\":20}},\"required\":[\"peer\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.compose", .title = "写短信或回复当前会话", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.recipient", .title = "从联系人选择收件人", .activity = "SmsCompose", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.text", .title = "输入短信", .activity = "SmsCompose", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.long", .title = "打开短信长文编辑页", .activity = "SmsCompose", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.prev", .title = "短信上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.next", .title = "短信下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}
};
int ink_sms_init(void) {
    return ink_db_register(&MESSAGES) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_cap_subscribe("message.received", received, NULL, INK_EXEC_BG) ? -1 : 0;
}
