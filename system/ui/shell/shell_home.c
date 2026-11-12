/* ui-scale-runtime: dimension width Canvas width minus theme margins, shared by all shell pages. */
/* ui-scale-runtime: dimension y tile and notifications accept a theme-derived row position. */
/* ui-scale-runtime: dimension ink_activity_card_draw,ink_activity_home_offset F6 returns theme-sized row heights and offsets. */
/* ui-scale-runtime: dimension F.y,heading_h,notice_h,home_room,below_heading Page flow: theme sizes added up, plus text measured at the current size. */
/* ui-scale-runtime: dimension first_h The lead card reserves two full title lines plus theme padding. */
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
    if (N.kid_time_label) lv_label_set_text(N.kid_time_label, N.kid_time);
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

const char *const CARD_APPS[] = {"reader", "listen", "news", "word", "upcoming"};
static const char *const CARD_TITLES[] = {TR_NOOP("继续读"), TR_NOOP("继续听"), TR_NOOP("今日报纸"), TR_NOOP("单词"), TR_NOOP("接下来")};
static const char *const CARD_EMPTY[] = {TR_NOOP("还没有开始读书"), TR_NOOP("还没有在听的内容"), TR_NOOP("今天的还没到"), TR_NOOP("还没有记下的词"), TR_NOOP("还没有接下来的事")};

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
    N.cards_ok = false;
    ink_call_opts_t opts = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_DIRECT, .done = cards_reply};
    ink_cap_call("app.cards", NULL, &opts);
    if (!N.cards_ok) memset(N.cards, 0, sizeof N.cards);
    N.cards_rev = integer("app.cards_rev");
    N.cards_at = osal_now_us();
    for (int i = 0; i < 5; i++) {
        ink_card_t *c = &N.cards[i];
        snprintf(N.card_objects[i], sizeof N.card_objects[i], "%s:%s", CARD_APPS[i], c->id);
        ink_ui_object_ref_t object = {N.card_objects[i], c->rev};
        N.card_fields[i] = (ink_ui_field_spec_t){CARD_APPS[i], "card", c->title[0] ? c->title : TR(CARD_TITLES[i]),
                                                !N.cards_ok ? TR("卡片没能读取") : c->id[0] ? c->text : TR(CARD_EMPTY[i]), object, false};
        N.card_actions[i] = (ink_ui_action_spec_t){CARD_APPS[i], N.card_fields[i].label, c->action, "{}", object,
                                                  c->id[0] && c->action[0]};
    }
    N.today = (ink_ui_page_spec_t){"home", TR("今日"), {"app.cards", (uint32_t)N.cards_rev}, N.card_fields, 5, N.card_actions, 5};
    today_model();
    if (child()) kid_today_model();
    ink_probe_count("home.cards_updates");
}

