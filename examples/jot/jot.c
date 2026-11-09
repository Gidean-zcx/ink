/* A bounded, test-only app using the existing framework. */
#include "jot.h"
#include "ink/sys/app.h"
#include "ink/sys/appreg.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/textfield.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include <stdio.h>
#include <string.h>

static ink_db_t *ink_jot_db;
static ink_ui_list_t *ink_jot_list;
static ink_ui_list_position_t ink_jot_position;
static lv_obj_t *ink_jot_field;
static char ink_jot_ids[3][32], ink_jot_texts[3][96];
static const ink_activity_t INK_JOT_LIST, INK_JOT_EDIT;

/* snippet: collection */
static const ink_db_def_t INK_JOT_DATA = {
    .name = "jot.entries", .title = "记一笔", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .ai_write = false,
    .max_records = 8, .max_bytes = 8192
};
int ink_jot_prepare(void) {
    if (!ink_jot_db) ink_jot_db = ink_app_db_open("jot", &INK_JOT_DATA);
    return ink_jot_db ? 0 : -1;
}
/* end snippet */

static const ink_ui_action_spec_t INK_JOT_LIST_ACTIONS[] = {
    {.id = "prev", .label = "上一页", .capability = "jot.prev"},
    {.id = "next", .label = "下一页", .capability = "jot.next"},
    {.id = "new", .label = "记一笔", .capability = "jot.new", .enabled = true}
};
static const ink_ui_page_spec_t INK_JOT_LIST_MODEL = {
    .id = "jot.list", .title = "记一笔", .object = {"jot:entries", 1},
    .actions = INK_JOT_LIST_ACTIONS, .action_count = 3
};
/* snippet: semantics */
static ink_ui_field_spec_t ink_jot_spec = {
    .id = "body", .role = "textbox", .label = "内容", .object = {"jot:input", 1}
};
static const ink_ui_action_spec_t INK_JOT_EDIT_ACTIONS[] = {
    {.id = "focus", .label = "输入", .capability = "jot.focus", .enabled = true},
    {.id = "save", .label = "保存", .capability = "jot.save", .enabled = true}
};
static const ink_ui_page_spec_t INK_JOT_EDIT_MODEL = {
    .id = "jot.edit", .title = "写一笔", .object = {"jot:input", 1},
    .fields = &ink_jot_spec, .field_count = 1,
    .actions = INK_JOT_EDIT_ACTIONS, .action_count = 2
};
/* end snippet */

static void ink_jot_click(lv_event_t *e) {
    const char *id = lv_event_get_user_data(e);
    bool editing = !strcmp(ink_ui_page_name(), INK_JOT_EDIT_MODEL.id);
    const ink_ui_page_spec_t *model = editing ? &INK_JOT_EDIT_MODEL : &INK_JOT_LIST_MODEL;
    ink_ui_action_target_t target = {ink_ui_content_version(), model->object};
    ink_receipt_t rc;
    if (editing) ink_ui_semantics_action(model, id, &target, INK_SRC_TOUCH, &rc);
    else ink_ui_list_action(ink_jot_list, id, &target, INK_SRC_TOUCH, &rc);
}

/* snippet: save */
static void ink_jot_save(ink_call_t *c) {
    const char *text = ink_textfield_text(ink_jot_field);
    if (!ink_jot_db || !text || !*text) { c->rc.status = INK_RC_INVALID; return; }
    char payload[600];
    ink_jw_t json;
    ink_jw_init(&json, payload, sizeof payload);
    ink_jw_raw(&json, "{\"text\":"); ink_jw_str(&json, text); ink_jw_raw(&json, "}");
    ink_db_write_t write = {.data = payload, .len = json.len, .src = c->src};
    uint32_t id = 0;
    if (json.overflow || ink_db_put(ink_jot_db, &write, &id) != INK_DB_OK) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.say, sizeof c->rc.say, "这笔没能记下");
        return;
    }
    snprintf(c->rc.object, sizeof c->rc.object, "jot:%u", (unsigned)id);
    c->rc.revision = ink_db_last_ver(ink_jot_db);
    snprintf(c->rc.say, sizeof c->rc.say, "已接收，正在保存");
    ink_app_card_changed("jot");
    ink_ui_pop();
}
/* end snippet */

