/* ui-scale-runtime: dimension width Canvas width minus theme margins, shared by all shell pages. */
#include "shell_internal.h"

/* F8 keeps presentation choices in the existing member collection. */
static const ink_db_def_t TODAY_DB = {.name = "home.today", .title = "今日屏放什么", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_records = 1, .max_bytes = 256};
static struct {
    bool saved;
    int count, total;
    char selected[4][48], keys[6][48], args[6][96], up[6][96], order[6][32];
    ink_card_t items[6];
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    lv_obj_t *menu;
    ink_ui_page_spec_t model;
    ink_ui_field_spec_t fields[2];
    ink_ui_action_spec_t actions[3];
} T INK_PSRAM_BSS;
static int today_selected(const char *key) {
    for (int i = 0; i < T.count; i++) if (!strcmp(T.selected[i], key)) return i;
    return -1;
}
static const ink_app_def_t *today_item(int index, ink_card_t *card, char key[48]) {
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *a = ink_app_at(i);
        if (!a->card || !ink_app_visible(a)) continue;
        for (int j = 0; j < 8; j++) {
            memset(card, 0, sizeof *card);
            if (a->card(j, card) != 1) break;
            if (card->bookmark || !ink_app_card_visible(card)) continue;
            if (index-- == 0) {
                snprintf(key, 48, "%s:%s", a->id, card->id);
                return a;
            }
        }
    }
    return NULL;
}
void today_load(void) {
    T.saved = false; T.count = 0;
    ink_db_t *db = ink_db_open_member(&TODAY_DB, ink_member_current()->id);
    char json[256] = "";
    ink_rec_t rec;
    if (db && ink_db_get(db, 1, &rec, json, sizeof json - 1) == INK_DB_OK) {
        ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
        if (j && ink_json_parse(j, json, strlen(json)) == 0) {
            int at = ink_json_find(j, 0, "cards");
            T.saved = at >= 0;
            for (int k = at + 1; at >= 0 && k < j->ntok && T.count < 4 && j->tok[k].start < j->tok[at].end; k++)
                if (ink_json_str(j, k, T.selected[T.count], sizeof T.selected[0]) >= 0) T.count++;
        }
        osal_free(j);
    }
    ink_db_close(db);
    if (!T.saved) {
        ink_card_t card; char key[48];
        for (int slot = 1; slot < 5; slot++) for (int i = 0; ; i++) {
            const ink_app_def_t *a = today_item(i, &card, key);
            if (!a) break;
            if (strcmp(a->id, CARD_APPS[slot]) && !(slot == 4 && !strcmp(key, "home:upcoming"))) continue;
            memcpy(T.selected[T.count++], key, sizeof key); break;
        }
    }
}
int today_slots(void) { return T.saved ? T.count : 4; }
bool today_match(const ink_json_t *j, int slot) {
    if (!T.saved || !slot) return ink_json_eq(j, ink_json_find(j, 0, "app"), CARD_APPS[slot]) ||
        (slot == 4 && ink_json_eq(j, ink_json_find(j, 0, "app"), "home") && ink_json_eq(j, ink_json_find(j, 0, "id"), "upcoming"));
    char app[32] = "", id[16] = "", key[48];
    ink_json_str(j, ink_json_find(j, 0, "app"), app, sizeof app);
    ink_json_str(j, ink_json_find(j, 0, "id"), id, sizeof id);
    snprintf(key, sizeof key, "%s:%s", app, id);
    return slot <= T.count && !strcmp(key, T.selected[slot - 1]);
}
const char *today_icon(int slot) {
    if (child() && !T.saved && slot == 3) return INK_UI_ICON(clock);
    if (!T.saved) return APP_ICONS[slot];
    if (N.cards[slot + 1].icon[0]) return N.cards[slot + 1].icon;
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *a = ink_app_at(i);
        size_t n = strlen(a->id);
        if (!strncmp(T.selected[slot], a->id, n) && T.selected[slot][n] == ':') return a->icon ? a->icon : "grid";
    }
    return "grid";
}
void today_model(void) {
    for (int i = 0; i <= today_slots(); i++) {
        if (T.saved && i) {
            snprintf(N.card_objects[i], sizeof N.card_objects[i], "%s", T.selected[i - 1]);
            N.card_fields[i].id = N.card_actions[i].id = T.selected[i - 1];
            if (!N.cards[i].id[0]) N.card_fields[i].label = N.card_fields[i].value = TR("卡片暂不可用");
        }
    }
    N.today.field_count = N.today.action_count = today_slots() + 1;
}
void kid_today_model(void) {
    if (T.saved) return;
    N.card_fields[4] = (ink_ui_field_spec_t){"time", "text", TR("今天"), N.kid_time, {"member.time", 0}, false};
    N.card_actions[4].enabled = false;
}
static void today_bind(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    unsigned i = index % 6;
    const ink_app_def_t *app = today_item((int)index, &T.items[i], T.keys[i]);
    if (!app) return;
    char key[48]; memcpy(key, T.keys[i], sizeof key);
    int picked = today_selected(key);
    snprintf(T.args[i], sizeof T.args[i], "{\"card\":\"%s\"}", key);
    snprintf(T.up[i], sizeof T.up[i], "{\"card\":\"%s\",\"up\":true}", key);
    snprintf(T.order[i], sizeof T.order[i], picked < 0 ? TR("未选") : TR("第 %d 张"), picked + 1);
    *field = (ink_ui_field_spec_t){T.keys[i], "card", T.items[i].title, T.order[i], {T.keys[i], T.items[i].rev}, false};
    *action = (ink_ui_action_spec_t){T.keys[i], TR("选择卡片"), "shell.today_card", T.args[i], field->object, true};
    lv_obj_clean(slot);
    ink_ui_circle(slot, app->icon ? app->icon : "grid", INK_GAP, 0, INK_CIRCLE_SMALL, INK_ICON_MEDIUM, INK_SURFACE_STATIC, NULL, NULL);
    ink_ui_line(slot, T.items[i].title, INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_CIRCLE + INK_GAP, 0,
        width() - INK_CIRCLE - 2 * INK_PRIMARY - 2 * INK_GAP);
    ink_ui_line(slot, T.order[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CIRCLE + INK_GAP, INK_LINE_LIST,
        width() - INK_CIRCLE - 2 * INK_PRIMARY - 2 * INK_GAP);
    lv_obj_t *toggle = ink_ui_toggle(slot, "", picked >= 0, width() - INK_PRIMARY - INK_GAP, 0, INK_PRIMARY, NULL, NULL);
    ink_shell_bind(toggle, "shell.today_card", T.args[i]);
    if (picked > 0) ink_shell_bind(ink_ui_pill(slot, TR("上移"), width() - 2 * INK_PRIMARY - 2 * INK_GAP, 0,
        INK_PRIMARY, INK_CONTROL, false, NULL, NULL), "shell.today_card", T.up[i]);
}
static int today_snapshot(char *buf, size_t cap) { return T.list ? ink_ui_list_snapshot(T.list, buf, cap) : -1; }
static int icon_snapshot(char *buf, size_t cap) { return ink_ui_semantics_snapshot(&T.model, buf, cap); }
static void today_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    today_load();
    T.position = (ink_ui_list_position_t){0};
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR("今日屏放什么"));
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP;
    ink_ui_list_row(screen, "book", TR("继续读"), TR("总在最上面"), INK_MARGIN, y, width(), false, NULL, NULL);
    y += INK_CONTROL + INK_GAP;
    ink_ui_notice(screen, TR("最上面总是你正在读的书。下面放四张。"), y);
    y += ink_ui_line_height(INK_FONT_NOTE) + INK_GAP;
    static const ink_ui_action_spec_t actions[] = {
        {.id = "prev", .label = TR_NOOP("上一页"), .capability = "shell.today_prev"},
        {.id = "next", .label = TR_NOOP("下一页"), .capability = "shell.today_next"},
        {.id = "back", .label = TR_NOOP("返回"), .capability = "shell.back", .enabled = true}};
    static const ink_ui_page_spec_t page = {.id = "TodayEdit", .title = TR_NOOP("今日屏放什么"), .object = {"home.today", 0}, .actions = actions, .action_count = 3};
    lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false);
    T.list = ink_ui_list_create(body, &page, &T.position, 6, 1, INK_CONTROL, TR("应用还没有提供卡片"), today_bind, NULL);
    ink_card_t card; char key[48]; T.total = 0;
    while (today_item(T.total, &card, key)) T.total++;
    if (T.list) ink_ui_list_refresh(T.list, (size_t)T.total);
}
static void today_destroy(void) { T.list = NULL; }
static bool today_key(ink_key_t key, bool lng) { return T.list && ink_ui_list_key(T.list, key, lng); }
static const ink_activity_t TODAY_EDIT = {.name = "TodayEdit", .create = today_create, .destroy = today_destroy, .snapshot = today_snapshot, .on_key = today_key};
static bool icon_key(ink_key_t key, bool lng) {
    if (!lng && (key == INK_KEY_HOME || key == INK_KEY_BACK)) icon_close();
    return true;
}
static const ink_ui_overlay_t ICON_MENU = {.name = "IconMenu", .level = 0, .on_key = icon_key, .snapshot = icon_snapshot};
bool icon_close(void) {
    if (!T.menu) return false;
    ink_ui_overlay_remove(&ICON_MENU); lv_obj_delete(T.menu); T.menu = NULL;
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI); return true;
}
static void icon_show(const ink_app_def_t *app, const ink_card_t *card, const char *key) {
    icon_close();
    const char *title = TR(app->title);
    T.items[0] = *card; card = &T.items[0];
    T.menu = ink_ui_box(lv_layer_sys(), 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
    lv_obj_set_style_bg_opa(T.menu, LV_OPA_TRANSP, 0); lv_obj_add_flag(T.menu, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *panel = ink_ui_panel(T.menu, 4 * INK_CONTROL + 2 * INK_MARGIN + 2 * INK_GAP_L, true);
    ink_ui_circle(panel, app->icon ? app->icon : "grid", INK_MARGIN, INK_MARGIN, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL);
    ink_ui_line(panel, title, INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN + INK_CIRCLE + INK_GAP, INK_MARGIN, width() - INK_CIRCLE - INK_GAP);
    ink_ui_line(panel, card->text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + INK_CIRCLE + INK_GAP,
        INK_MARGIN + ink_ui_line_height(INK_FONT_SUBTITLE), width() - INK_CIRCLE - INK_GAP);
    T.fields[0] = (ink_ui_field_spec_t){"app", "card", title, card->text, {app->id, card->rev}, false};
    T.fields[1] = (ink_ui_field_spec_t){"today", "toggle", TR("放到今日屏"), today_selected(key) >= 0 ? TR("开") : TR("关"), {key, 0}, false};
    char args[96]; snprintf(args, sizeof args, "{\"card\":\"%s\"}", key);
    memcpy(T.args[0], args, sizeof args);
    T.actions[0] = (ink_ui_action_spec_t){"resume", !strcmp(app->id, "listen") ? TR("接着听") : TR("接着做"), card->action, NULL, {app->id, card->rev}, card->action[0] != 0};
    T.actions[1] = (ink_ui_action_spec_t){"today", TR("放到今日屏"), "shell.today_card", T.args[0], {key, 0}, key[0] != 0};
    T.actions[2] = (ink_ui_action_spec_t){"close", TR("关闭"), "shell.back", NULL, {NULL, 0}, true};
    T.model = (ink_ui_page_spec_t){"IconMenu", title, {app->id, 0}, T.fields, 2, T.actions, 3};
    int y = INK_MARGIN + INK_CONTROL + INK_GAP_L;
    lv_obj_t *resume = ink_ui_list_row(panel, "play", !strcmp(app->id, "listen") ? TR("接着听") : TR("接着做"), NULL, INK_MARGIN, y, width(), false, NULL, NULL);
    if (card->action[0]) ink_shell_bind(resume, card->action, NULL);
    lv_obj_t *toggle = ink_ui_toggle(panel, TR("放到今日屏"), today_selected(key) >= 0, INK_MARGIN, y + INK_CONTROL, width(), NULL, NULL);
    if (key[0]) ink_shell_bind(toggle, "shell.today_card", T.args[0]);
    ink_shell_bind(ink_ui_pill(panel, TR("关闭"), INK_MARGIN, y + 2 * INK_CONTROL + INK_GAP_L, width(), INK_CONTROL, false, NULL, NULL), "shell.back", NULL);
    ink_ui_overlay_add(&ICON_MENU, T.menu); ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
}
static void today_call(ink_call_t *c) {
    if (!strcmp(c->cap->id, "shell.today_edit")) {
        ink_shell_close(); ink_ui_push(&TODAY_EDIT, NULL);
        return;
    }
    if (!strcmp(c->cap->id, "shell.today_next") || !strcmp(c->cap->id, "shell.today_prev")) {
        if (T.list) ink_ui_list_turn(T.list, !strcmp(c->cap->id, "shell.today_next") ? 1 : -1);
        return;
    }
    today_load();
    char key[48] = "", id[32] = ""; ink_card_t card;
    if (!strcmp(c->cap->id, "shell.icon_menu")) {
        ink_args_get_str(&c->args, "id", id, sizeof id);
        if (N.home_page != 2 || strcmp(ink_ui_page_name(), "home")) {
            c->rc.status = INK_RC_REJECTED;
            return;
        }
        for (int i = 0; i < ink_app_count(); i++) {
            const ink_app_def_t *a = ink_app_at(i);
            if (strcmp(a->id, id) || !ink_app_visible(a)) continue;
            memset(&card, 0, sizeof card); T.keys[0][0] = 0;
            bool has = false;
            for (int j = 0; ; j++) {
                const ink_app_def_t *owner = today_item(j, &card, T.keys[0]);
                if (!owner) break;
                if (owner == a) { has = true;
                    break;
                }
            }
            if (!has) { memset(&card, 0, sizeof card);
                T.keys[0][0] = 0;
            }
            icon_show(a, &card, T.keys[0]); return;
        }
        c->rc.status = INK_RC_REJECTED; return;
    }
    ink_args_get_str(&c->args, "card", key, sizeof key);
    bool found = false, up = false; ink_args_get_bool(&c->args, "up", &up);
    for (int i = 0; ; i++) {
        char candidate[48];
        if (!today_item(i, &card, candidate)) break;
        if (!strcmp(candidate, key)) { found = true;
            break;
        }
    }
    int at = today_selected(key);
    if (!found || (up && at <= 0) || (!up && at < 0 && T.count == 4)) {
        c->rc.status = INK_RC_REJECTED; snprintf(c->rc.say, sizeof c->rc.say, TR("最多放四张，请先去掉一张再选")); return;
    }
    if (up) {
        char swap[48]; memcpy(swap, T.selected[at - 1], sizeof swap);
        memcpy(T.selected[at - 1], key, sizeof key); memcpy(T.selected[at], swap, sizeof swap);
    } else if (at >= 0) memmove(T.selected[at], T.selected[at + 1], (size_t)(--T.count - at) * sizeof T.selected[0]);
    else memcpy(T.selected[T.count++], key, sizeof key);
    char json[256]; ink_jw_t w; ink_jw_init(&w, json, sizeof json); ink_jw_raw(&w, "{\"cards\":[");
    for (int i = 0; i < T.count; i++) {
        if (i) ink_jw_raw(&w, ",");
        ink_jw_str(&w, T.selected[i]);
    }
    ink_jw_raw(&w, "]}");
    ink_db_t *db = ink_db_open_member(&TODAY_DB, ink_member_current()->id);
    ink_db_write_t write = {.id = 1, .data = json, .len = strlen(json), .src = c->src};
    if (!db || ink_db_put(db, &write, NULL) != INK_DB_OK) c->rc.status = INK_RC_FAILED;
    ink_db_close(db); today_load();
    ink_ui_content_changed();
    if (T.list) ink_ui_list_refresh(T.list, (size_t)T.total);
    if (T.menu) icon_close();
    if (N.content && N.home_page == 0) draw_home_content();
}
void icon_event(lv_event_t *e) {
    if (!current_session(N.epoch)) return;
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) { N.icon_version = ink_ui_content_version(); N.icon_generation = ink_ui_generation(); return; }
    if (N.icon_version && (N.icon_version != ink_ui_content_version() || N.icon_generation != ink_ui_generation())) return;
    char args[96]; snprintf(args, sizeof args, "{\"id\":\"%s\"}", (const char *)lv_event_get_user_data(e));
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .page_gen = ink_ui_content_version(), .args_json = args, .reply_exec = INK_EXEC_UI};
    ink_cap_call(lv_event_get_code(e) == LV_EVENT_SHORT_CLICKED ? "shell.launch" : "shell.icon_menu", NULL, &opts);
}
void icon_bind(lv_obj_t *obj, const char *id) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj, icon_event, LV_EVENT_PRESSED, (void *)id);
    lv_obj_add_event_cb(obj, icon_event, LV_EVENT_SHORT_CLICKED, (void *)id);
    lv_obj_add_event_cb(obj, icon_event, LV_EVENT_LONG_PRESSED, (void *)id);
}
int today_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "shell.today_edit", .title = "今日屏放什么", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = today_call},
        {.id = "shell.icon_menu", .title = "应用快捷动作", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = today_call},
        {.id = "shell.today_card", .title = "选择今日卡片或上移", .input = "{\"type\":\"object\",\"properties\":{\"card\":{\"type\":\"string\"},\"up\":{\"type\":\"boolean\"}},\"required\":[\"card\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = today_call},
        {.id = "shell.today_prev", .title = "今日卡片上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = today_call},
        {.id = "shell.today_next", .title = "今日卡片下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = today_call}};
    return ink_db_register(&TODAY_DB) || ink_cap_register(caps, sizeof caps / sizeof *caps);
}
