/* ui-scale-runtime: dimension width Canvas width minus theme margins, shared by all shell pages. */
/* ui-scale-runtime: dimension S,tile_h One factor for the whole home page: theme sizes grow with the list line at the text size. */
/* ui-scale-runtime: dimension y,h,w,top,line,bottom,all,grid,room,rows,need,at,apps_h,row_y Page flow: theme sizes added up at the current text size. */
#include "shell_internal.h"

static bool kid_app_visible(const char *id) {
    if (!strcmp(id, "settings")) return false;
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *app = ink_app_at(i);
        if (strcmp(app->id, id)) continue;
        if (!app->entry || !ink_app_visible(app)) return false;
        for (const char *const *need = app->needs; need && *need; need++)
            if (!ink_cap_ai_callable(ink_cap_find(*need))) return false;
        return true;
    }
    return !strcmp(id, "ai") && ink_cap_ai_callable(ink_cap_find("shell.ai_home"));
}
static void kid_time_reply(void *ctx, const ink_receipt_t *rc) {
    if (!current_session((uint32_t)(uintptr_t)ctx) || !child() || rc->status != INK_RC_DONE || !rc->result) return;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int64_t minutes = 0, extra = 0, used = 0;
    bool ok = j && ink_json_parse(j, rc->result, strlen(rc->result)) == 0 &&
        ink_json_int(j, ink_json_find(j, 0, "minutes"), &minutes) &&
        ink_json_int(j, ink_json_find(j, 0, "extra_minutes"), &extra) &&
        ink_json_int(j, ink_json_find(j, 0, "used_seconds"), &used);
    osal_free(j);
    if (!ok) return;
    int64_t left = (minutes + extra) * 60 - used;
    if (!minutes) snprintf(N.kid_time, sizeof N.kid_time, TR("今天不限时"));
    else snprintf(N.kid_time, sizeof N.kid_time, TR("今天还能用 %lld 分钟"), (long long)(left > 0 ? (left + 59) / 60 : 0));
    if (N.kid_time_label && !N.result[0]) lv_label_set_text(N.kid_time_label, N.kid_time);
    ink_ui_content_changed();
}
void kid_time_read(void) {
    ink_call_opts_t opts = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_UI, .done = kid_time_reply,
        .ctx = (void *)(uintptr_t)N.epoch, .member = ink_member_current()->id};
    ink_cap_call("member.time_get", NULL, &opts);
}
bool bookmark(ink_card_t *out) {
    memset(out, 0, sizeof *out);
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *a = ink_app_at(i);
        if (!ink_app_visible(a)) continue;
        for (int j = 0; a->card && a->card(j, out) == 1; j++)
            if (out->bookmark && ink_app_card_visible(out)) return true;
    }
    return false;
}

/* One page (D-135): the date and weather, today's cards, six favourites, all apps. Gaps and cells grow with the
 * text by one factor, so a larger text size is the same page, larger. */
enum { HOME_CARDS = 2, HOME_APPS = 6, AI_HINT_TURNS = 3 };
static int S(int v) { return v * ink_ui_line_height(INK_FONT_LIST) / INK_LINE_LIST; }
static const ink_app_def_t *app_def(const char *id) {
    for (int i = 0; i < ink_app_count(); i++) if (!strcmp(ink_app_at(i)->id, id)) return ink_app_at(i);
    return NULL;
}
/* What this member can open from the home; settings is the shell's own page. */
bool home_usable(const char *id) {
    if (child()) return kid_app_visible(id);
    const ink_app_def_t *a = app_def(id);
    return !strcmp(id, "settings") || (a && a->entry && ink_app_visible(a));
}
static const char *def_icon(const ink_app_def_t *a) { /* no app: settings, the shell's own page */
    return !a ? INK_UI_ICON(gear) : a->icon ? a->icon : INK_UI_ICON(grid);
}
const char *home_icon(const char *which) { return def_icon(app_def(which)); }
/* A favourite's name on the home: the AI's is what it does there. */
const char *home_label(const char *id) {
    const ink_app_def_t *a = app_def(id);
    return !strcmp(id, "ai") ? TR("问 AI") : a ? TR(a->title) : TR("设置");
}