static void ink_jot_open(ink_call_t *c) {
    if (!ink_jot_db || ink_app_open("jot", NULL, NULL) != 0) c->rc.status = INK_RC_FAILED;
}
static void ink_jot_new(ink_call_t *c) { (void)c; ink_ui_push(&INK_JOT_EDIT, NULL); }
static void ink_jot_focus(ink_call_t *c) {
    if (ink_textfield_short_focus(ink_jot_field) != 0) c->rc.status = INK_RC_STALE;
}
static void ink_jot_turn(ink_call_t *c) {
    ink_ui_list_turn(ink_jot_list, !strcmp(c->cap->id, "jot.next") ? 1 : -1);
    if (ink_app_save_state("jot") != 0) c->rc.status = INK_RC_FAILED;
}
/* snippet: capabilities */
static const ink_cap_t INK_JOT_CAPS[] = {
    {.id = "jot.open", .title = "打开记一笔", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = ink_jot_open},
    {.id = "jot.new", .title = "写一笔", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "jot.list", .run = ink_jot_new},
    {.id = "jot.focus", .title = "输入内容", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "jot.edit", .run = ink_jot_focus},
    {.id = "jot.save", .title = "保存这一笔", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .activity = "jot.edit", .run = ink_jot_save},
    {.id = "jot.prev", .title = "上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "jot.list", .run = ink_jot_turn},
    {.id = "jot.next", .title = "下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .activity = "jot.list", .run = ink_jot_turn}
};
/* end snippet */

static void ink_jot_bind(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field,
                          ink_ui_action_spec_t *action, void *ctx) {
    (void)action; (void)ctx;
    ink_db_cursor_t cursor = {0};
    ink_rec_t rec = {0};
    char payload[600];
    unsigned at = (unsigned)(index % 3);
    for (size_t i = 0; i <= index; i++) {
        if (ink_db_next(ink_jot_db, &cursor, &rec, payload, sizeof payload) != 1) return;
    }
    ink_json_t *json = ink_app_alloc(sizeof *json, true);
    ink_jot_texts[at][0] = 0;
    if (json && ink_json_parse(json, payload, rec.len) == 0)
        ink_json_str(json, ink_json_find(json, 0, "text"), ink_jot_texts[at], sizeof ink_jot_texts[at]);
    ink_app_free(json);
    snprintf(ink_jot_ids[at], sizeof ink_jot_ids[at], "jot:%u", (unsigned)rec.id);
    *field = (ink_ui_field_spec_t){.id = ink_jot_ids[at], .role = "listitem", .label = "记录",
        .value = ink_jot_texts[at], .object = {ink_jot_ids[at], rec.ver}};
    if (!lv_obj_get_child_count(slot))
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_GAP, INK_GAP, INK_CANVAS_W - 2 * INK_MARGIN - 2 * INK_GAP);
    lv_label_set_text(lv_obj_get_child(slot, 0), field->value);
}
/* snippet: list */
static void ink_jot_list_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_topbar(screen, INK_JOT_LIST_MODEL.title);
    lv_obj_t *body = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP,
        INK_CANVAS_W, INK_CANVAS_H - INK_STATUS_BAR - 2 * INK_PRIMARY - 2 * INK_GAP - INK_MARGIN, 0, INK_SURFACE_WHITE, false);
    ink_jot_list = ink_ui_list_create(body, &INK_JOT_LIST_MODEL, &ink_jot_position,
        3, 1, INK_CONTROL, "还没有记录", ink_jot_bind, NULL);
    if (!ink_jot_list) ink_app_fail("jot list allocation failed");
    ink_ui_list_refresh(ink_jot_list, (size_t)ink_db_count(ink_jot_db));
    ink_ui_pill(screen, "记一笔", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY,
        INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, ink_jot_click, "new");
}
/* end snippet */
static int ink_jot_list_snapshot(char *out, size_t cap) { return ink_ui_list_snapshot(ink_jot_list, out, cap); }
static void ink_jot_list_destroy(void) { ink_jot_list = NULL; }
static bool ink_jot_key(ink_key_t key, bool long_press) { return ink_ui_list_key(ink_jot_list, key, long_press); }

