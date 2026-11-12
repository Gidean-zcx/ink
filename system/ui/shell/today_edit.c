/* ui-scale-runtime: dimension width Canvas width minus theme margins, shared by all shell pages. */
#include "shell_internal.h"

/* F8 keeps presentation choices in the existing member collection: the cards and the six favourites (D-135). */
static const ink_db_def_t TODAY_DB = {.name = "home.today", .title = "主屏放什么", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_records = 1, .max_bytes = 256};
enum { FAVORITES = 6 };
static const char *const DEFAULT_APPS[FAVORITES] = {"phone", "message", "reader", "ai", "camera", "clock"};
static struct {
    bool saved, apps_saved;
    int count, total, app_count;
    char selected[4][48], keys[6][48], args[6][96], up[6][96], order[6][32], apps[FAVORITES][24], app_args[96], which[32];
    ink_card_t items[6];
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    lv_obj_t *menu;
    ink_ui_page_spec_t model;
    ink_ui_field_spec_t fields[3];
    ink_ui_action_spec_t actions[4];
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
            if (card->bookmark || !ink_app_card_visible(card) || !strcmp(a->id, "weather")) continue; /* weather: on the date line */
            if (index-- == 0) {
                snprintf(key, 48, "%s:%s", a->id, card->id);
                return a;
            }
        }
    }
    return NULL;
}
void today_load(void) {
    T.saved = T.apps_saved = false; T.count = T.app_count = 0;
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
            at = ink_json_find(j, 0, "apps");
            T.apps_saved = at >= 0;
            for (int k = at + 1; at >= 0 && k < j->ntok && T.app_count < FAVORITES && j->tok[k].start < j->tok[at].end; k++)
                if (ink_json_str(j, k, T.apps[T.app_count], sizeof T.apps[0]) > 0 && home_usable(T.apps[T.app_count])) T.app_count++;
        }
        osal_free(j);
    }
    ink_db_close(db);
    for (int i = 0; !T.apps_saved && i < FAVORITES; i++)
        if (home_usable(DEFAULT_APPS[i])) snprintf(T.apps[T.app_count++], sizeof T.apps[0], "%s", DEFAULT_APPS[i]);
    if (!T.saved) { /* the next thing first, then what can be continued (D-135) */
        static const char *const FIRST[] = {"home", "listen", "news", "word"};
        ink_card_t card; char key[48];
        for (unsigned f = 0; f < sizeof FIRST / sizeof *FIRST; f++) for (int i = 0; ; i++) {
            const ink_app_def_t *a = today_item(i, &card, key);
            if (!a) break;
            if (strcmp(a->id, FIRST[f])) continue;
            memcpy(T.selected[T.count++], key, sizeof key); break;
        }
    }
}
int home_apps(const char *ids[6]) {
    for (int i = 0; i < T.app_count; i++) ids[i] = T.apps[i];
    return T.app_count;
}
int today_slots(void) { return T.count; }
bool today_match(const ink_json_t *j, int slot) {
    if (!slot) return ink_json_eq(j, ink_json_find(j, 0, "app"), "reader");
    char app[32] = "", id[16] = "", key[48];
    ink_json_str(j, ink_json_find(j, 0, "app"), app, sizeof app);
    ink_json_str(j, ink_json_find(j, 0, "id"), id, sizeof id);
    snprintf(key, sizeof key, "%s:%s", app, id);
    return slot <= T.count && !strcmp(key, T.selected[slot - 1]);
}
const char *today_icon(int slot) {
    if (N.cards[slot + 1].icon[0]) return N.cards[slot + 1].icon;
    if (!strncmp(T.selected[slot], "home:", strlen("home:"))) return INK_UI_ICON(calendar); /* the next thing */
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *a = ink_app_at(i);
        size_t n = strlen(a->id);
        if (!strncmp(T.selected[slot], a->id, n) && T.selected[slot][n] == ':') return a->icon ? a->icon : "grid";
    }
    return "grid";
}
void today_model(void) {
    for (int i = 1; i <= T.count; i++) {
        snprintf(N.card_objects[i], sizeof N.card_objects[i], "%s", T.selected[i - 1]);
        N.card_fields[i].id = N.card_actions[i].id = T.selected[i - 1];
    }
    N.today.field_count = N.today.action_count = (size_t)T.count + 1;
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
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR("主屏卡片"));
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP;
    ink_ui_list_row(screen, "book", TR("继续读"), TR("有在读的书时在最前"), INK_MARGIN, y, width(), false, NULL, NULL);
    y += INK_CONTROL + INK_GAP;
    const char *note = TR("主屏放两张，按下面的顺序，没有内容的不放。");
    ink_ui_notice(screen, note, y);
    y += ink_ui_text_height(note, INK_FONT_NOTE, INK_WEIGHT_REGULAR, width()) + INK_GAP;
    static const ink_ui_action_spec_t actions[] = {
        {.id = "prev", .label = TR_NOOP("上一页"), .capability = "shell.today_prev"},
        {.id = "next", .label = TR_NOOP("下一页"), .capability = "shell.today_next"},
        {.id = "back", .label = TR_NOOP("返回"), .capability = "shell.back", .enabled = true}};
    static const ink_ui_page_spec_t page = {.id = "TodayEdit", .title = TR_NOOP("主屏卡片"), .object = {"home.today", 0}, .actions = actions, .action_count = 3};
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
static int favorite(const char *id) {
    for (int i = 0; i < T.app_count; i++) if (!strcmp(T.apps[i], id)) return i;
    return -1;
}
/* Long press (D-135): on the home or off it, carry on with its card, its card on the home. */
static void icon_show(const char *which, const char *title, const ink_card_t *card, const char *key) {
    icon_close();
    T.items[0] = *card; card = &T.items[0];
    snprintf(T.which, sizeof T.which, "%s", which);
    const char *id = T.which; /* the menu's model outlives the call */
    bool resume = card->action[0] != 0, has_card = key[0] != 0, home = favorite(id) >= 0, picked = today_selected(key) >= 0;
    int y = INK_MARGIN + INK_CONTROL + INK_GAP_L, rows = INK_CONTROL + (resume ? INK_CONTROL : 0) + (has_card ? INK_CONTROL : 0);
    T.menu = ink_ui_box(lv_layer_sys(), 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
    lv_obj_set_style_bg_opa(T.menu, LV_OPA_TRANSP, 0); lv_obj_add_flag(T.menu, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *panel = ink_ui_panel(T.menu, y + rows + INK_GAP_L + INK_CONTROL + INK_MARGIN, true);
    ink_ui_circle(panel, home_icon(which), INK_MARGIN, INK_MARGIN, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL);
    ink_ui_line(panel, title, INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN + INK_CIRCLE + INK_GAP, INK_MARGIN, width() - INK_CIRCLE - INK_GAP);
    ink_ui_line(panel, card->text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + INK_CIRCLE + INK_GAP,
        INK_MARGIN + ink_ui_line_height(INK_FONT_SUBTITLE), width() - INK_CIRCLE - INK_GAP);
    snprintf(T.args[0], sizeof T.args[0], "{\"card\":\"%s\"}", key);
    snprintf(T.app_args, sizeof T.app_args, "{\"id\":\"%s\"}", id);
    const char *go = !strcmp(id, "listen") ? TR("接着听") : TR("接着做");
    T.fields[0] = (ink_ui_field_spec_t){"app", "card", title, card->text, {id, card->rev}, false};
    T.fields[1] = (ink_ui_field_spec_t){"home", "toggle", TR("放在主屏"), home ? TR("开") : TR("关"), {id, 0}, false};
    T.fields[2] = (ink_ui_field_spec_t){"today", "toggle", TR("卡片放在主屏"), picked ? TR("开") : TR("关"), {key, 0}, false};
    T.actions[0] = (ink_ui_action_spec_t){"resume", go, card->action, NULL, {id, card->rev}, resume};
    T.actions[1] = (ink_ui_action_spec_t){"home", TR("放在主屏"), "shell.favorite", T.app_args, {id, 0}, true};
    T.actions[2] = (ink_ui_action_spec_t){"today", TR("卡片放在主屏"), "shell.today_card", T.args[0], {key, 0}, has_card};
    T.actions[3] = (ink_ui_action_spec_t){"close", TR("关闭"), "shell.back", NULL, {NULL, 0}, true};
    T.model = (ink_ui_page_spec_t){"IconMenu", title, {id, 0}, T.fields, has_card ? 3 : 2, T.actions, 4};
    if (resume) {
        ink_shell_bind(ink_ui_list_row(panel, "play", go, NULL, INK_MARGIN, y, width(), false, NULL, NULL), card->action, NULL);
        y += INK_CONTROL;
    }
    ink_shell_bind(ink_ui_toggle(panel, TR("放在主屏"), home, INK_MARGIN, y, width(), NULL, NULL), "shell.favorite", T.app_args);
    y += INK_CONTROL;
    if (has_card) {
        ink_shell_bind(ink_ui_toggle(panel, TR("卡片放在主屏"), picked, INK_MARGIN, y, width(), NULL, NULL), "shell.today_card", T.args[0]);
        y += INK_CONTROL;
    }
    ink_shell_bind(ink_ui_pill(panel, TR("关闭"), INK_MARGIN, y + INK_GAP_L, width(), INK_CONTROL, false, NULL, NULL), "shell.back", NULL);
    ink_ui_overlay_add(&ICON_MENU, T.menu); ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
}
static void list_json(ink_jw_t *w, const char *name, const char *base, int n, size_t size, bool comma) {
    ink_jw_raw(w, comma ? ",\"" : "\""); ink_jw_raw(w, name); ink_jw_raw(w, "\":[");
    for (int i = 0; i < n; i++) {
        if (i) ink_jw_raw(w, ",");
        ink_jw_str(w, base + (size_t)i * size);
    }
    ink_jw_raw(w, "]");
}
/* What this member chose; what was never chosen stays the default. */
static void today_save(ink_call_t *c) {
    char json[256]; ink_jw_t w; ink_jw_init(&w, json, sizeof json); ink_jw_raw(&w, "{");
    if (T.saved) list_json(&w, "cards", T.selected[0], T.count, sizeof T.selected[0], false);
    if (T.apps_saved) list_json(&w, "apps", T.apps[0], T.app_count, sizeof T.apps[0], T.saved);
    ink_jw_raw(&w, "}");
    ink_db_t *db = w.overflow ? NULL : ink_db_open_member(&TODAY_DB, ink_member_current()->id);
    ink_db_write_t write = {.id = 1, .data = json, .len = strlen(json), .src = c->src};
    if (!db || ink_db_put(db, &write, NULL) != INK_DB_OK) c->rc.status = INK_RC_FAILED;
    ink_db_close(db); today_load();
    ink_ui_content_changed();
    if (T.list) ink_ui_list_refresh(T.list, (size_t)T.total);
    if (T.menu) icon_close();
    if (N.content) draw_home_content();
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
    ink_args_get_str(&c->args, "id", id, sizeof id);
    if (!strcmp(c->cap->id, "shell.icon_menu")) { /* the home's favourites and all apps */
        bool home = !strcmp(ink_ui_page_name(), "home");
        if ((!home && strcmp(ink_ui_page_name(), "Apps")) || !home_usable(id)) { c->rc.status = INK_RC_REJECTED; return; }
        const ink_app_def_t *a = NULL;
        for (int i = 0; i < ink_app_count(); i++) if (!strcmp(ink_app_at(i)->id, id)) a = ink_app_at(i);
        memset(&card, 0, sizeof card); T.keys[0][0] = 0;
        for (int j = 0; a; j++) {
            const ink_app_def_t *owner = today_item(j, &card, T.keys[0]);
            if (owner == a) break;
            if (!owner) { memset(&card, 0, sizeof card); T.keys[0][0] = 0; break; }
        }
        icon_show(id, home || !a ? home_label(id) : TR(a->title), &card, T.keys[0]);
        return;
    }
    if (!strcmp(c->cap->id, "shell.favorite")) {
        int at = favorite(id);
        if (at < 0 && (!home_usable(id) || T.app_count == FAVORITES)) {
            c->rc.status = INK_RC_REJECTED;
            snprintf(c->rc.say, sizeof c->rc.say, "%s", home_usable(id) ? TR("主屏最多放 6 个，请先去掉一个") : TR("这个应用不能放到主屏"));
            return;
        }
        if (at >= 0) memmove(T.apps[at], T.apps[at + 1], (size_t)(--T.app_count - at) * sizeof T.apps[0]);
        else snprintf(T.apps[T.app_count++], sizeof T.apps[0], "%s", id);
        T.apps_saved = true;
        snprintf(c->rc.say, sizeof c->rc.say, "%s", at >= 0 ? TR("已从主屏拿掉") : TR("已放到主屏"));
        today_save(c);
        return;
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
        c->rc.status = INK_RC_REJECTED;
        snprintf(c->rc.say, sizeof c->rc.say, "%s", found ? TR("最多放四张，请先去掉一张再选") : TR("没有这张卡片"));
        return;
    }
    if (up) {
        char swap[48]; memcpy(swap, T.selected[at - 1], sizeof swap);
        memcpy(T.selected[at - 1], key, sizeof key); memcpy(T.selected[at], swap, sizeof swap);
    } else if (at >= 0) memmove(T.selected[at], T.selected[at + 1], (size_t)(--T.count - at) * sizeof T.selected[0]);
    else memcpy(T.selected[T.count++], key, sizeof key);
    T.saved = true;
    today_save(c);
}
void icon_event(lv_event_t *e) {
    if (!current_session(N.epoch)) return;
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) { N.icon_version = ink_ui_content_version(); N.icon_generation = ink_ui_generation(); return; }
    if (N.icon_version && (N.icon_version != ink_ui_content_version() || N.icon_generation != ink_ui_generation())) return;
    const char *id = lv_event_get_user_data(e);
    bool menu = lv_event_get_code(e) != LV_EVENT_SHORT_CLICKED, ask = !menu && !strcmp(id, "ai") && !strcmp(ink_ui_page_name(), "home");
    char args[96]; snprintf(args, sizeof args, "{\"id\":\"%s\"}", id);
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .page_gen = ink_ui_content_version(), .args_json = ask ? NULL : args, .reply_exec = INK_EXEC_UI};
    ink_cap_call(menu ? "shell.icon_menu" : ask ? "ai.panel.listen" : "shell.launch", NULL, &opts); /* 问 AI: the panel, as the AI key */
}
void icon_bind(lv_obj_t *obj, const char *id) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj, icon_event, LV_EVENT_PRESSED, (void *)id);
    lv_obj_add_event_cb(obj, icon_event, LV_EVENT_SHORT_CLICKED, (void *)id);
    lv_obj_add_event_cb(obj, icon_event, LV_EVENT_LONG_PRESSED, (void *)id);
}
int today_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "shell.today_edit", .title = "选主屏放哪些卡片", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = today_call},
        {.id = "shell.icon_menu", .title = "应用快捷动作", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = today_call},
        {.id = "shell.favorite", .title = "把应用放到主屏或从主屏拿掉（最多 6 个）", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = today_call},
        {.id = "shell.today_card", .title = "选主屏卡片或上移", .input = "{\"type\":\"object\",\"properties\":{\"card\":{\"type\":\"string\"},\"up\":{\"type\":\"boolean\"}},\"required\":[\"card\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = today_call},
        {.id = "shell.today_prev", .title = "主屏卡片上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = today_call},
        {.id = "shell.today_next", .title = "主屏卡片下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = today_call}};
    return ink_db_register(&TODAY_DB) || ink_cap_register(caps, sizeof caps / sizeof *caps);
}