static char weather[48]; /* "12° 小雨": the weather app's card, read with the others */
static void cards_reply(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    const char *p = rc->result;
    N.cards_ok = j && rc->status == INK_RC_DONE && p && *p++ == '[';
    while (N.cards_ok && *p != ']') {
        const char *end = p;
        bool quoted = false;
        /* app.cards emits flat objects; delimiters inside strings are data. */
        for (; *end; end++) {
            if (quoted && *end == '\\' && end[1]) end++;
            else if (*end == '"') quoted = !quoted;
            else if (*end == '}' && !quoted) break;
        }
        N.cards_ok = *end && ink_json_parse(j, p, (size_t)(end - p + 1)) == 0;
        if (!N.cards_ok) break;
        char icon[16] = "";
        if (ink_json_eq(j, ink_json_find(j, 0, "app"), "weather") && ink_json_str(j, ink_json_find(j, 0, "icon"), icon, sizeof icon) > 0 &&
            ink_json_str(j, ink_json_find(j, 0, "text"), weather, sizeof weather) > 0) {
            char *cut = strchr(weather, ' '); /* the temperature and the sky; the day's range is on the weather page */
            if (cut && (cut = strchr(cut + 1, ' '))) *cut = 0;
        }
        for (int i = 0; i < 5; i++) {
            if (N.cards[i].id[0] || !today_match(j, i)) continue;
            ink_card_t *c = &N.cards[i];
            ink_json_str(j, ink_json_find(j, 0, "id"), c->id, sizeof c->id);
            ink_json_str(j, ink_json_find(j, 0, "title"), c->title, sizeof c->title);
            ink_json_str(j, ink_json_find(j, 0, "text"), c->text, sizeof c->text);
            ink_json_str(j, ink_json_find(j, 0, "action"), c->action, sizeof c->action);
            ink_json_str(j, ink_json_find(j, 0, "icon"), c->icon, sizeof c->icon);
            int64_t rev = 0;
            ink_json_int(j, ink_json_find(j, 0, "rev"), &rev);
            c->rev = (uint32_t)rev;
        }
        p = end + 1;
        if (*p == ',') p++;
    }
    osal_free(j);
}

static void read_cards(void) {
    today_load();
    memset(N.cards, 0, sizeof N.cards);
    weather[0] = 0;
    N.cards_ok = false;
    ink_call_opts_t opts = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_DIRECT, .done = cards_reply};
    ink_cap_call("app.cards", NULL, &opts);
    if (!N.cards_ok) memset(N.cards, 0, sizeof N.cards);
    N.cards_rev = integer("app.cards_rev");
    N.cards_at = osal_now_us();
    for (int i = 0; i < 5; i++) {
        ink_card_t *c = &N.cards[i];
        snprintf(N.card_objects[i], sizeof N.card_objects[i], "reader:%s", c->id);
        ink_ui_object_ref_t object = {N.card_objects[i], c->rev};
        N.card_fields[i] = (ink_ui_field_spec_t){"reader", "card", c->title, c->text, object, false};
        N.card_actions[i] = (ink_ui_action_spec_t){"reader", N.card_fields[i].label, c->action, "{}", object, c->id[0] && c->action[0]};
    }
    N.today = (ink_ui_page_spec_t){"home", TR("主屏"), {"app.cards", (uint32_t)N.cards_rev}, N.card_fields, 5, N.card_actions, 5};
    today_model();
    ink_probe_count("home.cards_updates");
}

static void card_clicked(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (!current_session(N.epoch)) return;
    ink_ui_action_target_t target = {ink_ui_content_version(), N.card_actions[i].object};
    ink_receipt_t rc;
    ink_ui_semantics_action(&N.today, N.card_actions[i].id, &target, INK_SRC_TOUCH, &rc);
}

static void cards_due(lv_timer_t *timer) {
    (void)timer;
    N.cards_timer = NULL;
    ink_shell_home_settle();
}

/* A refusal on the home page is a passing note, not a state: it goes after a few seconds, and with the page (trial
 * report 2026-11-11: "这个应用还没有可打开的内容" stayed on the screen until the member changed). */
