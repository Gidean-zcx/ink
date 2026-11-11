/* Contacts use the member store, the capability bus and the shared F1/F2/F4 hosts. */
#include <stdio.h>
#include <string.h>
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

static const ink_db_def_t CONTACTS = {.name = "phone.contacts", .title = "联系人", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_records = 200};
enum { CONTACT_ROWS = 4, CONTACT_LIMIT = 200, CONTACT_FIELDS = 3 };
static const ink_activity_t LIST, EDIT, DETAIL;
extern void ink_phone_tabs(lv_obj_t *screen, int selected);
static struct {
    ink_db_t *db;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    char member[16], name[97], number[25], note[193], query[97], ids[CONTACT_ROWS][32], args[CONTACT_ROWS][48], names[CONTACT_ROWS][97], numbers[CONTACT_ROWS][25];
    uint32_t rows[CONTACT_LIMIT]; size_t count;
    bool picking, phone;
    uint32_t id, ver;
    lv_obj_t *fields[CONTACT_FIELDS], *notice, *search;
    ink_ui_field_spec_t specs[CONTACT_FIELDS];
    ink_ui_page_spec_t form;
    ink_ui_action_spec_t actions[6];
    char save_args[2048], record_args[48], delete_args[384], dial_args[64];
    ink_exec_job_t refresh;
} P INK_PSRAM_BSS;

static void read_note(ink_db_t *db, uint32_t id, char *note) {
    char raw[1024] = {0}; ink_json_t *j = ink_app_alloc(sizeof *j, true); note[0] = 0;
    if (j && ink_db_get(db, id, NULL, raw, sizeof raw - 1) == INK_DB_OK && !ink_json_parse(j, raw, strlen(raw))) ink_json_str(j, ink_json_find(j, 0, "note"), note, 193);
    ink_app_free(j);
}