/* snippet: input */
static void ink_jot_edit_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_topbar(screen, INK_JOT_EDIT_MODEL.title);
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .initial = "", .hint = "写下这件事", .max = 95};
    ink_jot_field = ink_textfield_create(screen, &cfg, NULL, NULL, false);
    if (!ink_jot_field) ink_app_fail("jot field allocation failed");
    lv_obj_set_pos(ink_jot_field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_STATUS_BAR + INK_PRIMARY + INK_GAP));
    lv_obj_set_width(ink_jot_field, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN));
    if (ink_textfield_bind_short(ink_jot_field, &INK_JOT_EDIT_MODEL, &ink_jot_spec, &INK_JOT_EDIT_ACTIONS[0]) != 0)
        ink_app_fail("jot input binding failed");
    ink_ui_pill(screen, "保存", INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY,
        INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, ink_jot_click, "save");
}
/* end snippet */
static int ink_jot_edit_snapshot(char *out, size_t cap) { return ink_textfield_short_snapshot(ink_jot_field, out, cap); }
static void ink_jot_edit_destroy(void) { ink_jot_field = NULL; ink_jot_spec.value = ""; }
/* snippet: activities */
static const ink_activity_t INK_JOT_LIST = {
    .name = "jot.list", .app = "jot", .create = ink_jot_list_create, .destroy = ink_jot_list_destroy,
    .snapshot = ink_jot_list_snapshot, .on_key = ink_jot_key
};
static const ink_activity_t INK_JOT_EDIT = {
    .name = "jot.edit", .app = "jot", .create = ink_jot_edit_create, .destroy = ink_jot_edit_destroy,
    .snapshot = ink_jot_edit_snapshot
};
/* end snippet */

/* snippet: recovery */
static int ink_jot_save_state(void *buf, size_t cap) {
    if (!cap) return 0;
    *(uint8_t *)buf = (uint8_t)ink_jot_position.page;
    return 1;
}
static void ink_jot_restore_state(const void *buf, size_t len) {
    if (len == 1) ink_jot_position.page = *(const uint8_t *)buf;
}
static void ink_jot_stop(void) {
    ink_db_close(ink_jot_db); ink_jot_db = NULL;
    ink_jot_position = (ink_ui_list_position_t){0};
}
static const ink_app_ops_t INK_JOT_OPS = {
    .on_stop = ink_jot_stop, .save_state = ink_jot_save_state, .restore_state = ink_jot_restore_state
};
/* end snippet */
/* snippet: card */
static int ink_jot_card(int index, ink_card_t *out) {
    if (index) return 0;
    *out = (ink_card_t){0};
    snprintf(out->id, sizeof out->id, "new");
    snprintf(out->title, sizeof out->title, "记一笔");
    snprintf(out->text, sizeof out->text, "记下新的事情");
    snprintf(out->action, sizeof out->action, "jot.open");
    snprintf(out->member, sizeof out->member, "%s", ink_member_current()->id);
    out->rev = 1;
    return 1;
}
/* end snippet */
/* snippet: registration */
static const ink_app_def_t INK_JOT_APP = {
    .id = "jot", .title = "记一笔", .icon = "memo", .entry = &INK_JOT_LIST,
    .child_ok = true, .ops = &INK_JOT_OPS, .card = ink_jot_card
};
int ink_jot_init(void) {
    if (ink_db_register(&INK_JOT_DATA) != 0 ||
        ink_cap_register(INK_JOT_CAPS, sizeof INK_JOT_CAPS / sizeof *INK_JOT_CAPS) != 0) return -1;
    return ink_app_register(&INK_JOT_APP);
}
/* end snippet */