#define NOTE_MS 4000
static const char *idle = ""; /* the bottom line when no note passes: the AI tip, or a child's time left */
static void note_due(lv_timer_t *timer) {
    (void)timer;
    N.note_timer = NULL;
    N.result[0] = 0;
    if (N.message) lv_label_set_text(N.message, idle);
}
void home_note(const char *text) {
    snprintf(N.result, sizeof N.result, "%s", text);
    if (N.message) lv_label_set_text(N.message, N.result);
    if (N.note_timer) lv_timer_reset(N.note_timer);
    else {
        N.note_timer = lv_timer_create(note_due, NOTE_MS, NULL);
        lv_timer_set_repeat_count(N.note_timer, 1);
    }
}
void ink_shell_home_settle(void) {
    if (!N.home || strcmp(ink_ui_top_name(), "home") || !ink_ui_page_current() ||
        !current_session(N.epoch) || ink_power_state() == INK_PWR_LOCKED || N.cards_rev == integer("app.cards_rev")) return;
    uint64_t now = osal_now_us();
    if (now < N.cards_at + 1000000) {
        if (!N.cards_timer) {
            N.cards_timer = lv_timer_create(cards_due, (uint32_t)((N.cards_at + 1000000 - now + 999) / 1000), NULL);
            lv_timer_set_repeat_count(N.cards_timer, 1);
        }
        return;
    }
    ink_ui_content_changed();
    draw_home_content();
}

/* "11月12日 周四 · 12° 小雨": the weather when there is one, else the lunar date. */
static void date_line(char *out, size_t cap) {
    char clock[16], date[96];
    ink_shell_time_strings(clock, sizeof clock, date, sizeof date);
    const char *dot = strstr(date, " · ");
    if (dot && weather[0]) snprintf(out, cap, "%.*s · %s", (int)(dot - date), date, weather);
    else snprintf(out, cap, "%s", date);
}
static char shown_date[160]; /* what the date line says (its label may be cut with dots) */
void home_refresh(void) {
    char text[160];
    date_line(text, sizeof text);
    if (N.date && strcmp(shown_date, text)) {
        memcpy(shown_date, text, sizeof text);
        lv_label_set_text(N.date, text);
        ink_ui_hint_obj(N.date, INK_REFRESH_UI);
    }
}
/* The tip stays until the AI has answered this member a few times; a history not read yet is asked for off the UI
 * task, and the tip goes when the count comes back. */
static void hint_reply(void *ctx, const ink_receipt_t *rc) {
    long long turns = 0;
    if (!current_session((uint32_t)(uintptr_t)ctx) || child() || rc->status != INK_RC_DONE || !rc->result ||
        sscanf(rc->result, "{\"total\":%lld", &turns) != 1 || turns < AI_HINT_TURNS) return;
    idle = "";
    if (N.message && !N.result[0]) lv_label_set_text(N.message, idle);
}
static bool ai_new(void) {
    const ink_db_def_t *def = ink_db_find_def("ai.history");
    ink_db_t *db = def ? ink_db_open_cached_member(def, ink_member_current()->id) : NULL;
    if (!db) {
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_UI, .done = hint_reply, .ctx = (void *)(uintptr_t)N.epoch,
                             .member = ink_member_current()->id};
        ink_cap_call("ai.history.list", NULL, &o);
        return true;
    }
    int turns = ink_db_count(db);
    ink_db_close(db);
    return turns < AI_HINT_TURNS;
}
static void chevron(lv_obj_t *row, int h) {
    ink_ui_icon(row, "next", INK_ICON_SMALL, INK_ICON_LINE, width() - INK_READING_MARGIN, (h - INK_ICON_SMALL) / 2, false);
}
/* What is drawn, as the AI reads it (UI task only). */
static struct { char cards[384], apps[256], book[40]; int card[HOME_CARDS], count; } seen;
/* A card: what it is on top, where you are under it; a 《title》 it starts with goes on top (继续读《书名》). */
static lv_obj_t *card_row(const char *icon, const ink_card_t *c, int y, bool tap) {
    const char *end = strstr(c->text, "》"), *rest = c->text;
    if (!strncmp(c->text, "《", strlen("《")) && end) rest = end + strlen("》") + strspn(end + strlen("》"), " ");
    char title[160];
    snprintf(title, sizeof title, "%s%.*s", c->title, (int)(rest - c->text), c->text);
    lv_obj_t *row = ink_ui_list_row(N.content, icon, title, rest, INK_MARGIN, y, width(), true, NULL, NULL);
    if (tap) chevron(row, ink_ui_row_double());
    size_t n = strlen(seen.cards);
    snprintf(seen.cards + n, sizeof seen.cards - n, "%s%s%s%s", n ? "；" : "", c->title, c->text[0] ? "：" : "", c->text);
    return row;
}
static int tile_h(void) {
    return S(INK_GAP_M) + INK_ICON_APP + S(INK_GAP_S) + ink_ui_line_height(INK_FONT_LIST) + S(INK_GAP_M);
}
/* A favourite: its icon over its name, without a circle; the whole cell is the target. */
static void tile(const char *which, int at, int top) {
    int w = width() / 3, h = tile_h();
    lv_obj_t *cell = ink_ui_box(N.content, INK_MARGIN + at % 3 * w, top + at / 3 * (h + S(INK_GAP_M)), w, h, 0,
                                INK_SURFACE_WHITE, false);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
    ink_ui_icon(cell, home_icon(which), INK_ICON_APP, INK_ICON_LINE, (w - INK_ICON_APP) / 2, S(INK_GAP_M), false);
    lv_obj_t *name = ink_ui_line(cell, home_label(which), INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, S(INK_GAP_M) + INK_ICON_APP + S(INK_GAP_S), w);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
    const ink_app_def_t *a = app_def(which);
    icon_bind(cell, a ? a->id : "settings"); /* a name that outlives the favourites being reread */
    size_t n = strlen(seen.apps);
    snprintf(seen.apps + n, sizeof seen.apps - n, "%s（%s）；", home_label(which), which);
}