static uint32_t read_contact(ink_db_t *db, uint32_t id, char *name, char *number) {
    char raw[1024] = {0};
    ink_rec_t rec = {0};
    ink_json_t *j = ink_app_alloc(sizeof *j, true);
    bool ok = j && ink_db_get(db, id, &rec, raw, sizeof raw - 1) == INK_DB_OK &&
        ink_json_parse(j, raw, strlen(raw)) == 0 &&
        ink_json_str(j, ink_json_find(j, 0, "name"), name, 97) > 0 &&
        ink_json_str(j, ink_json_find(j, 0, "number"), number, 25) > 0;
    ink_app_free(j);
    return ok ? rec.ver : 0;
}
/* Cached member data only: safe in the UI task, shared by the two number fields. */
int ink_contacts_match(const char *query, bool exact, char *name, char *number) {
    ink_db_t *db = ink_db_open_cached_member(&CONTACTS, ink_member_current()->id);
    if (!db) return -1;
    ink_db_cursor_t cur = {0}; ink_rec_t r; int found = 0;
    while (query[0] && ink_db_next(db, &cur, &r, NULL, 0) == 1) {
        if (!read_contact(db, r.id, name, number)) continue;
        if (exact ? !strcmp(number, query) : strstr(name, query) || strstr(number, query)) { found = (int)r.id; break; }
    }
    ink_db_close(db); return found;
}
static void build_rows(void) {
    ink_db_cursor_t cur = {0}; ink_rec_t r; char name[97], number[25], before[97], unused[25]; P.count = 0;
    while (ink_db_next(P.db, &cur, &r, NULL, 0) == 1 && P.count < CONTACT_LIMIT) {
        if (!read_contact(P.db, r.id, name, number) || (P.query[0] && !strstr(name, P.query) && !strstr(number, P.query))) continue;
        size_t at = P.count++;
        while (at && read_contact(P.db, P.rows[at - 1], before, unused) && strcmp(before, name) > 0) { P.rows[at] = P.rows[at - 1]; at--; }
        P.rows[at] = r.id;
    }
}
static void searched(lv_event_t *e) {
    snprintf(P.query, sizeof P.query, "%s", ink_textfield_text(lv_event_get_target_obj(e)));
    P.position.page = 0; build_rows(); if (P.list) ink_ui_list_refresh(P.list, P.count);
}
static void refresh(void *arg) {
    (void)arg;
    if (!P.db || !ink_ui_page_current() || strcmp(P.member, ink_member_current()->id)) return;
    if (!strcmp(ink_ui_page_name(), EDIT.name) && P.id && ink_db_get(P.db, P.id, NULL, NULL, 0) != INK_DB_OK) ink_ui_pop();
    else if (!strcmp(ink_ui_page_name(), LIST.name)) ink_ui_redraw();
}
static void record(ink_call_t *c) { /* BG: first load and durable writes may wait. */
    int64_t id = 0, expected = 0;
    ink_args_get_int(&c->args, "id", &id);
    ink_args_get_int(&c->args, "if_revision", &expected);
    ink_db_t *db = ink_db_open_member(&CONTACTS, ink_cap_call_member(c));
    if (!db) { c->rc.status = INK_RC_FAILED; return; }
    char name[97] = "", number[25] = "", note[193] = "", raw[1024];
    bool put = !strcmp(c->cap->id, "contacts.put"), del = !strcmp(c->cap->id, "contacts.delete");
    uint32_t ver = id ? read_contact(db, (uint32_t)id, name, number) : 0;
    if ((!put && !id) || (id && !ver)) c->rc.status = INK_RC_INVALID;
    if (expected && expected != ver) c->rc.status = INK_RC_STALE;
    if (put && c->rc.status == INK_RC_DONE) {
        if (id) read_note(db, (uint32_t)id, note);
        if (ink_args_find(&c->args, "note")) ink_args_get_str(&c->args, "note", note, sizeof note);
        int n = ink_args_get_str(&c->args, "name", name, sizeof name);
        int length = ink_args_get_str(&c->args, "number", number, sizeof number);
        bool valid = n > 0 && n < (int)sizeof name && length > 0 && length < (int)sizeof number;
        bool digit = false;
        for (int k = 0; k < length && valid; k++) { if (number[k] >= '0' && number[k] <= '9') digit = true; else valid = strchr(" -()", number[k]) || (k == 0 && number[k] == '+'); }
        valid = valid && digit;
        if (!valid) c->rc.status = INK_RC_INVALID;
        else {
            ink_jw_t w; ink_jw_init(&w, raw, sizeof raw);
            ink_jw_raw(&w, "{\"name\":"); ink_jw_str(&w, name); ink_jw_raw(&w, ",\"number\":"); ink_jw_str(&w, number); ink_jw_raw(&w, ",\"note\":"); ink_jw_str(&w, note); ink_jw_raw(&w, "}");
            uint32_t got = 0;
            ink_db_write_t wr = {.id = (uint32_t)id, .data = raw, .len = w.len, .src = c->src, .written_ver = &c->rc.revision};
            if (w.overflow || ink_db_put(db, &wr, &got) != INK_DB_OK) c->rc.status = INK_RC_FAILED;
            else id = got;
        }
    } else if (del && c->rc.status == INK_RC_DONE) {
        const ink_arg_t *target_name = ink_args_find(&c->args, "name"), *target_number = ink_args_find(&c->args, "number");
        if (strcmp(name, c->args.text + target_name->off) || strcmp(number, c->args.text + target_number->off)) c->rc.status = INK_RC_STALE;
        else if (ink_db_del_if_ver(db, (uint32_t)id, c->src, (uint32_t)expected) != INK_DB_OK) c->rc.status = INK_RC_STALE;
        c->rc.revision = ink_db_last_ver(db);
    }
    if (c->rc.status == INK_RC_DONE && (put || del)) {
        if (ink_db_wait(db, c->rc.revision, (int64_t)ink_cap_time_left_ms(c) * 1000) != 0) c->rc.status = INK_RC_FAILED;
        ink_exec_post(INK_EXEC_UI, &P.refresh, refresh, NULL);
    }
    if (c->rc.status == INK_RC_DONE) {
        snprintf(c->rc.object, sizeof c->rc.object, "contact:%u", (unsigned)id);
        ink_jw_t w; ink_jw_init(&w, raw, sizeof raw);
        ink_jw_raw(&w, "{\"id\":"); ink_jw_int(&w, id); ink_jw_raw(&w, ",\"number\":"); ink_jw_str(&w, number); ink_jw_raw(&w, "}"); ink_cap_result(c, raw);
        if (!put && !del) { ink_args_str(&c->args, "number", number); ink_cap_emit_for("contacts.selected", &c->args, ink_cap_call_member(c)); }
    }
    snprintf(c->rc.say, sizeof c->rc.say, "%s", c->rc.status == INK_RC_STALE ? TR("联系人已被修改，请返回列表重新打开后重试") : c->rc.status != INK_RC_DONE ? TR("没有完成，请检查姓名和号码后重试") : put ? TR("已保存联系人") : del ? TR("已删除联系人") : TR("已选择这个号码"));
    ink_db_close(db);
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != ink_ui_generation() || !ink_ui_page_current()) return;
    if (rc->status == INK_RC_DONE) {
        if (rc->revision && !strcmp(ink_ui_page_name(), EDIT.name)) ink_ui_pop();
        else ink_ui_redraw();
        if (P.notice && rc->say[0]) lv_label_set_text(P.notice, rc->say);
    }
    else if (P.notice) lv_label_set_text(P.notice, rc->say[0] ? rc->say : TR("暂时打不开联系人，请返回重试"));
}
static void request(const char *cap, const ink_args_t *args) {
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .member = P.member, .reply_exec = INK_EXEC_UI,
        .done = reply, .ctx = (void *)(uintptr_t)ink_ui_generation()};
    if (ink_cap_call(cap, args, &opts) != 0 && P.notice) lv_label_set_text(P.notice, TR("没有完成，请重试"));
}
static void clicked(lv_event_t *e) {
    const char *cap = lv_event_get_user_data(e);
    ink_args_t *a = ink_app_alloc(sizeof *a, true);
    if (!a) return;
    ink_args_init(a); if (P.id) ink_args_int(a, "id", P.id);
    if (P.id && strcmp(cap, "contacts.select")) ink_args_int(a, "if_revision", P.ver);
    if (!strcmp(cap, "contacts.put")) {
        ink_args_str(a, "name", ink_textfield_text(P.fields[0])); ink_args_str(a, "number", ink_textfield_text(P.fields[1]));
        ink_args_str(a, "note", ink_textfield_text(P.fields[2]));
    } else if (!strcmp(cap, "contacts.delete")) { ink_args_str(a, "name", P.name); ink_args_str(a, "number", P.number); }
    request(cap, a); ink_app_free(a);
}
static void navigate(ink_call_t *c) {
    const char *id = c->cap->id;
    if (!strcmp(id, "contacts.open") || !strcmp(id, "contacts.pick")) {
        P.query[0] = 0; P.position.page = 0; ink_ui_drop_app("contacts"); ink_ui_push(&LIST, !strcmp(id, "contacts.pick") ? (void *)1 : NULL);
    } else if (!strcmp(id, "contacts.edit") || !strcmp(id, "contacts.view")) {
        int64_t selected = 0; ink_args_get_int(&c->args, "id", &selected);
        if (!strcmp(ink_ui_page_name(), DETAIL.name)) ink_ui_pop();
        ink_ui_push(!strcmp(id, "contacts.view") ? &DETAIL : &EDIT, (void *)(uintptr_t)selected);
    } else if (!strcmp(id, "contacts.next")) { if (P.list) ink_ui_list_turn(P.list, 1); }
    else if (!strcmp(id, "contacts.prev")) { if (P.list) ink_ui_list_turn(P.list, -1); }
    else if (!strcmp(id, "contacts.search")) { if (!P.search || ink_textfield_short_focus(P.search)) c->rc.status = INK_RC_STALE; }
    else if (!P.fields[0] || ink_textfield_short_focus(P.fields[!strcmp(id, "contacts.note") ? 2 : !strcmp(id, "contacts.number")]) != 0) c->rc.status = INK_RC_STALE;
}
static void bind_item(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    unsigned at = index % CONTACT_ROWS;
    uint32_t id = P.rows[index], ver = read_contact(P.db, id, P.names[at], P.numbers[at]);
    snprintf(P.ids[at], sizeof P.ids[at], "contact:%u", (unsigned)id);
    snprintf(P.args[at], sizeof P.args[at], "{\"id\":%u}", (unsigned)id);
    lv_obj_set_y(slot, ink_ui_px(at * INK_ROW_DOUBLE));
    lv_obj_set_style_bg_opa(slot, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(slot, 0, 0);
    lv_obj_set_style_radius(slot, 0, 0);
    if (!lv_obj_get_child_count(slot)) {
        int x = INK_GAP + INK_CIRCLE + INK_GAP, width = INK_CANVAS_W - 2 * INK_MARGIN - x - INK_GAP;
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, x, INK_GAP_S, width);
        ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, x, INK_LINE_LIST + INK_GAP_S, width);
        lv_obj_t *avatar = ink_ui_box(slot, INK_GAP, INK_GAP_M, INK_CIRCLE, INK_CIRCLE, -1, INK_SURFACE_WHITE, false);
        lv_obj_t *initial = ink_ui_line(avatar, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, INK_GAP, INK_CIRCLE);
        lv_obj_set_style_text_align(initial, LV_TEXT_ALIGN_CENTER, 0);
    }
    size_t first = 1; while (P.names[at][first] && (P.names[at][first] & 0xC0) == 0x80) first++;
    char initial[5]; snprintf(initial, sizeof initial, "%.*s", (int)first, P.names[at]);
    lv_label_set_text(lv_obj_get_child(lv_obj_get_child(slot, 2), 0), initial);
    lv_label_set_text(lv_obj_get_child(slot, 0), P.names[at]); lv_label_set_text(lv_obj_get_child(slot, 1), P.numbers[at]);
    *field = (ink_ui_field_spec_t){.id = P.ids[at], .role = "listitem", .label = P.names[at], .value = P.numbers[at], .object = {P.ids[at], ver}};
    *action = (ink_ui_action_spec_t){.id = P.ids[at], .label = TR("查看联系人"), .capability = P.picking ? "contacts.select" : "contacts.view", .args_json = P.args[at], .object = field->object, .enabled = true};
}
static void create(lv_obj_t *screen, void *arg) {
    const ink_ui_action_spec_t ACTIONS[] = {
        {.id = "prev", .label = TR("上一页"), .capability = "contacts.prev"},
        {.id = "next", .label = TR("下一页"), .capability = "contacts.next"},
        {.id = "search", .label = TR("搜索联系人"), .capability = "contacts.search", .enabled = true}
    };
    const ink_ui_page_spec_t MODEL = {.id = "PhoneContacts", .title = TR("联系人"), .object = {"phone.contacts", 1}, .actions = ACTIONS, .action_count = 3};
    P.actions[0] = (ink_ui_action_spec_t){.id = "name", .label = TR("输入姓名"), .capability = "contacts.name", .enabled = true};
    P.actions[1] = (ink_ui_action_spec_t){.id = "number", .label = TR("输入号码"), .capability = "contacts.number", .enabled = true};
    P.actions[2] = (ink_ui_action_spec_t){.id = "note", .label = TR("输入备注"), .capability = "contacts.note", .enabled = true};
    bool edit = !strcmp(ink_ui_page_name(), EDIT.name);
    bool detail = !strcmp(ink_ui_page_name(), DETAIL.name);
    if (strcmp(P.member, ink_member_current()->id)) { P.query[0] = 0; memset(&P.position, 0, sizeof P.position); snprintf(P.member, sizeof P.member, "%s", ink_member_current()->id); }
    if (!edit && !detail) { P.picking = arg == (void *)1; P.phone = arg == (void *)2; }
    P.id = edit || detail ? (uint32_t)(uintptr_t)arg : 0;
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, edit ? P.id ? TR("编辑联系人") : TR("新建联系人") : detail ? TR("联系人详情") : P.phone ? "" : TR("联系人"));
    if (!edit && !detail && P.phone) ink_phone_tabs(screen, 2);
    P.notice = ink_ui_notice(screen, "", INK_CANVAS_H - INK_PRIMARY - INK_CONTROL - INK_GAP_L);
    P.db = ink_db_open_cached_member(&CONTACTS, P.member);
    if (!P.db) {
        lv_label_set_text(P.notice, TR("正在读取联系人"));
        ink_args_t *a = ink_app_alloc(sizeof *a, true);
        if (a) { ink_args_init(a); ink_args_str(a, "collection", CONTACTS.name); ink_args_int(a, "limit", 1); request("data.list", a); ink_app_free(a); }
        return;
    }
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_L;
    if (!edit && !detail) {
        P.form = MODEL; P.form.fields = P.specs; P.form.field_count = 1; P.actions[0] = ACTIONS[2]; P.form.actions = P.actions; P.form.action_count = 2; P.actions[1] = (ink_ui_action_spec_t){.id = "add", .label = TR("添加联系人"), .capability = "contacts.edit", .enabled = true};
        P.specs[0] = (ink_ui_field_spec_t){.id = "query", .role = "searchbox", .label = TR("搜索联系人")};
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .initial = P.query, .hint = TR("搜索姓名或号码"), .max = 96};
        P.search = ink_textfield_create(screen, &cfg, NULL, NULL, false);
        lv_obj_set_pos(P.search, ink_ui_px(INK_MARGIN), ink_ui_px(y)); lv_obj_set_width(P.search, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
        ink_textfield_bind_short(P.search, &P.form, &P.specs[0], &P.actions[0]); lv_obj_add_event_cb(P.search, searched, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_t *body = ink_ui_box(screen, 0, y + INK_CONTROL + INK_GAP_XS, INK_CANVAS_W, CONTACT_ROWS * (INK_ROW_DOUBLE + INK_GAP_M) + INK_CONTROL + INK_GAP_L, 0, INK_SURFACE_WHITE, false);
        build_rows(); P.list = ink_ui_list_create(body, &MODEL, &P.position, CONTACT_ROWS, 1, INK_ROW_DOUBLE, TR("没有匹配的联系人"), bind_item, NULL);
        if (P.list) ink_ui_list_refresh(P.list, P.count);
        lv_obj_t *button = ink_ui_pill(screen, TR("添加联系人"), INK_MARGIN, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN, INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, false, NULL, NULL);
        ink_shell_bind(button, "contacts.edit", "{}");
    } else {
        P.name[0] = P.number[0] = P.note[0] = 0;
        P.ver = P.id ? read_contact(P.db, P.id, P.name, P.number) : 0;
        if (P.id && !P.ver) { lv_label_set_text(P.notice, TR("找不到这个联系人")); return; }
        snprintf(P.ids[0], sizeof P.ids[0], "contact:%u", (unsigned)P.id);
        if (P.id) read_note(P.db, P.id, P.note);
        P.form = (ink_ui_page_spec_t){.id = ink_ui_page_name(), .title = detail ? TR("联系人详情") : P.id ? TR("编辑联系人") : TR("新建联系人"), .object = {P.ids[0], P.ver}, .fields = P.specs, .field_count = CONTACT_FIELDS, .actions = P.actions, .action_count = CONTACT_FIELDS};
        const char *values[] = {P.name, P.number, P.note}, *labels[] = {TR("姓名"), TR("号码"), TR("备注")}, *ids[] = {"name", "number", "note"};
        _Static_assert(sizeof values / sizeof *values <= CONTACT_FIELDS, "contact fields");
        _Static_assert(sizeof labels / sizeof *labels <= CONTACT_FIELDS, "contact labels");
        _Static_assert(sizeof ids / sizeof *ids <= CONTACT_FIELDS, "contact ids");
        if (detail) {
            for (int i = 0; i < CONTACT_FIELDS; i++) { ink_ui_line(screen, values[i], i ? INK_FONT_LIST : INK_FONT_TITLE, INK_WEIGHT_MEDIUM, INK_MARGIN, y + i * (INK_CONTROL + INK_GAP_L), INK_CANVAS_W - 2 * INK_MARGIN); P.specs[i] = (ink_ui_field_spec_t){.id = ids[i], .role = "text", .label = labels[i], .value = values[i]}; }
            snprintf(P.record_args, sizeof P.record_args, "{\"id\":%u}", (unsigned)P.id); char number[sizeof P.number]; snprintf(number, sizeof number, "%s", P.number); snprintf(P.dial_args, sizeof P.dial_args, "{\"number\":\"%.24s\"}", number);
            const char *caps[] = {"call.dial", "sms.compose", "contacts.edit"}, *titles[] = {TR("打电话"), TR("发短信"), TR("编辑")};
            _Static_assert(sizeof caps / sizeof *caps <= sizeof P.actions / sizeof *P.actions, "detail actions");
            _Static_assert(sizeof titles / sizeof *titles <= sizeof P.actions / sizeof *P.actions, "detail labels");
            for (int i = 0; i < 3; i++) { const char *args = i == 2 ? P.record_args : P.dial_args; P.actions[i] = (ink_ui_action_spec_t){.id = caps[i], .label = titles[i], .capability = caps[i], .args_json = args, .enabled = true}; ink_shell_bind(ink_ui_pill(screen, titles[i], INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY - (2 - i) * (INK_CONTROL + INK_GAP_L), INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, i == 0, NULL, NULL), caps[i], args); }
            return;
        }
        for (int i = 0; i < CONTACT_FIELDS; i++) {
            ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .initial = values[i], .hint = labels[i], .max = i == 1 ? 24 : i == 2 ? 192 : 96};
            P.specs[i] = (ink_ui_field_spec_t){.id = ids[i], .role = "textbox", .label = labels[i]};
            P.fields[i] = ink_textfield_create(screen, &cfg, NULL, NULL, false);
            lv_obj_set_pos(P.fields[i], ink_ui_px(INK_MARGIN), ink_ui_px(y + i * (INK_CONTROL + INK_GAP_L)));
            lv_obj_set_width(P.fields[i], ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
            ink_textfield_bind_short(P.fields[i], &P.form, &P.specs[i], &P.actions[i]);
        }
        int bottom = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY;
        ink_ui_pill(screen, TR("保存联系人"), INK_MARGIN, bottom, INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, clicked, "contacts.put");
        if (P.id) {
            ink_ui_pill(screen, TR("删除联系人"), INK_MARGIN, bottom - INK_CONTROL - INK_GAP_L, INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, clicked, "contacts.delete");
            ink_ui_pill(screen, TR("选择这个号码"), INK_MARGIN, bottom - 2 * (INK_CONTROL + INK_GAP_L), INK_CANVAS_W - 2 * INK_MARGIN, INK_CONTROL, false, clicked, "contacts.select");
        }
    }
}
static void destroy(void) { ink_db_close(P.db); P.db = NULL; P.list = NULL; P.notice = P.search = NULL; memset(P.fields, 0, sizeof P.fields); }
static int snapshot(char *out, size_t cap) {
    if (P.list) {
        int n = ink_ui_list_snapshot(P.list, out, cap), m = P.search ? ink_textfield_short_snapshot(P.search, P.save_args, sizeof P.save_args) : -1;
        if (n < 0 || m < 0) return -1;
        ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)n - 1}; ink_jw_raw(&w, ",\"search\":"); ink_jw_raw(&w, P.save_args); ink_jw_raw(&w, "}"); return w.overflow ? -1 : (int)w.len;
    }
    if (!strcmp(ink_ui_page_name(), DETAIL.name)) return ink_ui_semantics_snapshot(&P.form, out, cap);
    if (P.fields[0]) {
        ink_jw_t w; ink_jw_init(&w, P.save_args, sizeof P.save_args);
        ink_jw_raw(&w, "{");
        if (P.id) { ink_jw_raw(&w, "\"id\":"); ink_jw_int(&w, P.id); ink_jw_raw(&w, ",\"if_revision\":"); ink_jw_int(&w, P.ver); ink_jw_raw(&w, ","); }
        ink_jw_raw(&w, "\"name\":"); ink_jw_str(&w, P.specs[0].value); ink_jw_raw(&w, ",\"number\":"); ink_jw_str(&w, P.specs[1].value); ink_jw_raw(&w, ",\"note\":"); ink_jw_str(&w, P.specs[2].value); ink_jw_raw(&w, "}");
        snprintf(P.record_args, sizeof P.record_args, "{\"id\":%u}", (unsigned)P.id);
        P.actions[3] = (ink_ui_action_spec_t){.id = "save", .label = TR("保存联系人"), .capability = "contacts.put", .args_json = P.save_args, .enabled = !w.overflow};
        P.actions[4] = (ink_ui_action_spec_t){.id = "select", .label = TR("选择这个号码"), .capability = "contacts.select", .args_json = P.record_args, .enabled = P.id != 0};
        ink_jw_init(&w, P.delete_args, sizeof P.delete_args);
        ink_jw_raw(&w, "{\"id\":"); ink_jw_int(&w, P.id); ink_jw_raw(&w, ",\"if_revision\":"); ink_jw_int(&w, P.ver);
        ink_jw_raw(&w, ",\"name\":"); ink_jw_str(&w, P.name); ink_jw_raw(&w, ",\"number\":"); ink_jw_str(&w, P.number); ink_jw_raw(&w, "}");
        P.actions[5] = (ink_ui_action_spec_t){.id = "delete", .label = TR("删除联系人"), .capability = "contacts.delete", .args_json = P.delete_args, .enabled = P.id != 0 && !w.overflow};
        P.form.actions = P.actions; P.form.action_count = 6;
        unsigned focused = lv_obj_has_state(P.fields[2], LV_STATE_FOCUSED) ? 2 : lv_obj_has_state(P.fields[1], LV_STATE_FOCUSED) ? 1 : 0;
        return ink_textfield_short_snapshot(P.fields[focused], out, cap);
    }
    return snprintf(out, cap, TR("联系人：%s"), P.notice ? lv_label_get_text(P.notice) : TR("暂时打不开"));
}
static bool page_key(ink_key_t k, bool lng) { return P.list && ink_ui_list_key(P.list, k, lng); }
static const ink_activity_t LIST = {.name = "PhoneContacts", .app = "contacts", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
static const ink_activity_t EDIT = {.name = "ContactEdit", .app = "contacts", .create = create, .destroy = destroy, .snapshot = snapshot};
static const ink_activity_t DETAIL = {.name = "ContactDetail", .app = "contacts", .create = create, .destroy = destroy, .snapshot = snapshot};
extern const ink_activity_t ink_phone_page;
static const ink_app_def_t APP = {.id = "phone", .title = TR_NOOP("电话"), .icon = INK_UI_ICON(phone), .entry = &ink_phone_page, .system_app = true, .child_ok = true};
static const ink_app_def_t CONTACT_APP = {.id = "contacts", .title = TR_NOOP("联系人"), .icon = INK_UI_ICON(person), .entry = &LIST, .system_app = true, .child_ok = true};
#define ID "\"id\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}"
#define RECORD "{\"type\":\"object\",\"properties\":{" ID "},\"required\":[\"id\"]}"
#define VERSION "\"if_revision\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":4294967295}"
#define DETAILS "\"name\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":32},\"number\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":24},\"note\":{\"type\":\"string\",\"maxLength\":64}"
static const ink_cap_t CAPS[] = {
    {.id = "contacts.open", .app = "phone", .locked_owner_only = true, .title = "打开联系人", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "contacts.pick", .app = "phone", .title = "挑选联系人号码", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "contacts.search", .app = "phone", .title = "搜索联系人", .activity = "PhoneContacts", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "contacts.view", .app = "phone", .title = "查看联系人详情", .input = RECORD, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "contacts.note", .app = "phone", .title = "输入联系人备注", .activity = "ContactEdit", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "contacts.put", .app = "phone", .title = "添加或修改联系人（每人一个号码）", .input = "{\"type\":\"object\",\"properties\":{" ID "," VERSION "," DETAILS "},\"required\":[\"name\",\"number\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = record},
    {.id = "contacts.delete", .app = "phone", .title = "删除联系人", .ask = TR_NOOP("删除联系人 {name}（{number}），不能撤销"), .input = "{\"type\":\"object\",\"properties\":{" ID "," VERSION "," DETAILS "},\"required\":[\"id\",\"if_revision\",\"name\",\"number\"]}", .effect = INK_FX_LOCAL_DESTRUCTIVE, .ask_by_hand = true, .child_ok = true, .exec = INK_EXEC_BG, .run = record},
    {.id = "contacts.select", .app = "phone", .title = "选联系人和号码，返回编号和号码", .input = RECORD, .output = "{\"type\":\"object\",\"properties\":{" ID ",\"number\":{\"type\":\"string\"}},\"required\":[\"id\",\"number\"]}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .run = record},
    {.id = "contacts.edit", .app = "phone", .title = "打开联系人或添加联系人", .input = "{\"type\":\"object\",\"properties\":{" ID "}}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "contacts.next", .app = "phone", .title = "联系人下一页", .activity = "PhoneContacts", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "contacts.prev", .app = "phone", .title = "联系人上一页", .activity = "PhoneContacts", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "contacts.name", .app = "phone", .title = "输入联系人姓名", .activity = "ContactEdit", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
    {.id = "contacts.number", .app = "phone", .title = "输入联系人号码", .activity = "ContactEdit", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}
};
int ink_contacts_init(void) {
    return ink_db_register(&CONTACTS) || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_app_register(&APP) || ink_app_register(&CONTACT_APP) ? -1 : 0;
}
void ink_contacts_open(void) { ink_ui_drop_app("contacts"); ink_ui_push(&LIST, (void *)2); }
