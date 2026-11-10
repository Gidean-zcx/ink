/* SMS pages share the member store, capability bus and F2/F3/F4 hosts. */
#include <stdio.h>
#include <string.h>
#include "ink/probe.h"
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/i18n.h"
#include "ink/sys/comm.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

static const ink_activity_t LIST, THREAD, COMPOSE, LONG;
static struct {
    ink_db_t *db, *contacts;
    uint32_t shown_ver; /* sms.messages as the open page last read it; 0 when closed (atomic: read on BG) */
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    lv_obj_t *field, *notice, *send, *parts;
    char member[16], peer[24], to[24], draft[INK_SMS_TEXT_MAX], result[160], scratch[INK_SMS_TEXT_MAX], row[INK_SMS_TEXT_MAX], send_args[3 * INK_SMS_TEXT_MAX], send_label[96];
    struct { char to[24]; uint32_t id; } draft_ref;
    uint32_t rows[512]; size_t count;
    uint32_t pages[513]; int body_h;
    char peers[512][24], ids[4][32], args[4][64], texts[4][INK_SMS_TEXT_MAX], labels[4][96];
    ink_ui_field_spec_t field_spec;
    ink_ui_action_spec_t actions[5];
    ink_ui_page_spec_t form;
    ink_exec_job_t refresh;
} S INK_PSRAM_BSS;

static bool read_message(ink_db_t *db, uint32_t id, char *peer, char *text, char *status) { /* text: INK_SMS_TEXT_MAX */
    size_t cap = 3 * INK_SMS_TEXT_MAX; char *raw = ink_app_alloc(cap, true); ink_json_t *j = ink_app_alloc(sizeof *j, true);
    bool ok = j && raw && (memset(raw, 0, cap), ink_db_get(db, id, NULL, raw, cap - 1) == INK_DB_OK) && ink_json_parse(j, raw, strlen(raw)) == 0;
    if (ok) { ink_json_str(j, ink_json_find(j, 0, "peer"), peer, 24); ink_json_str(j, ink_json_find(j, 0, "text"), text, INK_SMS_TEXT_MAX); ink_json_str(j, ink_json_find(j, 0, "status"), status, 96); }
    ink_app_free(j); ink_app_free(raw); return ok;
}
/* Each part is billed: under the text (above the keyboard), how many it goes as, when more than one. */
static void parts_note(lv_event_t *e) {
    (void)e; int n = S.field && S.parts ? ink_comm_sms_parts(ink_textfield_text(S.field)) : 0; char note[64] = "";
    if (n > INK_SMS_PARTS_MAX) snprintf(note, sizeof note, TR("太长了，最多分 %d 条发送"), INK_SMS_PARTS_MAX);
    else if (n > 1) snprintf(note, sizeof note, TR("将按 %d 条发送"), n);
    if (S.parts) lv_label_set_text(S.parts, note);
}
static void refresh(void *arg) {
    (void)arg;
    if (ink_ui_page_current() && !strcmp(S.member, ink_member_current()->id) &&
        (!strcmp(ink_ui_page_name(), LIST.name) || !strcmp(ink_ui_page_name(), THREAD.name))) ink_ui_redraw();
}
/* BG, after the communication service's own subscriber has stored the message (it subscribed first). An open
 * page that already shows this version gets no UI work at all: even a no-op job would keep the UI busy. */