void draw_home_content(void) {
    if (!N.content) return;
    if (N.cards_timer) { lv_timer_delete(N.cards_timer); N.cards_timer = NULL; }
    N.date = N.message = N.kid_time_label = NULL;
    lv_obj_clean(N.content);
    memset(&seen, 0, sizeof seen);
    read_cards();
    int call = ink_activity_home_offset(), y = S(INK_GAP_M), line = ink_ui_line_height(INK_FONT_NOTE), h = ink_ui_row_double();
    if (call > 0) { ink_activity_home_draw(N.content, 0); y += INK_CONTROL; } /* a call in progress: its banner on top */
    date_line(shown_date, sizeof shown_date);
    N.date = ink_ui_line(N.content, shown_date, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, y, width());
    y += ink_ui_line_height(INK_FONT_LIST) + S(INK_GAP_M);
    /* From the bottom up, so the favourites stay where the hand knows them, with or without cards. */
    int bottom = INK_CANVAS_H - INK_STATUS_BAR - S(INK_MARGIN) - (child() ? 2 : 1) * line;
    int all = bottom - S(INK_GAP_M) - INK_CONTROL, grid = all - S(INK_GAP_L) - 2 * tile_h() - S(INK_GAP_M);
    int room = grid - S(INK_GAP_M);
    if (call < 0 && y + h <= room && ink_activity_home_draw(N.content, y)) { y += h + S(INK_GAP_S); seen.count++; }
    ink_card_t book;
    if (seen.count < HOME_CARDS && y + h <= room && bookmark(&book)) { /* the book being read, first */
        ink_shell_bind(card_row("book", &book, y, true), "shell.resume", NULL);
        snprintf(seen.book, sizeof seen.book, "%s", book.action[0] ? book.action : "shell.resume");
        y += h + S(INK_GAP_S); seen.count++;
    }
    for (int i = 1; i < 5 && seen.count < HOME_CARDS && y + h <= room; i++) {
        if (!N.cards[i].id[0]) continue; /* nothing to show: no card at all */
        lv_obj_t *row = card_row(today_icon(i - 1), &N.cards[i], y, N.card_actions[i].enabled);
        if (N.card_actions[i].enabled && ink_cap_available(N.card_actions[i].capability)) {
            lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(row, card_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
        seen.card[seen.count++] = i; y += h + S(INK_GAP_S);
    }
    ink_card_t failed = {0}; /* a failed read is said, not shown as nothing */
    snprintf(failed.title, sizeof failed.title, "%s", TR("卡片没能读取"));
    if (!N.cards_ok && seen.count < HOME_CARDS && y + h <= room) card_row("warn", &failed, y, false);
    const char *apps[HOME_APPS];
    for (int i = 0, n = home_apps(apps), shown = 0; i < n && shown < HOME_APPS; i++)
        if (home_usable(apps[i])) tile(apps[i], shown++, grid);
    lv_obj_t *row = ink_ui_list_row(N.content, "grid", TR("全部应用"), NULL, INK_MARGIN, all, width(), false, NULL, NULL);
    chevron(row, INK_CONTROL);
    ink_shell_bind(row, "shell.apps", NULL);
    idle = child() ? N.kid_time : ai_new() ? TR("按住侧边 AI 键，直接说话") : "";
    for (int i = 0; i < (child() ? 2 : 1); i++) {
        lv_obj_t *l = ink_ui_line(N.content, i ? TR("爸爸妈妈可以看到你的记录") : N.result[0] ? N.result : idle,
                                  INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, bottom + i * line, width());
        lv_obj_set_style_text_color(l, ink_ui_secondary(), 0);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        if (!i) N.message = l;
    }
    if (child()) { N.kid_time_label = N.message; kid_time_read(); }
    ink_ui_hint_obj(N.content, INK_REFRESH_TEXT);
}

void ink_shell_home_create(lv_obj_t *screen) {
    uint32_t session = __atomic_load_n(&N.session, __ATOMIC_ACQUIRE);
    if (N.epoch != session || strcmp(N.member, ink_member_current()->id) || !ink_member_space_open()) {
        ink_shell_close();
        N.search[0] = N.result[0] = N.picked_ssid[0] = 0;
        N.kid_time[0] = 0;
        memset(N.wifi_ssid, 0, sizeof N.wifi_ssid);
        N.apps_page = N.wifi_page = N.wifi_count = 0;
        memset(&N.notify_position, 0, sizeof N.notify_position);
        N.apps_pages = 1;
        N.more = N.scan_busy = false;
        N.control_count = 0;
        N.epoch = session;
        snprintf(N.member, sizeof N.member, "%s", ink_member_current()->id);
    }
    N.home = screen;
    N.initialized = true;
    ink_shell_statusbar(screen, true);
    N.content = ink_ui_box(screen, 0, INK_STATUS_BAR, INK_CANVAS_W, INK_CANVAS_H - INK_STATUS_BAR, 0, INK_SURFACE_WHITE, false);
    draw_home_content();
}
/* Nothing to turn: everything is on the one page (D-135). */
bool ink_shell_home_swipe(ink_swipe_dir_t dir) { (void)dir; return false; }
bool ink_shell_home_key(ink_key_t key, bool long_press) { (void)key; (void)long_press; return false; }

void ink_shell_home_destroy(void) {
    icon_close();
    if (N.cards_timer) { lv_timer_delete(N.cards_timer); N.cards_timer = NULL; }
    if (N.note_timer) { lv_timer_delete(N.note_timer); N.note_timer = NULL; N.result[0] = 0; }
    N.home = N.content = N.date = N.message = NULL;
    N.kid_time_label = NULL;
}

int ink_shell_home_snapshot(char *buf, size_t cap) {
    if (!current_session(N.epoch)) { if (cap) buf[0] = 0; return -1; }
    char activity[512], date[160], board[64];
    ink_activity_card_snapshot(activity, sizeof activity);
    date_line(date, sizeof date);
    snprintf(board, sizeof board, "页面=%s", ink_shell_board());
    ink_ui_field_spec_t fields[] = {
        {.id = "board", .role = "status", .label = "页面", .value = board},
        {.id = "member", .role = "text", .label = "当前", .value = ink_member_current()->name},
        {.id = "date", .role = "text", .label = "日期", .value = date},
        {.id = "cards", .role = "text", .label = "卡片", .value = seen.cards[0] ? seen.cards : "没有"},
        {.id = "apps", .role = "text", .label = "常用", .value = seen.apps},
        {.id = "status", .role = "status", .label = "状态", .value = child() ? N.kid_time : N.result},
        {.id = "notice", .role = "text", .label = "提示", .value = child() ? "爸爸妈妈可以看到你的记录" : idle},
        {.id = "activity", .role = "group", .label = "正在做的事", .value = activity}};
    static const char *const CAPS[] = {"shell.launch", "shell.resume", "shell.apps", "shell.search", "shell.icon_menu",
                                       "shell.favorite", "shell.today_edit", "shell.control", "shell.tasks"};
    ink_ui_action_spec_t actions[sizeof CAPS / sizeof *CAPS + HOME_CARDS + 1];
    size_t n = 0;
    for (size_t i = 0; i < sizeof CAPS / sizeof *CAPS; i++)
        actions[n++] = (ink_ui_action_spec_t){.id = CAPS[i], .label = CAPS[i], .capability = CAPS[i], .enabled = i != 1 || seen.book[0]};
    if (seen.book[0]) actions[n++] = (ink_ui_action_spec_t){.id = "book", .label = seen.book, .capability = seen.book, .enabled = true};
    for (int i = 0; i < seen.count; i++) if (seen.card[i]) actions[n++] = N.card_actions[seen.card[i]];
    ink_ui_page_spec_t page = {.id = "home", .title = ink_shell_board(), .object = {"home", ink_ui_generation()},
        .fields = fields, .field_count = sizeof fields / sizeof *fields, .actions = actions, .action_count = n};
    return ink_ui_semantics_summary(&page, buf, cap);
}

/* All apps, the second page from 全部应用 (D-135): search on top, the apps in groups, pages instead of scrolling.
 * A search lists only the matches, those starting with it first, and offers the search of everything (as KISS). */
enum { TOOLS_MAX = 48 };
typedef struct { const char *id, *title, *icon; int group, rank; } tool_t;
static const char *const GROUPS[] = {TR_NOOP("读和听"), TR_NOOP("记和联系"), TR_NOOP("工具")};
static const char *const GROUP_IDS[] = {" reader listen music news word ", " phone message contacts memo upcoming "};
static const ink_activity_t APPS;
static lv_obj_t *apps_box;
static int apps_h; /* the list's height, below the search */
static int tools(tool_t out[TOOLS_MAX]) {
    int n = 0;
    size_t q = strlen(N.search);
    for (int i = 0; i <= ink_app_count() && n < TOOLS_MAX; i++) {
        const ink_app_def_t *a = i < ink_app_count() ? ink_app_at(i) : NULL;
        const char *id = a ? a->id : "settings", *title = a ? TR(a->title) : TR("设置");
        if (!strcmp(id, "home") || !home_usable(id)) continue;
        char key[40];
        snprintf(key, sizeof key, " %s ", id);
        int g = strstr(GROUP_IDS[0], key) ? 0 : strstr(GROUP_IDS[1], key) ? 1 : 2;
        int rank = g < 2 ? (int)(strstr(GROUP_IDS[g], key) - GROUP_IDS[g]) : i;
        if (q) {
            if (!strstr(title, N.search) && !strstr(id, N.search)) continue;
            g = 0; rank = strncmp(title, N.search, q) && strncmp(id, N.search, q);
        }
        int k = n++;
        for (; k && (out[k - 1].group > g || (out[k - 1].group == g && out[k - 1].rank > rank)); k--) out[k] = out[k - 1];
        out[k] = (tool_t){id, title, def_icon(a), g, rank};
    }
    return n;
}
/* Lays the rows out page by page: draws the page being shown and lists it for the AI. Returns the pages. */
static int apps_walk(bool draw, char *list, size_t cap) {
    static tool_t items[TOOLS_MAX]; /* UI task only, never re-entered: off its small stack */
    int count = tools(items), rows = apps_h - INK_CONTROL - INK_MARGIN, y = 0, page = 0, last = -1, row_y = 0;
    size_t len = 0;
    lv_obj_t *box = NULL;
    for (int i = 0; i <= count; i++) {
        bool title = i < count && !N.search[0] && items[i].group != last, more = i == count && N.search[0];
        int need = (title ? INK_GROUP_TITLE : 0) + (i < count || more ? INK_CONTROL : 0) + (more ? (count ? INK_GAP_M : INK_CONTROL) : 0);
        if (!need) break;
        if (y && y + need > rows) { page++; y = 0; box = NULL; }
        bool here = page == N.apps_page;
        if (title) {
            last = items[i].group;
            lv_obj_t *l = here && draw ? ink_ui_line(apps_box, TR(GROUPS[last]), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN,
                                                     y + (INK_GROUP_TITLE - ink_ui_line_height(INK_FONT_NOTE)) / 2, width()) : NULL;
            if (l) lv_obj_set_style_text_color(l, ink_ui_secondary(), 0);
            y += INK_GROUP_TITLE; box = NULL;
        }
        if (more) { /* the search of everything: contacts, notes, files and settings too */
            if (here && draw) {
                if (!count) ink_ui_notice(apps_box, TR("没有找到应用"), y);
                char args[192];
                ink_jw_t w; ink_jw_init(&w, args, sizeof args);
                ink_jw_raw(&w, "{\"query\":"); ink_jw_str(&w, N.search); ink_jw_raw(&w, "}");
                lv_obj_t *r = ink_ui_list_row(apps_box, "search", TR("搜索全部内容"), NULL, INK_MARGIN, /* the words are in the field above */
                                              y + (count ? INK_GAP_M : INK_CONTROL), width(), false, NULL, NULL);
                chevron(r, INK_CONTROL);
                ink_shell_bind(r, "search.show", args);
            }
            if (here && list && len < cap) len += (size_t)snprintf(list + len, cap - len, "%s（search.show）；", TR("搜索全部内容"));
            break;
        }
        if (here && draw) {
            if (!box) { box = ink_ui_box(apps_box, INK_MARGIN, y, width(), 0, 0, INK_SURFACE_STATIC, false); row_y = 0; }
            lv_obj_set_height(box, ink_ui_px(row_y + INK_CONTROL));
            lv_obj_t *r = ink_ui_list_row(box, items[i].icon, items[i].title, NULL, 0, row_y, width(), false, NULL, NULL);
            row_y += INK_CONTROL;
            icon_bind(r, items[i].id);
        }
        if (here && list && len < cap) len += (size_t)snprintf(list + len, cap - len, "%s（%s）；", items[i].title, items[i].id);
        y += INK_CONTROL;
    }
    return page + 1;
}
static void apps_page_clicked(lv_event_t *e) {
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .reply_exec = INK_EXEC_UI};
    ink_cap_call((intptr_t)lv_event_get_user_data(e) > 0 ? "shell.apps_next" : "shell.apps_prev", NULL, &o);
}
static void draw_apps(void) {
    lv_obj_clean(apps_box);
    N.apps_pages = apps_walk(false, NULL, 0);
    if (N.apps_page >= N.apps_pages) N.apps_page = N.apps_pages - 1;
    apps_walk(true, NULL, 0);
    if (N.apps_pages > 1) ink_ui_pagebar(apps_box, N.apps_page + 1, N.apps_pages, apps_h - INK_MARGIN - INK_CONTROL, apps_page_clicked);
    ink_ui_hint_obj(apps_box, INK_REFRESH_UI);
}
static void search_entered(void *ctx, const char *value) {
    if (!current_session((uint32_t)(uintptr_t)ctx)) return;
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    if (!args) return;
    ink_args_init(args);
    ink_args_str(args, "query", value);
    ink_receipt_t rc;
    ink_cap_invoke("shell.search", args, INK_SRC_TOUCH, 0, &rc);
    osal_free(args);
}
static void apps_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, TR("全部应用"));
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS;
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEARCH, .max = 64, .hint = TR("应用的名字"), .initial = N.search};
    lv_obj_t *head = ink_ui_box(screen, 0, y, INK_CANVAS_W, ink_textfield_height(INK_FIELD_SEARCH), 0, INK_SURFACE_WHITE, false);
    lv_obj_t *search = ink_textfield_create(head, &cfg, search_entered, (void *)(uintptr_t)N.epoch, false);
    lv_obj_set_pos(search, ink_ui_px(INK_MARGIN), 0); /* in the canvas, wherever the panel puts it */
    ink_textfield_style(search, INK_FIELD_SEARCH, TR("搜索"));
    y += ink_textfield_height(INK_FIELD_SEARCH) + INK_GAP_XS; /* the group name below has its own room */
    apps_h = INK_CANVAS_H - y;
    apps_box = ink_ui_box(screen, 0, y, INK_CANVAS_W, apps_h, 0, INK_SURFACE_WHITE, false);
    draw_apps();
}
static void apps_destroy(void) { icon_close(); apps_box = NULL; }
static bool apps_key(ink_key_t key, bool long_press) {
    if (long_press || (key != INK_KEY_NEXT && key != INK_KEY_PREV)) return false;
    ink_receipt_t rc;
    ink_cap_invoke(key == INK_KEY_NEXT ? "shell.apps_next" : "shell.apps_prev", NULL, INK_SRC_KEY, 0, &rc);
    return true;
}
static bool apps_swipe(ink_swipe_dir_t dir) {
    return (dir == INK_SWIPE_LEFT || dir == INK_SWIPE_RIGHT) && apps_key(dir == INK_SWIPE_LEFT ? INK_KEY_NEXT : INK_KEY_PREV, false);
}
static int apps_snapshot(char *buf, size_t cap) {
    if (!current_session(N.epoch) || !apps_box) { if (cap) buf[0] = 0; return -1; }
    static char list[2048] INK_PSRAM_BSS;
    char board[64], at[64];
    list[0] = 0;
    apps_walk(false, list, sizeof list);
    snprintf(board, sizeof board, "页面=%s", ink_shell_board());
    snprintf(at, sizeof at, "工具页=%d/%d", N.apps_page + 1, N.apps_pages);
    ink_ui_field_spec_t fields[] = {
        {.id = "board", .role = "status", .label = "页面", .value = board},
        {.id = "member", .role = "text", .label = "当前", .value = ink_member_current()->name},
        {.id = "search", .role = "text", .label = "查找", .value = N.search},
        {.id = "tools_page", .role = "status", .label = "工具页", .value = at},
        {.id = "tools", .role = "text", .label = "应用", .value = list}};
    static const char *const CAPS[] = {"shell.launch", "shell.icon_menu", "shell.search", "shell.apps_prev", "shell.apps_next", "search.show", "shell.back"};
    bool on[] = {true, true, true, N.apps_page > 0, N.apps_page + 1 < N.apps_pages, N.search[0] != 0, true};
    ink_ui_action_spec_t actions[sizeof CAPS / sizeof *CAPS];
    for (size_t i = 0; i < sizeof CAPS / sizeof *CAPS; i++) actions[i] = (ink_ui_action_spec_t){.id = CAPS[i], .label = CAPS[i], .capability = CAPS[i], .enabled = on[i]};
    ink_ui_page_spec_t page = {.id = APPS.name, .title = ink_shell_board(), .object = {"apps", ink_ui_generation()},
        .fields = fields, .field_count = sizeof fields / sizeof *fields, .actions = actions, .action_count = sizeof actions / sizeof *actions};
    return ink_ui_semantics_summary(&page, buf, cap);
}
static const ink_activity_t APPS = {.name = "Apps", .app = "home", .create = apps_create, .destroy = apps_destroy, .on_key = apps_key,
                                    .on_swipe = apps_swipe, .snapshot = apps_snapshot};

