/* ui-scale-runtime: dimension S.body_h,S.heights[i] Page space and bubble heights come from theme tokens and measured text. */
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
#include "ink/sys/notify.h"
#include "ink/sys/time.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

enum { SMS_ROWS = 4, SMS_VISIBLE = 8, SMS_LIMIT = 512 };
extern int ink_contacts_match(const char *query, bool exact, char *name, char *number);
static const ink_activity_t LIST, THREAD, COMPOSE, LONG;
static struct {
    ink_db_t *db;
    uint32_t shown_ver; /* sms.messages as the open page last read it; 0 when closed (atomic: read on BG) */
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    lv_obj_t *field, *recipient, *suggestion, *notice, *send, *parts;
    bool selecting;
    char match_name[97], match_number[25], match_args[48], list_snapshot[4096];
    char member[16], peer[24], to[24], draft[INK_SMS_TEXT_MAX], result[160], scratch[INK_SMS_TEXT_MAX], row[INK_SMS_TEXT_MAX], send_args[3 * INK_SMS_TEXT_MAX], send_label[96];
    struct { char to[24]; uint32_t id; } draft_ref;
    uint32_t rows[SMS_LIMIT]; size_t count;
    uint32_t pages[SMS_LIMIT + 1]; int body_h;
    bool groups[SMS_LIMIT], stamps[SMS_VISIBLE], outgoing[SMS_VISIBLE];
    int heights[SMS_VISIBLE]; size_t thread_pages, visible;
    lv_obj_t *thread_slot;
    char times[SMS_VISIBLE][32], states[SMS_VISIBLE][128];
    ink_ui_field_spec_t message_fields[SMS_VISIBLE];
    ink_ui_action_spec_t message_actions[SMS_VISIBLE + 2];
    ink_ui_page_spec_t messages;
    char peers[SMS_LIMIT][24], ids[SMS_VISIBLE][32], args[SMS_VISIBLE][64], texts[SMS_VISIBLE][INK_SMS_TEXT_MAX], labels[SMS_VISIBLE][192];
    ink_ui_field_spec_t fields[2];
    ink_ui_action_spec_t actions[6];
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
static bool unread(const char *peer, bool mark) {
    ink_db_cursor_t cur = {0}; ink_notify_view_t n; bool found = false;
    while (ink_notify_next(&cur, &n) == 1) {
        if (n.read || strcmp(n.channel, "message") || strcmp(n.group, peer)) continue;
        found = true;
        if (mark) { ink_args_t a; ink_args_init(&a); ink_args_int(&a, "id", n.id); ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = S.member}; ink_cap_call("notify.read", &a, &o); }
    }
    return found;
}
static void contact_selected(void *ctx, const char *event, const ink_args_t *a) {
    (void)ctx; (void)event; char member[16]; ink_args_get_str(a, "member", member, sizeof member);
    if (!S.selecting || strcmp(member, S.member) || strcmp(member, ink_member_current()->id) || strcmp(ink_ui_page_name(), "PhoneContacts")) return;
    ink_args_get_str(a, "number", S.to, sizeof S.to); S.selecting = false; ink_ui_pop();
}
static void recipient_edited(lv_event_t *e) {
    (void)e; snprintf(S.to, sizeof S.to, "%s", ink_textfield_text(S.recipient));
    int id = ink_contacts_match(S.to, false, S.match_name, S.match_number);
    S.actions[5] = (ink_ui_action_spec_t){.id = "match", .label = S.match_name, .capability = "sms.recipient", .args_json = S.match_args, .enabled = id > 0};
    if (id > 0) { snprintf(S.match_args, sizeof S.match_args, "{\"id\":%u}", (unsigned)id); lv_label_set_text(lv_obj_get_child(S.suggestion, 0), S.match_name); lv_obj_remove_flag(S.suggestion, LV_OBJ_FLAG_HIDDEN); ink_shell_bind(S.suggestion, "sms.recipient", S.match_args); }
    else lv_obj_add_flag(S.suggestion, LV_OBJ_FLAG_HIDDEN);
    ink_ui_content_changed();
}
static void refresh_thread(void);
static void reply_field(lv_obj_t *screen);
static void refresh(void *arg) {
    (void)arg;
    if (ink_ui_page_current() && !strcmp(S.member, ink_member_current()->id) &&
        (!strcmp(ink_ui_page_name(), LIST.name) || !strcmp(ink_ui_page_name(), THREAD.name))) {
        if (S.list && !strcmp(ink_ui_page_name(), THREAD.name)) refresh_thread(); else ink_ui_redraw();
    }
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
static void show(const ink_activity_t *page) {
    ink_ui_drop_app("message");
    if (page != &LIST) ink_ui_push(&LIST, NULL);
    if (page == &THREAD) S.position.page = SIZE_MAX;
    ink_ui_push(page, NULL);
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || strcmp(S.member, ink_member_current()->id)) return;
    bool sending = S.send && lv_obj_has_state(S.send, LV_STATE_DISABLED), sent = sending && rc->status == INK_RC_DONE;
    if (S.send) lv_obj_remove_state(S.send, LV_STATE_DISABLED);
    snprintf(S.result, sizeof S.result, "%s", rc->say[0] ? rc->say : rc->status == INK_RC_DONE ? "" : TR("没有完成，请重试"));
    if (sent) {
        if (!strncmp(rc->object, "message:to:", 11)) snprintf(S.to, sizeof S.to, "%.20s", rc->object + 11);
        snprintf(S.peer, sizeof S.peer, "%s", S.to); S.draft[0] = 0; S.draft_ref.id = 0;
        if (!strcmp(ink_ui_page_name(), COMPOSE.name)) { S.field = NULL; S.position.page = SIZE_MAX; show(&THREAD); }
        else { lv_obj_delete(S.field); reply_field(lv_screen_active()); parts_note(NULL); lv_label_set_text(S.notice, ""); refresh_thread(); }
    } else if (sending && S.list && !strcmp(ink_ui_page_name(), THREAD.name)) { lv_label_set_text(S.notice, ""); refresh_thread(); }
    else if (rc->status == INK_RC_DONE) ink_ui_redraw(); else if (S.notice) lv_label_set_text(S.notice, S.result);
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
    if (strcmp(S.member, ink_member_current()->id)) { memset(&S.position, 0, sizeof S.position); memset(&S.draft_ref, 0, sizeof S.draft_ref); S.selecting = false; S.to[0] = S.peer[0] = S.draft[0] = S.result[0] = 0; snprintf(S.member, sizeof S.member, "%s", ink_member_current()->id); }
}
static void navigate(ink_call_t *c) {
    member_ready();
    const char *id = c->cap->id;
    if (!strcmp(id, "sms.recipient")) {
        ink_cap_defer(c); ink_call_opts_t o = {.src = c->src, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_UI, .done = selected, .ctx = c};
        if (ink_cap_call("contacts.select", &c->args, &o) != 0) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
    } else if (!strcmp(id, "sms.pick")) {
        S.selecting = true; ink_call_opts_t o = {.src = c->src, .member = S.member};
        if (ink_cap_call("contacts.pick", NULL, &o)) { S.selecting = false; c->rc.status = INK_RC_FAILED; }
    } else if (!strcmp(id, "sms.to")) {
        if (!S.recipient || ink_textfield_short_focus(S.recipient)) c->rc.status = INK_RC_STALE;
    } else if (!strcmp(id, "sms.retry")) {
        int64_t row = 0; char peer[24], status[96]; ink_args_get_int(&c->args, "id", &row);
        if (!S.db || !read_message(S.db, (uint32_t)row, peer, S.draft, status) || strcmp(peer, S.peer) || strcmp(status, TR("发送失败，未发出"))) { c->rc.status = INK_RC_STALE; return; }
        snprintf(S.to, sizeof S.to, "%s", peer); S.field = NULL; show(&COMPOSE);
    } else if (!strcmp(id, "sms.thread")) {
        ink_args_get_str(&c->args, "peer", S.peer, sizeof S.peer); snprintf(S.to, sizeof S.to, "%s", S.peer); S.draft[0] = S.result[0] = 0; S.position.page = SIZE_MAX; S.field = NULL; unread(S.peer, true); show(&THREAD);
    } else if (!strcmp(id, "sms.compose")) {
        char to[24] = ""; ink_args_get_str(&c->args, "number", to, sizeof to);
        snprintf(S.to, sizeof S.to, "%s", to[0] ? to : !strcmp(ink_ui_page_name(), THREAD.name) ? S.peer : ""); S.selecting = false; S.draft[0] = S.result[0] = 0; S.position.page = 0; S.field = NULL; show(&COMPOSE);
    } else if (!strcmp(id, "sms.next") || !strcmp(id, "sms.prev")) { if (S.list) { ink_ui_list_turn(S.list, !strcmp(id, "sms.next") ? 1 : -1); if (S.thread_slot) lv_obj_set_y(lv_obj_get_child(lv_obj_get_parent(S.thread_slot), -1), 0); } }
    else if (!strcmp(id, "sms.long")) {
        if (!S.field) c->rc.status = INK_RC_STALE;
        else { snprintf(S.draft, sizeof S.draft, "%s", ink_textfield_text(S.field)); ink_ui_push(&LONG, NULL); }
    } else if (!S.field || ink_textfield_short_focus(S.field) != 0) c->rc.status = INK_RC_STALE;
}
static void entered(void *ctx, const char *value) { (void)ctx; S.draft_ref.id = 0; snprintf(S.draft, sizeof S.draft, "%s", value); }
static void send_clicked(lv_event_t *e) {
    (void)e; ink_args_t *a = ink_app_alloc(sizeof *a, true); if (!a) return;
    ink_args_init(a); ink_args_str(a, "to", S.to);
    if (ink_args_str(a, "text", ink_textfield_text(S.field)) != 0) {
        const char *page = S.draft_ref.id && !strcmp(S.to, S.draft_ref.to) && !strcmp(S.draft, ink_textfield_text(S.field)) ? LONG.name : COMPOSE.name;
        uint32_t draft = !strcmp(page, LONG.name) ? S.draft_ref.id : 0;
        if (!draft) {
            ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = INK_SMS_TEXT_MAX - 1, .initial = ink_textfield_text(S.field), .draft_page = page};
            int session = ink_text_focus(&cfg, NULL, NULL); draft = session > 0 ? ink_text_draft_id() : 0;
            if (session > 0) ink_text_blur(session);
        }
        if (!draft) { ink_app_free(a); lv_label_set_text(S.notice, TR("短信草稿没能保存，未发送")); return; }
        ink_args_str(a, "page", page); ink_args_int(a, "draft", draft);
    }
    lv_obj_add_state(S.send, LV_STATE_DISABLED); ink_ui_busy_control(S.send, true); lv_label_set_text(S.notice, TR("发送中"));
    if (S.thread_slot) lv_label_set_text(S.parts, "");
    request("message.send", a); ink_app_free(a);
}
static size_t text_end(const char *text, size_t start) {
    size_t end = start, len = strlen(text); char *scratch = S.scratch;
    while (end < len) {
        size_t next = ink_text_cluster_end(text, end); memcpy(scratch, text + start, next - start); scratch[next - start] = 0;
        lv_point_t size; lv_text_get_size(&size, scratch, ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR), 0, 0,
            ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP - INK_GAP_XL), LV_TEXT_FLAG_NONE);
        if (end > start && size.y > ink_ui_px(S.body_h - 2 * ink_ui_line_height(INK_FONT_NOTE) - INK_GAP_M - 3 * INK_GAP_S)) break;
        end = next;
    }
    return end;
}
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; unsigned at = index % SMS_ROWS; char peer[24], status[96];
    read_message(S.db, S.rows[index], peer, S.texts[at], status);
    ink_rec_t rec = {0}; ink_db_get(S.db, S.rows[index], &rec, NULL, 0); ink_tm_t tm; ink_time_to_local(rec.updated_s, &tm);
    char name[97], number[25]; bool known = ink_contacts_match(peer, true, name, number) > 0;
    snprintf(S.labels[at], sizeof S.labels[at], "%s%s · %02d:%02d", unread(peer, false) ? "● " : "", known ? name : peer, tm.hour, tm.min);
    snprintf(S.args[at], sizeof S.args[at], "{\"peer\":\"%s\"}", peer);
    snprintf(S.ids[at], sizeof S.ids[at], "sms:%u", (unsigned)S.rows[index]);
    if (!lv_obj_get_child_count(slot)) { ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_MEDIUM, INK_GAP, INK_GAP_S, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP); ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_GAP, INK_LINE_NOTE + INK_GAP_M, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP); }
    ink_ui_surface_style(slot, INK_SURFACE_STATIC, true);
    lv_label_set_text(lv_obj_get_child(slot, 0), S.labels[at]); lv_label_set_text(lv_obj_get_child(slot, 1), S.texts[at]);
    *field = (ink_ui_field_spec_t){.id = S.ids[at], .role = "listitem", .label = S.labels[at], .value = S.texts[at], .object = {S.ids[at], ink_db_last_ver(S.db)}};
    *action = (ink_ui_action_spec_t){.id = S.ids[at], .label = TR("打开会话"), .capability = "sms.thread", .args_json = S.args[at], .object = field->object, .enabled = true};
}
/* Pack from the newest fragment backwards; reserve a time above each page's first bubble. */
static void thread_layout(size_t wanted) {
    size_t page = 0, n = 0; int used = 0, stamp_h = ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S;
    bool last_stamp = false; S.visible = 0;
    for (size_t row = 0; row < S.count; row++) {
        char peer[24], status[96], state[128]; read_message(S.db, S.rows[row], peer, S.row, status);
        bool own = strcmp(status, "收到") != 0;
        snprintf(state, sizeof state, "%s%s%s%s", TR(status), !strcmp(status, TR("发送失败，未发出")) ? "（" : "", !strcmp(status, TR("发送失败，未发出")) ? TR("点按重发") : "", !strcmp(status, TR("发送失败，未发出")) ? "）" : "");
        ink_rec_t rec = {0}; ink_db_get(S.db, S.rows[row], &rec, NULL, 0); ink_tm_t tm; ink_time_to_local(rec.updated_s, &tm); char timestamp[32];
        snprintf(timestamp, sizeof timestamp, "%04d-%02d-%02d %02d:%02d", tm.year, tm.month, tm.day, tm.hour, tm.min);
        for (size_t part = S.pages[row + 1] - S.pages[row]; part-- > 0;) {
            size_t start = 0; for (size_t i = 0; i < part; i++) start = text_end(S.row, start);
            size_t end = text_end(S.row, start); memcpy(S.scratch, S.row + start, end - start); S.scratch[end - start] = 0;
            int h = ink_ui_text_height(S.scratch, INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP_XL - 2 * INK_GAP) + 2 * INK_GAP_S + INK_GAP_M;
            if (own) h += ink_ui_text_height(state, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP_XL) + INK_GAP_S;
            if (strstr(state, TR("发送失败，未发出")) && h < INK_CONTROL + INK_GAP_M + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S) h = INK_CONTROL + INK_GAP_M + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S;
            int next = used + h + (!used || last_stamp ? stamp_h : 0);
            if (used && (next > S.body_h || n == SMS_VISIBLE)) { page++; n = 0; used = 0; next = h + stamp_h; }
            if (page == wanted) {
                snprintf(S.texts[n], sizeof S.texts[n], "%s", S.scratch); snprintf(S.states[n], sizeof S.states[n], "%s", state);
                snprintf(S.times[n], sizeof S.times[n], "%s", timestamp);
                snprintf(S.labels[n], sizeof S.labels[n], "%s · %s", timestamp, state);
                snprintf(S.ids[n], sizeof S.ids[n], "sms:%u", (unsigned)S.rows[row]); snprintf(S.args[n], sizeof S.args[n], "{\"id\":%u}", (unsigned)S.rows[row]);
                S.heights[n] = h; S.stamps[n] = part == 0 && S.groups[row]; S.outgoing[n] = own; S.visible = n + 1;
            }
            used = next; n++; last_stamp = part == 0 && S.groups[row];
        }
    }
    S.thread_pages = page + 1;
}
static void bind_thread(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; thread_layout(S.thread_pages - 1 - index);
    if (S.thread_slot != slot) { S.thread_slot = slot; lv_obj_set_style_radius(slot, 0, 0); ink_ui_surface_style(slot, INK_SURFACE_WHITE, false); lv_obj_set_pos(slot, 0, ink_ui_px(INK_CONTROL + INK_GAP)); lv_obj_set_size(slot, ink_ui_px(INK_CANVAS_W), ink_ui_px(S.body_h)); lv_obj_remove_flag(slot, LV_OBJ_FLAG_CLICKABLE); }
    lv_obj_update_layout(slot); lv_area_t dirty; lv_obj_get_coords(slot, &dirty); int old_top = dirty.y2 + 1;
    for (uint32_t i = 0; i < lv_obj_get_child_count(slot); i++) { lv_area_t a; lv_obj_get_coords(lv_obj_get_child(slot, i), &a); if (a.y1 < old_top) old_top = a.y1; }
    lv_display_t *display = lv_obj_get_display(slot); lv_display_enable_invalidation(display, false);
    while (lv_obj_get_child_count(slot)) lv_obj_delete(lv_obj_get_child(slot, 0));
    int y = S.body_h, stamp_h = ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S;
    S.messages = (ink_ui_page_spec_t){.id = THREAD.name, .title = TR("短信"), .object = {"sms.messages", ink_db_last_ver(S.db)}, .fields = S.message_fields, .field_count = S.visible, .actions = S.message_actions, .action_count = S.visible + 2};
    S.message_actions[0] = (ink_ui_action_spec_t){.id = "prev", .label = TR("上一页"), .capability = "sms.prev", .enabled = index > 0};
    S.message_actions[1] = (ink_ui_action_spec_t){.id = "next", .label = TR("下一页"), .capability = "sms.next", .enabled = index + 1 < S.thread_pages};
    for (size_t i = 0; i < S.visible; i++) {
        int w = INK_CANVAS_W - 2 * INK_MARGIN - INK_GAP_XL, h = ink_ui_text_height(S.texts[i], INK_FONT_LIST, INK_WEIGHT_REGULAR, w - 2 * INK_GAP) + 2 * INK_GAP_S;
        if (strstr(S.states[i], TR("发送失败，未发出")) && h < INK_CONTROL) h = INK_CONTROL;
        int bw = ink_ui_text_width(S.texts[i], INK_FONT_LIST, INK_WEIGHT_REGULAR) + 2 * INK_GAP; if (bw > w) bw = w;
        if (bw < INK_CONTROL) bw = INK_CONTROL;
        bool stamp = S.stamps[i] || i + 1 == S.visible; y -= S.heights[i] + (stamp ? stamp_h : 0);
        if (stamp) ink_ui_line(slot, S.times[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
        int x = INK_MARGIN + (S.outgoing[i] ? INK_CANVAS_W - 2 * INK_MARGIN - bw : 0);
        int by = y + (stamp ? stamp_h : 0);
        lv_obj_t *bubble = ink_ui_card(slot, x, by, bw, h, S.outgoing[i] ? INK_SURFACE_BLACK : INK_SURFACE_STATIC);
        lv_obj_t *label = ink_ui_label(bubble, S.texts[i], INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_GAP, INK_GAP_S, bw - 2 * INK_GAP); lv_obj_set_style_text_color(label, S.outgoing[i] ? ink_ui_paper() : ink_ui_ink(), 0);
        if (S.outgoing[i]) { label = ink_ui_label(slot, S.states[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + INK_GAP_XL, by + h + INK_GAP_S, w); lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0); }
        size_t at = S.visible - 1 - i;
        S.message_fields[at] = (ink_ui_field_spec_t){.id = S.ids[i], .role = S.outgoing[i] ? "outgoing" : "incoming", .label = S.labels[i], .value = S.texts[i], .object = {S.ids[i], ink_db_last_ver(S.db)}};
        S.message_actions[at + 2] = (ink_ui_action_spec_t){.id = S.ids[i], .label = TR("重发"), .capability = "sms.retry", .args_json = S.args[i], .object = S.message_fields[at].object, .enabled = strstr(S.states[i], TR("发送失败，未发出")) != NULL};
        if (S.message_actions[at + 2].enabled) ink_shell_bind(bubble, "sms.retry", S.args[i]);
    }
    if (!S.visible) { ink_ui_notice(slot, TR("还没有短信"), INK_GAP_L); y = INK_GAP_L; }
    lv_obj_update_layout(slot); lv_display_enable_invalidation(display, true); dirty.y1 += ink_ui_px(y); if (old_top < dirty.y1) dirty.y1 = old_top;
    lv_obj_invalidate_area(slot, &dirty);
    *field = (ink_ui_field_spec_t){.id = "thread", .role = "group", .label = TR("短信")}; *action = (ink_ui_action_spec_t){0};
    lv_obj_t *body = lv_obj_get_parent(slot); lv_obj_t *bar = lv_obj_get_child(body, -1); if (bar != slot) lv_obj_set_y(bar, 0);
}
static void build_rows(bool thread) {
    S.count = 0; S.pages[0] = 0; ink_db_cursor_t cur = {.desc = true}; ink_rec_t r; char peer[24], *text = S.row, status[96];
    while (ink_db_next(S.db, &cur, &r, NULL, 0) == 1 && S.count < SMS_LIMIT) {
        {
            if (!read_message(S.db, r.id, peer, text, status) || (thread && strcmp(peer, S.peer))) continue;
            bool seen = false; if (!thread) for (size_t i = 0; i < S.count; i++) if (!strcmp(S.peers[i], peer)) seen = true;
            if (seen) continue;
            snprintf(S.peers[S.count], sizeof S.peers[0], "%s", peer);
        }
        S.pages[S.count + 1] = S.pages[S.count];
        if (thread) { size_t end = 0; do { end = text_end(text, end); S.pages[S.count + 1]++; } while (text[end]); }
        S.rows[S.count++] = r.id;
    }
    if (thread) {
        int64_t anchor = 0; bool previous = false;
        for (size_t i = S.count; i-- > 0;) {
            read_message(S.db, S.rows[i], peer, text, status); ink_db_get(S.db, S.rows[i], &r, NULL, 0); bool own = strcmp(status, "收到") != 0;
            S.groups[i] = !anchor || own != previous || r.updated_s - anchor > 300;
            if (S.groups[i]) anchor = r.updated_s;
            previous = own;
        }
        thread_layout(SIZE_MAX);
    }
}
static void refresh_thread(void) {
    size_t pages = S.thread_pages; bool latest = S.position.page + 1 == pages; build_rows(true);
    if (latest && pages == S.thread_pages && S.thread_slot) { S.position.page = S.thread_pages - 1; ink_ui_field_spec_t field; ink_ui_action_spec_t action; bind_thread(S.thread_slot, S.position.page, &field, &action, NULL); ink_ui_content_changed(); }
    else { S.position.page = SIZE_MAX; ink_ui_list_refresh(S.list, S.thread_pages); lv_obj_set_y(lv_obj_get_child(lv_obj_get_parent(S.thread_slot), -1), 0); }
    __atomic_store_n(&S.shown_ver, ink_db_last_ver(S.db), __ATOMIC_RELEASE);
}
static void reply_field(lv_obj_t *screen) {
    /* Do not dirty the new field at its temporary origin. */
    lv_display_t *display = lv_obj_get_display(screen); lv_display_enable_invalidation(display, false);
    int bottom = INK_CANVAS_H - INK_MARGIN - INK_CONTROL;
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .max = INK_SMS_TEXT_MAX - 1, .initial = S.draft, .hint = TR("内容")};
    S.field = ink_textfield_create(screen, &cfg, entered, NULL, false); lv_obj_set_pos(S.field, ink_ui_px(INK_MARGIN + INK_PRIMARY + INK_GAP), ink_ui_px(bottom));
    lv_obj_set_width(S.field, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_PRIMARY - 2 * INK_GAP));
    ink_textfield_bind_short(S.field, &S.form, &S.fields[0], &S.actions[0]); lv_obj_add_event_cb(S.field, parts_note, LV_EVENT_VALUE_CHANGED, NULL);
    lv_display_enable_invalidation(display, true); lv_obj_invalidate(S.field);
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg;
    member_ready();
    bool compose = !strcmp(ink_ui_page_name(), COMPOSE.name), lng = !strcmp(ink_ui_page_name(), LONG.name), thread = !strcmp(ink_ui_page_name(), THREAD.name);
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, compose || lng ? TR("写短信") : thread ? "" : TR("短信"));
    if (thread) { char name[97], number[25]; bool known = ink_contacts_match(S.peer, true, name, number) > 0; ink_ui_line(screen, known ? name : S.peer, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN + INK_CONTROL + INK_GAP, INK_STATUS_BAR + INK_GAP_L, INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP); }
    S.notice = ink_ui_notice(screen, thread ? "" : S.result, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY - INK_CONTROL - 2 * INK_GAP_L - INK_LINE_NOTE);
    S.form = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = compose || lng ? TR("写短信") : TR("短信"), .object = {"sms.messages", 1}, .actions = S.actions, .action_count = 3};
    S.actions[0] = (ink_ui_action_spec_t){.id = "prev", .label = TR("上一页"), .capability = "sms.prev"};
    S.actions[1] = (ink_ui_action_spec_t){.id = "next", .label = TR("下一页"), .capability = "sms.next"};
    S.actions[2] = (ink_ui_action_spec_t){.id = "compose", .label = TR("写短信"), .capability = "sms.compose", .enabled = true};
    if (lng) {
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .max = INK_SMS_TEXT_MAX - 1, .initial = S.draft, .draft_page = LONG.name, .draft_id = !strcmp(S.to, S.draft_ref.to) ? S.draft_ref.id : 0}; S.field = ink_textfield_long_create(screen, &cfg);
        if (S.field) { S.draft_ref.id = ink_text_draft_id(); snprintf(S.draft_ref.to, sizeof S.draft_ref.to, "%s", S.to); ink_app_save_state("message"); }
        lv_obj_t *done = ink_ui_pill(screen, TR("返回"), INK_CANVAS_W - INK_MARGIN - INK_PRIMARY, INK_STATUS_BAR, INK_PRIMARY, INK_CONTROL, false, NULL, NULL); ink_shell_bind(done, "shell.back", "{}"); return;
    }
    if (compose || thread) {
        int bottom = INK_CANVAS_H - INK_MARGIN - INK_CONTROL;
        S.actions[0] = (ink_ui_action_spec_t){.id = "text", .label = TR("输入短信"), .capability = "sms.text", .enabled = true};
        S.actions[1] = (ink_ui_action_spec_t){.id = "long", .label = TR("展开"), .capability = "sms.long", .enabled = true};
        S.actions[3] = (ink_ui_action_spec_t){.id = "to", .label = TR("输入收件人"), .capability = "sms.to", .enabled = compose};
        S.actions[4] = (ink_ui_action_spec_t){.id = "pick", .label = TR("从联系人选择"), .capability = "sms.pick", .enabled = compose};
        S.form.fields = S.fields; S.form.field_count = compose ? 2 : 1; S.form.action_count = compose ? 6 : 3;
        S.fields[0] = (ink_ui_field_spec_t){.id = "text", .role = "textbox", .label = TR("短信内容")};
        reply_field(screen);
        S.parts = ink_ui_line(screen, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, bottom - INK_LINE_NOTE - INK_GAP, INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_set_y(S.notice, ink_ui_px(thread ? bottom - INK_LINE_NOTE - INK_GAP : bottom - INK_LINE_NOTE - 2 * INK_GAP - 2 * INK_LINE_NOTE));
        lv_obj_t *expand = ink_ui_pill(screen, TR("展开"), INK_MARGIN, bottom, INK_PRIMARY, INK_CONTROL, false, NULL, NULL); ink_shell_bind(expand, "sms.long", "{}");
        S.send = ink_ui_pill(screen, TR("发送"), INK_CANVAS_W - INK_MARGIN - INK_PRIMARY, bottom, INK_PRIMARY, INK_CONTROL, true, send_clicked, NULL); parts_note(NULL);
        if (compose) {
            int top = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L;
            ink_ui_line(screen, TR("收件人"), INK_FONT_NOTE, INK_WEIGHT_MEDIUM, INK_MARGIN, top, INK_CANVAS_W - 2 * INK_MARGIN);
            S.fields[1] = (ink_ui_field_spec_t){.id = "to", .role = "textbox", .label = TR("收件人")};
            ink_text_cfg_t to = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .max = 20, .initial = S.to, .hint = TR("姓名或电话号码")};
            S.recipient = ink_textfield_create(screen, &to, NULL, NULL, false); lv_obj_set_pos(S.recipient, ink_ui_px(INK_MARGIN), ink_ui_px(top + INK_LINE_NOTE + INK_GAP));
            lv_obj_set_width(S.recipient, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN - INK_CONTROL - INK_GAP));
            ink_textfield_bind_short(S.recipient, &S.form, &S.fields[1], &S.actions[3]); lv_obj_add_event_cb(S.recipient, recipient_edited, LV_EVENT_VALUE_CHANGED, NULL);
            ink_shell_bind(ink_ui_pill(screen, "+", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, top + INK_LINE_NOTE + INK_GAP, INK_CONTROL, INK_CONTROL, false, NULL, NULL), "sms.pick", "{}");
            S.suggestion = ink_ui_pill(screen, "", INK_MARGIN, top + INK_LINE_NOTE + INK_CONTROL + 2 * INK_GAP, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, NULL, NULL); recipient_edited(NULL);
            if (ink_contacts_match("", false, S.match_name, S.match_number) < 0) {
                ink_args_t *a = ink_app_alloc(sizeof *a, true);
                if (a) { ink_args_init(a); ink_args_str(a, "collection", "phone.contacts"); ink_args_int(a, "limit", 1); request("data.list", a); ink_app_free(a); }
            }
            return;
        }
    }
    S.db = ink_db_open_cached_member(ink_db_find_def("sms.messages"), S.member);
    __atomic_store_n(&S.shown_ver, S.db ? ink_db_last_ver(S.db) : 0, __ATOMIC_RELEASE);
    if (!S.db) {
        ink_args_t *a = ink_app_alloc(sizeof *a, true);
        if (a) {
            ink_args_init(a); ink_args_str(a, "collection", !S.db ? "sms.messages" : "phone.contacts");
            ink_args_int(a, "limit", 1); request("data.list", a); ink_app_free(a);
            /* Coalesce a quick cold inbox read with the first screen refresh. */
            if (!compose && !thread) lv_timer_reset(lv_display_get_refr_timer(lv_obj_get_display(screen)));
        }
        return;
    }
    int top = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L; S.body_h = thread ? INK_CANVAS_H - INK_MARGIN - 2 * INK_CONTROL - top - 2 * INK_GAP - INK_LINE_NOTE : INK_ROW_DOUBLE; build_rows(thread);
    lv_obj_t *body = ink_ui_box(screen, 0, top, INK_CANVAS_W, thread ? S.body_h + INK_CONTROL + INK_GAP : INK_CANVAS_H - top - INK_PRIMARY - INK_MARGIN, 0, INK_SURFACE_WHITE, false);
    ink_ui_action_spec_t pages[] = {{.id = "prev", .label = TR("上一页"), .capability = "sms.prev"}, {.id = "next", .label = TR("下一页"), .capability = "sms.next"}, {.id = "compose", .label = TR("新建"), .capability = "sms.compose", .enabled = true}};
    _Static_assert(sizeof pages / sizeof *pages <= sizeof S.actions / sizeof *S.actions, "SMS page actions");
    ink_ui_page_spec_t model = S.form; model.actions = pages; model.action_count = 3;
    S.list = ink_ui_list_create(body, &model, &S.position, thread ? 1 : SMS_ROWS, 1, S.body_h, TR("还没有短信"), thread ? bind_thread : bind_item, NULL); if (S.list) ink_ui_list_refresh(S.list, thread ? S.thread_pages : S.count);
    if (thread) lv_obj_set_y(lv_obj_get_child(body, -1), 0);
    if (!thread) { lv_obj_t *b = ink_ui_pill(screen, TR("新建"), INK_CANVAS_W - INK_MARGIN - INK_PRIMARY, INK_STATUS_BAR, INK_PRIMARY, INK_CONTROL, false, NULL, NULL); ink_shell_bind(b, "sms.compose", "{}"); }
}
static void destroy(void) {
    if (S.field) snprintf(S.draft, sizeof S.draft, "%s", ink_textfield_text(S.field));
    __atomic_store_n(&S.shown_ver, 0, __ATOMIC_RELEASE);
    ink_db_close(S.db); S.db = NULL; S.list = NULL; S.thread_slot = NULL; S.field = S.recipient = S.suggestion = S.notice = S.send = S.parts = NULL;
}
static int snapshot(char *out, size_t cap) {
    if (strcmp(S.member, ink_member_current()->id)) { if (cap) out[0] = 0; return -1; }
    if (S.list && !S.field) return ink_ui_list_snapshot(S.list, out, cap);
    if (S.field && !strcmp(ink_ui_page_name(), LONG.name)) return ink_textfield_long_snapshot(S.field, out, cap);
    if (S.field) {
        char *args = S.send_args; ink_jw_t w; ink_jw_init(&w, args, sizeof S.send_args); ink_jw_raw(&w, "{\"to\":"); ink_jw_str(&w, S.to); ink_jw_raw(&w, ",\"text\":"); ink_jw_str(&w, ink_textfield_text(S.field)); ink_jw_raw(&w, "}");
        const char *note = S.parts ? lv_label_get_text(S.parts) : ""; snprintf(S.send_label, sizeof S.send_label, note[0] ? TR("发送短信，%s") : TR("发送短信%s"), note); /* the AI sees the parts too */
        S.actions[2] = (ink_ui_action_spec_t){.id = "send", .label = S.send_label, .capability = "message.send", .args_json = args, .enabled = !w.overflow && S.to[0] && ink_textfield_text(S.field)[0] && !lv_obj_has_state(S.send, LV_STATE_DISABLED)}; S.form.action_count = S.recipient ? 6 : 3;
        int n = ink_textfield_short_snapshot(S.recipient && lv_obj_has_state(S.recipient, LV_STATE_FOCUSED) ? S.recipient : S.field, out, cap);
        if (n < 0) return -1;
        w = (ink_jw_t){.buf = out, .cap = cap, .len = (size_t)n - 1};
        ink_jw_raw(&w, ",\"send_status\":"); ink_jw_str(&w, S.notice ? lv_label_get_text(S.notice) : "");
        if (S.list) { int m = ink_ui_semantics_snapshot(&S.messages, S.list_snapshot, sizeof S.list_snapshot);
            if (m < 0) return -1;
            int extra = snprintf(S.list_snapshot + m - 1, sizeof S.list_snapshot - (size_t)m + 1, ",\"pagination\":{\"page\":%zu,\"pages\":%zu,\"total\":%zu,\"visible\":%zu}}", S.position.page + 1, S.thread_pages, S.count, S.visible);
            if (extra < 0 || (size_t)extra >= sizeof S.list_snapshot - (size_t)m + 1) return -1;
            ink_jw_raw(&w, ",\"messages\":"); ink_jw_raw(&w, S.list_snapshot); }
        ink_jw_raw(&w, "}"); return w.overflow ? -1 : (int)w.len;
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
static const ink_app_def_t APP = {.id = "message", .title = TR_NOOP("短信"), .icon = INK_UI_ICON(message), .entry = &LIST, .system_app = true, .child_ok = true, .ops = &OPS};
static const ink_cap_t CAPS[] = {
    {.id = "sms.to", .app = "message", .title = "输入短信收件人号码", .activity = "SmsCompose", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.pick", .app = "message", .title = "打开短信联系人选择", .activity = "SmsCompose", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.retry", .app = "message", .title = "重新编辑明确未发出的短信", .activity = "SmsThread", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.thread", .app = "message", .title = "打开短信会话", .input = "{\"type\":\"object\",\"properties\":{\"peer\":{\"type\":\"string\",\"maxLength\":20}},\"required\":[\"peer\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.compose", .app = "message", .title = "写短信或回复当前会话", .input = "{\"type\":\"object\",\"properties\":{\"number\":{\"type\":\"string\",\"maxLength\":20}}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.recipient", .app = "message", .title = "从联系人选择收件人", .activity = "SmsCompose", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.text", .app = "message", .title = "输入短信", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.long", .app = "message", .title = "打开短信长文编辑页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.prev", .app = "message", .title = "短信上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "sms.next", .app = "message", .title = "短信下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}
};
int ink_sms_init(void) {
    return ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_cap_subscribe("message.received", received, NULL, INK_EXEC_BG) || ink_cap_subscribe("contacts.selected", contact_selected, NULL, INK_EXEC_UI) ? -1 : 0;
}