static void received(void *ctx, const char *event, const ink_args_t *a) {
    (void)ctx; (void)event;
    uint32_t shown = __atomic_load_n(&S.shown_ver, __ATOMIC_ACQUIRE);
    if (shown) {
        char member[16] = "";
        ink_args_get_str(a, "member", member, sizeof member);
        ink_db_t *db = member[0] ? ink_db_open_cached_member(ink_db_find_def("sms.messages"), member) : NULL;
        uint32_t stored = db ? ink_db_last_ver(db) : 0;
        ink_db_close(db);
        if (stored == shown) return;
    }
    ink_exec_post(INK_EXEC_UI, &S.refresh, refresh, NULL);
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || strcmp(S.member, ink_member_current()->id)) return;
    if (S.send) lv_obj_remove_state(S.send, LV_STATE_DISABLED);
    snprintf(S.result, sizeof S.result, "%s", rc->say[0] ? rc->say : rc->status == INK_RC_DONE ? "" : TR("没有完成，请重试"));
    if (rc->status == INK_RC_DONE) ink_ui_redraw(); else if (S.notice) lv_label_set_text(S.notice, S.result);
    ink_shell_airplane_card(rc);
}
static void request(const char *cap, const ink_args_t *args) {
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .member = S.member, .reply_exec = INK_EXEC_UI, .done = reply, .ctx = (void *)(uintptr_t)ink_ui_generation()};
    if (ink_cap_call(cap, args, &o) != 0) { if (S.send) lv_obj_remove_state(S.send, LV_STATE_DISABLED); if (S.notice) lv_label_set_text(S.notice, TR("没有完成，请重试")); }
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
static void member_ready(void) {
    if (strcmp(S.member, ink_member_current()->id)) { memset(&S.position, 0, sizeof S.position); memset(&S.draft_ref, 0, sizeof S.draft_ref); S.to[0] = S.peer[0] = S.draft[0] = S.result[0] = 0; snprintf(S.member, sizeof S.member, "%s", ink_member_current()->id); }
}
static void navigate(ink_call_t *c) {
    member_ready();
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
static void entered(void *ctx, const char *value) { (void)ctx; S.draft_ref.id = 0; snprintf(S.draft, sizeof S.draft, "%s", value); }
static void send_clicked(lv_event_t *e) {
    (void)e; ink_args_t *a = ink_app_alloc(sizeof *a, true); if (!a) return;
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = INK_SMS_TEXT_MAX - 1, .initial = ink_textfield_text(S.field), .draft_page = COMPOSE.name};
    int session = ink_text_focus(&cfg, NULL, NULL);
    uint32_t draft = session > 0 ? ink_text_draft_id() : 0;
    if (session > 0) ink_text_blur(session);
    if (!draft) { ink_app_free(a); lv_label_set_text(S.notice, TR("短信草稿没能保存，未发送")); return; }
    lv_obj_add_state(S.send, LV_STATE_DISABLED); ink_ui_busy_control(S.send, true);
    ink_args_init(a); ink_args_str(a, "to", S.to); ink_args_str(a, "page", COMPOSE.name); ink_args_int(a, "draft", draft); request("message.send", a); ink_app_free(a);
}
static size_t text_end(const char *text, size_t start) {
    size_t end = start, len = strlen(text); char *scratch = S.scratch;
    while (end < len) {
        size_t next = ink_text_cluster_end(text, end); memcpy(scratch, text + start, next - start); scratch[next - start] = 0;
        lv_point_t size; lv_text_get_size(&size, scratch, ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR), 0, 0,
            ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP), LV_TEXT_FLAG_NONE);
        if (end > start && size.y > ink_ui_px(S.body_h - INK_LINE_NOTE - INK_GAP_M - INK_GAP_S)) break;
        end = next;
    }
    return end;
}
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % 4; char peer[24], status[96]; bool picking = !strcmp(ink_ui_page_name(), COMPOSE.name), thread = !strcmp(ink_ui_page_name(), THREAD.name);
    size_t part = 0; if (thread) { size_t row = 0; while (row + 1 < S.count && S.pages[row + 1] <= index) row++; part = index - S.pages[row]; index = row; }
    if (picking) {
        char raw[768] = {0}; ink_db_get(S.contacts, S.rows[index], NULL, raw, sizeof raw - 1); ink_json_t *j = ink_app_alloc(sizeof *j, true);
        if (j && ink_json_parse(j, raw, strlen(raw)) == 0) { ink_json_str(j, ink_json_find(j, 0, "name"), S.labels[at], sizeof S.labels[at]); ink_json_str(j, ink_json_find(j, 0, "number"), S.texts[at], sizeof S.texts[at]); }
        ink_app_free(j); snprintf(S.args[at], sizeof S.args[at], "{\"id\":%u}", (unsigned)S.rows[index]);
    } else {
        read_message(S.db, S.rows[index], peer, S.texts[at], status); snprintf(S.labels[at], sizeof S.labels[at], "%s", thread ? TR(status) : peer);
        snprintf(S.args[at], sizeof S.args[at], "{\"peer\":\"%s\"}", peer);
        if (thread) { size_t start = 0; while (part--) start = text_end(S.texts[at], start); size_t end = text_end(S.texts[at], start); S.texts[at][end] = 0; memmove(S.texts[at], S.texts[at] + start, end - start + 1); }
    }
    snprintf(S.ids[at], sizeof S.ids[at], "sms:%u", (unsigned)S.rows[index]);
    if (!lv_obj_get_child_count(slot)) { ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_MEDIUM, INK_GAP, INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP); ink_ui_label(slot, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_GAP, INK_LINE_NOTE + INK_GAP_M, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP); }
    lv_obj_set_style_bg_color(slot, thread && strcmp(status, "收到") ? ink_ui_ink() : ink_ui_gray(), 0);
    for (int i = 0; i < 2; i++) lv_obj_set_style_text_color(lv_obj_get_child(slot, i), thread && strcmp(status, "收到") ? ink_ui_paper() : ink_ui_ink(), 0);
    lv_label_set_text(lv_obj_get_child(slot, 0), S.labels[at]); lv_label_set_text(lv_obj_get_child(slot, 1), S.texts[at]);
    *field = (ink_ui_field_spec_t){.id = S.ids[at], .role = "listitem", .label = S.labels[at], .value = S.texts[at], .object = {S.ids[at], ink_db_last_ver(picking ? S.contacts : S.db)}};
    *action = (ink_ui_action_spec_t){.id = S.ids[at], .label = picking ? TR("选择这个号码") : TR("打开会话"), .capability = picking ? "sms.recipient" : "sms.thread", .args_json = S.args[at], .object = field->object, .enabled = !thread};
}
static void build_rows(bool picking, bool thread) {
    S.count = 0; S.pages[0] = 0; ink_db_cursor_t cur = {.desc = true}; ink_rec_t r; char peer[24], *text = S.row, status[96];
    while (ink_db_next(picking ? S.contacts : S.db, &cur, &r, NULL, 0) == 1 && S.count < 512) {
        if (!picking) {
            if (!read_message(S.db, r.id, peer, text, status) || (thread && strcmp(peer, S.peer))) continue;
            bool seen = false; if (!thread) for (size_t i = 0; i < S.count; i++) if (!strcmp(S.peers[i], peer)) seen = true;
            if (seen) continue;
            snprintf(S.peers[S.count], sizeof S.peers[0], "%s", peer);
        }
        S.pages[S.count + 1] = S.pages[S.count];
        if (thread) { size_t end = 0; do { end = text_end(text, end); S.pages[S.count + 1]++; } while (text[end]); }
        S.rows[S.count++] = r.id;
    }
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    member_ready();
    bool compose = !strcmp(ink_ui_page_name(), COMPOSE.name), lng = !strcmp(ink_ui_page_name(), LONG.name), thread = !strcmp(ink_ui_page_name(), THREAD.name);
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, compose || lng ? TR("写短信") : thread ? S.peer : TR("短信"));
    S.notice = ink_ui_notice(screen, S.result, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY - INK_CONTROL - 2 * INK_GAP_L - INK_LINE_NOTE);
    S.form = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = compose || lng ? TR("写短信") : TR("短信"), .object = {"sms.messages", 1}, .actions = S.actions, .action_count = 3};
    S.actions[0] = (ink_ui_action_spec_t){.id = "prev", .label = TR("上一页"), .capability = "sms.prev"};
    S.actions[1] = (ink_ui_action_spec_t){.id = "next", .label = TR("下一页"), .capability = "sms.next"};
    S.actions[2] = (ink_ui_action_spec_t){.id = "compose", .label = TR("写短信"), .capability = "sms.compose", .enabled = true};
    if (lng) {
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = INK_SMS_TEXT_MAX - 1, .initial = S.draft, .draft_page = LONG.name, .draft_id = !strcmp(S.to, S.draft_ref.to) ? S.draft_ref.id : 0}; S.field = ink_textfield_long_create(screen, &cfg);
        if (S.field) { S.draft_ref.id = ink_text_draft_id(); snprintf(S.draft_ref.to, sizeof S.draft_ref.to, "%s", S.to); ink_app_save_state("message"); }
        lv_obj_t *done = ink_ui_pill(screen, TR("返回"), INK_CANVAS_W - INK_MARGIN - INK_PRIMARY, INK_STATUS_BAR, INK_PRIMARY, INK_CONTROL, false, NULL, NULL); ink_shell_bind(done, "shell.back", "{}"); return;
    }
    if (compose && S.to[0]) {
        ink_ui_line(screen, S.to, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L, INK_CANVAS_W - 2 * INK_MARGIN);
        S.actions[0] = (ink_ui_action_spec_t){.id = "text", .label = TR("输入短信"), .capability = "sms.text", .enabled = true};
        S.actions[1] = (ink_ui_action_spec_t){.id = "long", .label = TR("写长短信"), .capability = "sms.long", .enabled = true}; S.form.action_count = 2;
        S.field_spec = (ink_ui_field_spec_t){.id = "text", .role = "textbox", .label = TR("短信内容")}; S.form.fields = &S.field_spec; S.form.field_count = 1;
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .max = INK_SMS_TEXT_MAX - 1, .initial = S.draft, .hint = TR("写短信")};
        S.field = ink_textfield_create(screen, &cfg, entered, NULL, false); lv_obj_set_pos(S.field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_STATUS_BAR + INK_PRIMARY + INK_CONTROL));
        lv_obj_set_width(S.field, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
        S.parts = ink_ui_line(screen, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, INK_STATUS_BAR + INK_PRIMARY + 2 * INK_CONTROL + INK_GAP, INK_CANVAS_W - 2 * INK_MARGIN);
        ink_textfield_bind_short(S.field, &S.form, &S.field_spec, &S.actions[0]); lv_obj_add_event_cb(S.field, parts_note, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_t *long_button = ink_ui_pill(screen, TR("写长短信"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY - INK_CONTROL - INK_GAP_L, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL); ink_shell_bind(long_button, "sms.long", "{}");
        S.send = ink_ui_pill(screen, TR("发送"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, send_clicked, NULL); parts_note(NULL); return;
    }
    S.db = ink_db_open_cached_member(ink_db_find_def("sms.messages"), S.member);
    __atomic_store_n(&S.shown_ver, S.db ? ink_db_last_ver(S.db) : 0, __ATOMIC_RELEASE);
    S.contacts = compose ? ink_db_open_cached_member(ink_db_find_def("phone.contacts"), S.member) : NULL;
    if (!S.db || (compose && !S.contacts)) {
        ink_args_t *a = ink_app_alloc(sizeof *a, true); if (a) { ink_args_init(a); ink_args_str(a, "collection", !S.db ? "sms.messages" : "phone.contacts"); ink_args_int(a, "limit", 1); request("data.list", a); ink_app_free(a); } return;
    }
    int top = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L; S.body_h = thread ? INK_CANVAS_H - top - 2 * INK_PRIMARY - INK_CONTROL - INK_MARGIN : INK_ROW_DOUBLE; build_rows(compose, thread);
    lv_obj_t *body = ink_ui_box(screen, 0, top, INK_CANVAS_W, INK_CANVAS_H - top - INK_PRIMARY - INK_MARGIN, 0, INK_SURFACE_WHITE, false);
    S.list = ink_ui_list_create(body, &S.form, &S.position, thread ? 1 : 4, 1, S.body_h, compose ? TR("请先在联系人里添加号码") : TR("还没有短信"), bind_item, NULL); if (S.list) ink_ui_list_refresh(S.list, thread ? S.pages[S.count] : S.count);
    if (!compose) { lv_obj_t *b = ink_ui_pill(screen, thread ? TR("回复短信") : TR("写短信"), INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, false, NULL, NULL); ink_shell_bind(b, "sms.compose", "{}"); }
}
static void destroy(void) {
    if (S.field) snprintf(S.draft, sizeof S.draft, "%s", ink_textfield_text(S.field));
    __atomic_store_n(&S.shown_ver, 0, __ATOMIC_RELEASE);
    ink_db_close(S.db); ink_db_close(S.contacts); S.db = S.contacts = NULL; S.list = NULL; S.field = S.notice = S.send = S.parts = NULL;
}
static int snapshot(char *out, size_t cap) {
    if (strcmp(S.member, ink_member_current()->id)) { if (cap) out[0] = 0; return -1; }
    if (S.list) return ink_ui_list_snapshot(S.list, out, cap);
    if (S.field && !strcmp(ink_ui_page_name(), LONG.name)) return ink_textfield_long_snapshot(S.field, out, cap);
    if (S.field) {
        char *args = S.send_args; ink_jw_t w; ink_jw_init(&w, args, sizeof S.send_args); ink_jw_raw(&w, "{\"to\":"); ink_jw_str(&w, S.to); ink_jw_raw(&w, ",\"text\":"); ink_jw_str(&w, ink_textfield_text(S.field)); ink_jw_raw(&w, "}");
        const char *note = S.parts ? lv_label_get_text(S.parts) : ""; snprintf(S.send_label, sizeof S.send_label, note[0] ? TR("发送短信，%s") : TR("发送短信%s"), note); /* the AI sees the parts too */
        S.actions[2] = (ink_ui_action_spec_t){.id = "send", .label = S.send_label, .capability = "message.send", .args_json = args, .enabled = !w.overflow}; S.form.action_count = 3;
        return ink_textfield_short_snapshot(S.field, out, cap);
    }
    return ink_ui_semantics_snapshot(&S.form, out, cap);
}
static bool page_key(ink_key_t k, bool lng) { return S.list && ink_ui_list_key(S.list, k, lng); }
static const ink_activity_t LIST = {.name = "Sms", .app = "message", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t THREAD = {.name = "SmsThread", .app = "message", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t COMPOSE = {.name = "SmsCompose", .app = "message", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t LONG = {.name = "ImeLong", .app = "message", .create = create, .destroy = destroy, .snapshot = snapshot};
static int save_state(void *buf, size_t cap) { if (cap < sizeof S.draft_ref) return 0; memcpy(buf, &S.draft_ref, sizeof S.draft_ref); return sizeof S.draft_ref; }
static void restore_state(const void *buf, size_t len) { if (len == sizeof S.draft_ref) memcpy(&S.draft_ref, buf, len); }
static const ink_app_ops_t OPS = {.on_start = member_ready, .save_state = save_state, .restore_state = restore_state};
static const ink_app_def_t APP = {.id = "message", .title = "短信", .icon = INK_UI_ICON(message), .entry = &LIST, .system_app = true, .child_ok = true, .ops = &OPS};
static const ink_cap_t CAPS[] = {
    {.id = "sms.thread", .app = "message", .title = "打开短信会话", .input = "{\"type\":\"object\",\"properties\":{\"peer\":{\"type\":\"string\",\"maxLength\":20}},\"required\":[\"peer\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.compose", .app = "message", .title = "写短信或回复当前会话", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.recipient", .app = "message", .title = "从联系人选择收件人", .activity = "SmsCompose", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.text", .app = "message", .title = "输入短信", .activity = "SmsCompose", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.long", .app = "message", .title = "打开短信长文编辑页", .activity = "SmsCompose", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.prev", .app = "message", .title = "短信上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.next", .app = "message", .title = "短信下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}
};
int ink_sms_init(void) {
    return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_cap_subscribe("message.received", received, NULL, INK_EXEC_BG) ? -1 : 0;
}