static void apps_call(ink_call_t *c) {
    if (!strcmp(c->cap->id, "shell.apps_next") || !strcmp(c->cap->id, "shell.apps_prev")) {
        int to = N.apps_page + (!strcmp(c->cap->id, "shell.apps_next") ? 1 : -1);
        c->rc.status = to < 0 || to >= N.apps_pages ? INK_RC_REJECTED : INK_RC_DONE;
        snprintf(c->rc.say, sizeof c->rc.say, "%s", to < 0 ? TR("已经是第一页") : to >= N.apps_pages ? TR("已经是最后一页") : "");
        if (c->rc.status != INK_RC_DONE) return;
        N.apps_page = to;
        ink_ui_content_changed();
        ink_ui_hint_frame(INK_REFRESH_FLASH_UI); /* D-108: all of it is new, no ghost of the last page */
        draw_apps();
        ink_probe_count("home.page_turns");
        return;
    }
    char query[sizeof N.search] = "";
    ink_args_get_str(&c->args, "query", query, sizeof query);
    memcpy(N.search, query, sizeof query);
    N.apps_page = 0;
    ink_text_blur(0);
    if (!strcmp(ink_ui_page_name(), APPS.name)) ink_ui_redraw();
    else ink_ui_push(&APPS, NULL);
    snprintf(c->rc.object, sizeof c->rc.object, "screen:%s", ink_shell_board());
}
int home_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "shell.apps", .app = "home", .title = "打开全部应用", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = apps_call},
        {.id = "shell.search", .app = "home", .title = "在全部应用里找应用", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = apps_call,
         .input = "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"maxLength\":64}},\"required\":[\"query\"]}"},
        {.id = "shell.apps_next", .app = "home", .title = "全部应用的下一页", .activity = "Apps", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = apps_call},
        {.id = "shell.apps_prev", .app = "home", .title = "全部应用的上一页", .activity = "Apps", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = apps_call}};
    return ink_cap_register(caps, sizeof caps / sizeof *caps);
}