static void card_clicked(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (!current_session(N.epoch)) return;
    ink_ui_action_target_t target = {ink_ui_content_version(), N.card_actions[i].object};
    ink_receipt_t rc;
    ink_ui_semantics_action(&N.today, N.card_actions[i].id, &target, INK_SRC_TOUCH, &rc);
}
static void bind_card(lv_obj_t *obj, int i) {
    if (N.card_actions[i].enabled && ink_cap_available(N.card_actions[i].capability)) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(obj, card_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
}

static void cards_due(lv_timer_t *timer) {
    (void)timer;
    N.cards_timer = NULL;
    ink_shell_home_settle();
}

/* A refusal on the home page is a passing note, not a state: it goes after a few seconds, and with the page (trial
 * report 2026-11-11: "这个应用还没有可打开的内容" stayed on the screen until the member changed). */
#define NOTE_MS 4000
static void note_due(lv_timer_t *timer) {
    (void)timer;
    N.note_timer = NULL;
    N.result[0] = 0;
    if (N.message) lv_label_set_text(N.message, "");
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
    if (!N.home || N.home_page != 0 || strcmp(ink_ui_top_name(), "home") || !ink_ui_page_current() ||
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

static void draw_dots(void) {
    lv_obj_clean(N.dots);
    for (int i = 0; i < 3; i++)
        ink_ui_box(N.dots,
                   INK_CANVAS_W / 2 + (i - 1) * INK_GAP_L - INK_PAGE_DOT / 2,
                   (INK_GAP_XL - INK_PAGE_DOT) / 2, INK_PAGE_DOT, INK_PAGE_DOT, -1,
                   i == N.home_page ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, i != N.home_page);
    ink_ui_hint_obj(N.dots, INK_REFRESH_FAST);
}

static bool matches(const char *title, const char *id) {
    return !N.search[0] || strstr(title, N.search) || strstr(id, N.search);
}
static void tile(lv_obj_t *parent, const char *id, const char *title, const char *icon, int col, int row, int y,
                 bool compact, int columns) {
    title = TR(title);
    if (child() && !kid_app_visible(id)) return;
    if (N.home_page == 2 && !matches(title, id)) return;
    int cell = width() / columns, d = compact ? INK_CIRCLE : INK_CIRCLE_APP;
    int x = INK_MARGIN + col * cell + (cell - d) / 2;
    int at = y + row * (d + INK_GAP_S + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_L);
    lv_obj_t *button =
        ink_ui_circle(parent, icon, x, at, d, compact ? INK_ICON_LIST : INK_ICON_APP, INK_SURFACE_STATIC, NULL, NULL);
    char json[96];
    snprintf(json, sizeof json, "{\"id\":\"%s\"}", id);
    if (N.home_page == 2) icon_bind(button, id);
    else ink_shell_bind(button, "shell.launch", json);
    lv_obj_t *label = ink_ui_line(parent, title, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + col * cell,
                                  at + d + INK_GAP_S, cell);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

static const char *const APP_IDS[] = {"listen", "news", "word", "upcoming", "clock", "camera", "gallery", "settings"};
static const char *const APP_TITLES[] = {TR_NOOP("听"), TR_NOOP("报纸"), TR_NOOP("单词"), TR_NOOP("接下来"), TR_NOOP("时钟"), TR_NOOP("相机"), TR_NOOP("相册"), TR_NOOP("设置")};
const char *const APP_ICONS[] = {INK_UI_ICON(headphones), INK_UI_ICON(news),  INK_UI_ICON(word),
                                        INK_UI_ICON(calendar),   INK_UI_ICON(clock), INK_UI_ICON(camera),
                                        INK_UI_ICON(gallery),    INK_UI_ICON(gear)};
static const char *const GROUP_IDS[][4] = {{"reader", "listen", "news", "word"},
                                           {"memo", "upcoming", "phone", "message"}};
static const char *const GROUP_TITLES[][4] = {{TR_NOOP("阅读"), TR_NOOP("听"), TR_NOOP("报纸"), TR_NOOP("单词")}, {TR_NOOP("备忘"), TR_NOOP("接下来"), TR_NOOP("电话"), TR_NOOP("短信")}};
static const char *const GROUP_ICONS[][4] = {
    {INK_UI_ICON(book), INK_UI_ICON(headphones), INK_UI_ICON(news), INK_UI_ICON(word)},
    {INK_UI_ICON(memo), INK_UI_ICON(calendar), INK_UI_ICON(phone), INK_UI_ICON(message)}};
typedef struct {
    const char *id, *title, *icon;
} tool_t;
enum { TOOLS_MAX = 48 }; /* every registered app and the built-in entries: the Apps page turns pages, no cut-off */
static int tools(tool_t out[TOOLS_MAX]) {
    int count = 0;
    for (int i = 0; i < ink_app_count() && count < TOOLS_MAX - 7; i++) {
        const ink_app_def_t *a = ink_app_at(i);
        if (!a->entry || !ink_app_visible(a) || !strcmp(a->id, "home") || !strcmp(a->id, "reader")) continue;
        if (!strcmp(a->id, "contacts")) continue; /* Already in the contact group. */
        if (!strcmp(a->id, "phone")) continue; /* Already in the contact group. */
        if (!strcmp(a->id, "memo")) continue; /* Already in the memo/contact group. */
        if (!strcmp(a->id, "upcoming")) continue; /* The calendar is 接下来 in the same group. */
        if (child() && (!strcmp(a->id, "listen") || !strcmp(a->id, "news") || !strcmp(a->id, "word"))) continue;
        out[count++] = (tool_t){a->id, a->title, a->icon ? a->icon : INK_UI_ICON(grid)};
    }
    static const tool_t MORE[] = {{"clock", TR_NOOP("时钟"), INK_UI_ICON(clock)},     {"camera", TR_NOOP("相机"), INK_UI_ICON(camera)},
                                  {"gallery", TR_NOOP("相册"), INK_UI_ICON(gallery)}, {"files", TR_NOOP("文件"), INK_UI_ICON(folder)},
                                  {"remote", TR_NOOP("网页"), INK_UI_ICON(globe)},    {"ai", TR_NOOP("对话记录"), INK_UI_ICON(ai)},
                                  {"settings", TR_NOOP("设置"), INK_UI_ICON(gear)}};
    for (unsigned i = 0; i < sizeof MORE / sizeof *MORE; i++) {
        if (child() && (i == 3 || i == 4 || i == 6)) continue;
        bool present = false;
        for (int k = 0; k < count; k++) present |= !strcmp(out[k].id, MORE[i].id);
        if (!present && count < TOOLS_MAX) out[count++] = MORE[i];
    }
    for (int i = 0; i < count; i++) if (!strcmp(out[i].id, "message")) {
        memmove(&out[i], &out[i + 1], (size_t)(count - i - 1) * sizeof out[0]);
        count--; break; /* SMS already belongs to the contact group. */
    }
    return count;
}

/* Under a page's big heading, on Today and Apps. */
static int below_heading(void) {
    return INK_GAP_S + ink_ui_line_height(INK_FONT_HEADING) + INK_GAP_XS;
}
static int home_room(void) {
    return INK_CANVAS_H - INK_STATUS_BAR - INK_DOCK - INK_MARGIN - INK_GAP_XL;
}

typedef struct {
    char *buf;
    size_t cap;
    int n;
} tile_list_t;

/* Groups and their rows of tiles, page by page; a group's name stays with its first row. A search lays
 * out only what matches, from the first page, and leaves out groups with nothing found. Draws the
 * tiles of the page being shown and lists them for the AI. Returns the pages. */
static int apps_walk(bool draw, tile_list_t *list) {
    static const char *const groups[] = {TR_NOOP("读和听"), TR_NOOP("记和联系"), TR_NOOP("工具")};
    static tool_t items[TOOLS_MAX], shown[TOOLS_MAX]; /* UI task only, never re-entered: off its small stack */
    int count = tools(items), top = below_heading() + ink_textfield_height(INK_FIELD_SEARCH) + INK_GAP_L, y = top, page = 0;
    int name_h = ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S;
    int row_h = INK_CIRCLE + INK_GAP_S + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_L;
    for (int g = 0; g < 3; g++) {
        int total = 0;
        int columns = g == 1 ? 5 : 4;
        for (int k = 0; k < (g < 2 ? columns : count); k++) {
            tool_t t = g == 1 && k == 4 ? (tool_t){"contacts", TR_NOOP("联系人"), INK_UI_ICON(person)} : g < 2 ? (tool_t){GROUP_IDS[g][k], GROUP_TITLES[g][k], GROUP_ICONS[g][k]} : items[k];
            if (child() && !kid_app_visible(t.id)) continue;
            if (matches(TR(t.title), t.id)) shown[total++] = t;
        }
        for (int i = 0; i < total; i += columns) {
            int need = (i ? 0 : name_h) + row_h;
            if (y + need - INK_GAP_L > home_room()) {
                page++;
                y = top;
            }
            if (page == N.apps_page) {
                if (draw && !i)
                    ink_ui_line(N.content, TR(groups[g]), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, width());
                for (int k = i; k < i + columns && k < total; k++) {
                    if (draw) tile(N.content, shown[k].id, shown[k].title, shown[k].icon, k % columns, 0, y + (i ? 0 : name_h), true, columns);
                    if (list && list->n >= 0 && (size_t) list->n < list->cap)
                        list->n += snprintf(list->buf + list->n, list->cap - (size_t) list->n, "%s（%s）；",
                                            shown[k].title, shown[k].id);
                }
            }
            y += need;
        }
    }
    return page + 1;
}

static void search_entered(void *ctx, const char *value) {
    if (!current_session((uint32_t) (uintptr_t) ctx)) return;
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    if (!args) return;
    ink_args_init(args);
    ink_args_str(args, "query", value);
    ink_receipt_t rc;
    ink_cap_invoke("search.show", args, INK_SRC_TOUCH, 0, &rc);
    osal_free(args);
}

void draw_home_content(void) {
    if (!N.content) return;
    if (N.cards_timer) { lv_timer_delete(N.cards_timer); N.cards_timer = NULL; }
    for (unsigned i = 0; i < sizeof N.status / sizeof *N.status; i++) {
        if (N.status[i] && lv_obj_get_parent(N.status[i]) == N.home && N.status_time[i] != (N.home_page != 1)) {
            lv_obj_delete(N.status[i]);
            lv_obj_t *bar = ink_shell_statusbar(N.home, N.home_page != 1);
            ink_ui_hint_obj(bar, INK_REFRESH_FAST);
        }
    }
    N.clock = N.date = N.message = NULL;
    N.kid_time_label = NULL;
    lv_obj_clean(N.content);
    char clock[16], date[96];
    ink_shell_time_strings(clock, sizeof clock, date, sizeof date);
    ink_card_t card;
    bool has_book = N.home_page == 1 && bookmark(&card);
    int page_top = INK_GAP_S;
    if (N.home_page == 1) {
        int call_offset = ink_activity_home_offset();
        bool kid_idle = child() && !call_offset;
        if (kid_idle) call_offset -= INK_BLOCK;
        N.date = ink_ui_label(N.content, date, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_BLOCK + call_offset, width());
        lv_obj_set_style_text_align(N.date, LV_TEXT_ALIGN_CENTER, 0);
        N.clock = ink_ui_label(N.content, clock, INK_FONT_CLOCK, INK_WEIGHT_REGULAR, INK_MARGIN,
                               INK_BLOCK + call_offset + ink_ui_line_height(INK_FONT_LIST), width());
        lv_obj_set_style_text_align(N.clock, LV_TEXT_ALIGN_CENTER, 0);
        char text[160];
        snprintf(text, sizeof text, "%s", has_book ? card.text : TR("还没有读到一半的书"));
        if (child() && has_book) snprintf(text, sizeof text, TR("继续读 %s"), card.text);
        int y = INK_BLOCK + call_offset + ink_ui_line_height(INK_FONT_LIST) + ink_ui_line_height(INK_FONT_CLOCK) + INK_GAP_M;
        if (!ink_activity_home_draw(N.content, y)) {
        lv_obj_t *resume = ink_ui_box(N.content, INK_MARGIN + INK_GAP_L, y, width() - 2 * INK_GAP_L, INK_CONTROL, -1,
                                      INK_SURFACE_STATIC, false);
        ink_ui_icon(resume, "book", INK_ICON_MEDIUM, INK_ICON_LINE, INK_GAP_L, INK_GAP_L, false);
        ink_ui_line(resume, text, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP_L + INK_ICON_MEDIUM + INK_GAP_M,
                    (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2,
                    width() - 3 * INK_GAP_L - INK_ICON_MEDIUM - 2 * INK_GAP_M);
        ink_shell_bind(resume, "shell.resume", NULL);
        }
        if (kid_idle) {
            N.kid_time_label = ink_ui_line(N.content, N.kid_time, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN,
                y + INK_CONTROL + INK_GAP_M, width());
            lv_obj_set_style_text_align(N.kid_time_label, LV_TEXT_ALIGN_CENTER, 0);
        }
        int bottom =
            INK_CANVAS_H - INK_STATUS_BAR - INK_DOCK - INK_MARGIN - ink_ui_line_height(INK_FONT_LIST) - INK_GAP_S;
        int grid = bottom - 2 * (INK_CIRCLE_APP + INK_GAP_S + ink_ui_line_height(INK_FONT_NOTE)) - INK_GAP_L;
        bool compact = grid < y + INK_CONTROL + INK_GAP_M + (kid_idle ? ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_M : 0);
        if (compact) grid += 2 * (INK_CIRCLE_APP - INK_CIRCLE);
        static const int kid_order[] = {0, 1, 2, 5, 6, 3, 4};
        int shown = 0;
        for (int i = 0; i < (child() ? 7 : 8); i++) {
            int at = child() ? kid_order[i] : i;
            if (child() && !kid_app_visible(APP_IDS[at])) continue;
            tile(N.content, APP_IDS[at], APP_TITLES[at], APP_ICONS[at], shown % 4, shown / 4, grid, compact, 4);
            shown++;
        }
    } else if (N.home_page == 0) {
        read_cards();
        ink_ui_label(N.content, TR("今日"), INK_FONT_HEADING, INK_WEIGHT_BOLD, INK_MARGIN, page_top, 2 * INK_PRIMARY);
        const char *separator = strstr(date, " · ");
        char short_date[96];
        snprintf(short_date, sizeof short_date, "%.*s", (int) (separator ? separator - date : (ptrdiff_t) strlen(date)),
                 date);
        lv_obj_t *today_date =
            ink_ui_label(N.content, short_date, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_W / 2,
                         page_top + ink_ui_line_height(INK_FONT_HEADING) - ink_ui_line_height(INK_FONT_NOTE),
                         INK_CANVAS_W / 2 - INK_MARGIN);
        lv_obj_set_style_text_align(today_date, LV_TEXT_ALIGN_RIGHT, 0);
        int notice = child() ? ink_ui_line_height(INK_FONT_NOTE) + INK_GAP : 0;
        int first = below_heading() + INK_GAP_XS, h = (home_room() - notice - first - 2 * INK_GAP) / 3;
        int first_h = LV_MAX(h, INK_GAP_M + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S +
                            2 * ink_ui_line_height(INK_FONT_TITLE) + INK_GAP_XS);
        h = (home_room() - notice - first - first_h - 2 * INK_GAP) / 2;
        lv_obj_t *c = ink_ui_card(N.content, INK_MARGIN, first, width(), first_h, INK_SURFACE_BLACK);
        ink_ui_line(c, N.card_fields[0].label, INK_FONT_NOTE, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_GAP_M, width() - 2 * INK_MARGIN);
        lv_obj_t *reading =
            ink_ui_label(c, N.card_fields[0].value, INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN,
                         INK_GAP_M + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S, width() - 2 * INK_MARGIN);
        lv_label_set_long_mode(reading, LV_LABEL_LONG_DOT);
        lv_obj_set_height(reading, ink_ui_px(2 * ink_ui_line_height(INK_FONT_TITLE)));
        bind_card(c, 0);
        for (int i = 0; i < today_slots(); i++) {
            int w = (width() - INK_GAP) / 2;
            int y = first + first_h + INK_GAP + (i / 2) * (h + INK_GAP);
            lv_obj_t *small = ink_ui_card(N.content, INK_MARGIN + (i % 2) * (w + INK_GAP), y, w, h, INK_SURFACE_WHITE);
            ink_ui_icon(small, today_icon(i), INK_ICON_MEDIUM, INK_ICON_LINE, INK_GAP_L, INK_GAP_M, false);
            ink_ui_line(small, N.card_fields[i + 1].label, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_L + INK_ICON_MEDIUM + INK_GAP_S,
                        INK_GAP_M, w - 2 * INK_GAP_L - INK_ICON_MEDIUM - INK_GAP_S);
            lv_obj_t *what = ink_ui_label(small, N.card_fields[i + 1].value, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_GAP_L,
                                          INK_GAP_M + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S, w - 2 * INK_GAP_L);
            lv_label_set_long_mode(what, LV_LABEL_LONG_DOT);
            lv_obj_set_height(what, ink_ui_px(2 * ink_ui_line_height(INK_FONT_LIST)));
            if (child() && !strcmp(N.card_fields[i + 1].id, "time")) N.kid_time_label = what;
            bind_card(small, i + 1);
        }
        if (child()) {
            lv_obj_t *note = ink_ui_line(N.content, TR("爸爸妈妈可以看到你的记录"), INK_FONT_NOTE, INK_WEIGHT_REGULAR,
                INK_MARGIN, home_room() - ink_ui_line_height(INK_FONT_NOTE), width());
            lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
        }
    } else {
        ink_ui_label(N.content, TR("全部"), INK_FONT_HEADING, INK_WEIGHT_BOLD, INK_MARGIN, page_top, width());
        ink_text_cfg_t cfg = {
            .type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEARCH, .max = 64, .hint = TR("查找应用"), .initial = N.search};
        lv_obj_t *search = ink_textfield_create(N.content, &cfg, search_entered, (void *) (uintptr_t) N.epoch, false);
        lv_obj_set_pos(search, ink_ui_px(INK_MARGIN), ink_ui_px(below_heading()));
        ink_textfield_style(search, INK_FIELD_SEARCH, TR("查找应用"));
        N.apps_pages = apps_walk(false, NULL);
        if (N.apps_page >= N.apps_pages) N.apps_page = N.apps_pages - 1;
        apps_walk(true, NULL);
        if (N.apps_pages > 1) {
            char at[32];
            snprintf(at, sizeof at, "%d / %d", N.apps_page + 1, N.apps_pages);
            lv_obj_t *pages =
                ink_ui_line(N.content, at, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_W / 2,
                            page_top + ink_ui_line_height(INK_FONT_HEADING) - ink_ui_line_height(INK_FONT_NOTE),
                            INK_CANVAS_W / 2 - INK_MARGIN);
            lv_obj_set_style_text_align(pages, LV_TEXT_ALIGN_RIGHT, 0);
        }
    }
    if (child() && N.home_page != 2) kid_time_read();
    N.message =
        ink_ui_notice(N.content, N.result, INK_STATUS_BAR + INK_BLOCK_L + 2 * ink_ui_line_height(INK_FONT_CLOCK));
    ink_ui_hint_obj(N.content, INK_REFRESH_TEXT);
    draw_dots();
}

void ink_shell_home_create(lv_obj_t *screen) {
    uint32_t session = __atomic_load_n(&N.session, __ATOMIC_ACQUIRE);
    if (N.epoch != session || strcmp(N.member, ink_member_current()->id) || !ink_member_space_open()) {
        ink_shell_close();
        N.search[0] = N.result[0] = N.picked_ssid[0] = 0;
        N.kid_time[0] = 0;
        memset(N.wifi_ssid, 0, sizeof N.wifi_ssid);
        N.home_page = 1;
        N.apps_page = N.wifi_page = N.wifi_count = 0;
        memset(&N.notify_position, 0, sizeof N.notify_position);
        N.apps_pages = 1;
        N.more = N.scan_busy = false;
        N.control_count = 0;
        N.epoch = session;
        snprintf(N.member, sizeof N.member, "%s", ink_member_current()->id);
    }
    N.home = screen;
    N.home_page = N.initialized ? N.home_page : 1;
    N.initialized = true;
    ink_shell_statusbar(screen, N.home_page != 1);
    int h = home_room();
    N.content = ink_ui_box(screen, 0, INK_STATUS_BAR, INK_CANVAS_W, h, 0, INK_SURFACE_WHITE, false);
    N.dots = ink_ui_box(screen, 0, INK_STATUS_BAR + h, INK_CANVAS_W, INK_GAP_XL, 0,
                        INK_SURFACE_WHITE, false);
    ink_shell_dock(screen);
    draw_home_content();
}

lv_obj_t *ink_shell_dock(lv_obj_t *parent) {
    lv_obj_t *dock = ink_ui_box(parent, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_DOCK, width(), INK_DOCK, -1,
                                INK_SURFACE_STATIC, false);
    static const char *const icons[] = {INK_UI_ICON(phone), INK_UI_ICON(message), INK_UI_ICON(ai), INK_UI_ICON(book),
                                        INK_UI_ICON(memo)};
    static const char *const ids[] = {"phone", "message", "ai", "reader", "memo"};
    int dock_width = ink_ui_width() - 2 * ink_ui_px(INK_MARGIN);
    lv_obj_set_x(dock, ink_ui_px(INK_MARGIN));
    lv_obj_set_width(dock, dock_width);
    int cell = dock_width * INK_CANVAS_W / ink_ui_px(INK_CANVAS_W) / 5;
    for (int i = 0; i < 5; i++) {
        if (child() && !kid_app_visible(ids[i])) continue;
        lv_obj_t *b =
            ink_ui_circle(dock, icons[i], i * cell + (cell - INK_SEGMENT) / 2, (INK_DOCK - INK_SEGMENT) / 2,
                          INK_SEGMENT, INK_ICON_APP, i == 2 ? INK_SURFACE_BLACK : INK_SURFACE_STATIC, NULL, NULL);
        char json[64];
        snprintf(json, sizeof json, "{\"id\":\"%s\"}", ids[i]);
        ink_shell_bind(b, i == 2 ? "ai.panel.listen" : "shell.launch", i == 2 ? NULL : json);
    }
    return dock;
}

void ink_shell_home_destroy(void) {
    icon_close();
    if (N.cards_timer) { lv_timer_delete(N.cards_timer); N.cards_timer = NULL; }
    if (N.note_timer) { lv_timer_delete(N.note_timer); N.note_timer = NULL; N.result[0] = 0; }
    N.home = N.content = N.dots = N.clock = N.date = N.message = NULL;
    N.kid_time_label = NULL;
}

int ink_shell_home_snapshot(char *buf, size_t cap) {
    if (!current_session(N.epoch)) { if (cap) buf[0] = 0; return -1; }
    if (N.home_page == 0) {
        if (cap) buf[0] = 0;
        if (child()) {
            int n = snprintf(buf, cap, "{\"page\":\"KidToday\",\"page_version\":%u,\"notice\":\"爸爸妈妈可以看到你的记录\",\"content\":", (unsigned)ink_ui_content_version());
            if (n < 0 || (size_t)n + 2 >= cap) {
                if (cap) buf[0] = 0;
                return -1;
            }
            int size = N.cards_ok ? ink_ui_semantics_snapshot(&N.today, buf + n, cap - (size_t)n - 1) : -1;
            if (size < 0) { buf[0] = 0; return -1; }
            memcpy(buf + n + size, "}", 2);
            return n + size + 1;
        }
        return N.cards_ok ? ink_ui_semantics_snapshot(&N.today, buf, cap) : -1;
    }
    ink_card_t card;
    bool has = bookmark(&card);
    char activity[512], clock[16], date[96], upcoming[96] = "还没有接下来的事";
    ink_activity_card_snapshot(activity, sizeof activity);
    ink_shell_time_strings(clock, sizeof clock, date, sizeof date);
    int cursor = 0; ink_alarm_t alarm;
    if (ink_alarm_next(&cursor, INK_ALARM, &alarm) == 1)
        snprintf(upcoming, sizeof upcoming, "%s %d:%02d", alarm.title, alarm.hour, alarm.min);
    char page_no[64], page_name[64];
    snprintf(page_no, sizeof page_no, "工具页=%d/%d", N.apps_page + 1, N.apps_pages);
    snprintf(page_name, sizeof page_name, "页面=%s", ink_shell_board());
    static char apps[2048] INK_PSRAM_BSS;
    tile_list_t list = {apps, sizeof apps, 0}; apps[0] = 0;
    if (N.home_page == 2) apps_walk(false, &list);
    ink_ui_field_spec_t fields[] = {
        {.id = "board", .role = "status", .label = "页面", .value = page_name},
        {.id = "member", .role = "text", .label = "当前", .value = ink_member_current()->name},
        {.id = N.home_page == 2 ? "search" : "date", .role = "text", .label = N.home_page == 2 ? "查找" : "日期", .value = N.home_page == 2 ? N.search : date},
        {.id = N.home_page == 2 ? "tools_page" : "clock", .role = "status", .label = N.home_page == 2 ? "工具页" : "时间", .value = N.home_page == 2 ? page_no : clock},
        {.id = N.home_page == 2 ? "tools" : "resume", .role = "text", .label = N.home_page == 2 ? "应用" : "继续读", .value = N.home_page == 2 ? apps : has ? card.text : "没有阅读进度"},
        {.id = "upcoming", .role = "text", .label = "接下来", .value = upcoming},
        {.id = "status", .role = "status", .label = "状态", .value = child() ? N.kid_time : N.result},
        {.id = "listen", .role = "text", .label = "继续听", .value = "还没有在听的内容"},
        {.id = "news", .role = "text", .label = "今日报纸", .value = "今天的还没到"},
        {.id = "words", .role = "text", .label = "单词", .value = "还没有记下的词"},
        {.id = "activity", .role = "group", .label = "正在做的事", .value = activity}
    };
    static const char *const CAPS[] = {"shell.launch", "shell.resume", "home.next", "home.prev", "shell.control", "shell.tasks", "shell.icon_menu", "shell.search"};
    ink_ui_action_spec_t actions[sizeof CAPS / sizeof *CAPS];
    for (size_t i = 0; i < sizeof CAPS / sizeof *CAPS; i++)
        actions[i] = (ink_ui_action_spec_t){.id = CAPS[i], .label = CAPS[i], .capability = CAPS[i], .enabled = N.home_page == 2 ? i != 1 && i != 5 : i < 6};
    ink_ui_page_spec_t page = {.id = "home", .title = ink_shell_board(), .object = {"home", ink_ui_generation()},
        .fields = fields, .field_count = N.home_page == 2 ? 5 : sizeof fields / sizeof *fields, .actions = actions, .action_count = sizeof actions / sizeof *actions};
    return ink_ui_semantics_summary(&page, buf, cap);
}
