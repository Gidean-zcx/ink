/* Three home pages, control centre and the first settings pages. */
/* ui-scale-runtime: dimension width Canvas width minus theme margins, shared by all shell pages. */
/* ui-scale-runtime: dimension notifications Returns the bottom of theme-sized notification rows. */
/* ui-scale-runtime: dimension y tile and notifications accept a theme-derived row position. */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/ai.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/members.h"
#include "ink/sys/json.h"
#include "ink/sys/notify.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/ui_shell.h"

typedef struct { char cap[48], args[192]; uint32_t epoch; } binding_t;
typedef enum { PAGE_SETTINGS, PAGE_DISPLAY, PAGE_SOUND, PAGE_NETWORK, PAGE_NOTIFICATIONS } page_t;
static void create_page(lv_obj_t *screen, void *arg);
static void destroy_page(void);
static int page_snapshot(char *buf, size_t cap);
static bool page_key(ink_key_t key, bool long_press);
static const ink_activity_t PAGES[] = {
    {.name = "Settings", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key},
    {.name = "SetDisplay", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key},
    {.name = "SetSound", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key},
    {.name = "SetNetwork", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key},
    {.name = "Notifications", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key}
};

static struct {
    lv_obj_t *home, *content, *dots, *clock, *date, *message;
    lv_obj_t *control, *page, *page_body;
    lv_obj_t *status[3];
    bool status_time[3];
    int home_page, notify_page, apps_page, apps_pages, wifi_page;
    page_t page_id;
    bool more, initialized, need_body;
    char member[16];
    uint32_t session, epoch;
    char result[160], search[80];
    char wifi_ssid[6][40], picked_ssid[40];
    int wifi_count;
    bool scan_busy, notify_more;
    ink_notify_view_t control_items[2], notify_items[6];
    int control_count, notify_count;
    ink_exec_job_t refresh;
} N INK_PSRAM_BSS;
static void draw_page_body(void);
static void draw_home_content(void);

static bool current_session(uint32_t epoch) {
    return epoch == __atomic_load_n(&N.session, __ATOMIC_ACQUIRE) &&
           !strcmp(N.member, ink_member_current()->id) && ink_member_space_open();
}

static void wifi_receipt(void *ctx, const ink_receipt_t *rc) {
    uintptr_t request = (uintptr_t)ctx;
    if (!current_session((uint32_t)(request >> 1))) return;
    bool scan = (request & 1) != 0;
    N.scan_busy = false;
    snprintf(N.result, sizeof N.result, "%s", rc->say[0] ? rc->say : rc->status == INK_RC_DONE ? "已经找过附近的网络" : "没能完成，可以再试一次");
    if (scan && rc->status == INK_RC_DONE && rc->result) {
        ink_json_t j;
        N.wifi_count = 0;
        N.wifi_page = 0;
        if (ink_json_parse(&j, rc->result, strlen(rc->result)) == 0) for (int i = 1; i < j.ntok && N.wifi_count < 6; i++) {
            if (j.tok[i].type != 1) continue;
            int token = ink_json_find(&j, i, "ssid");
            if (ink_json_str(&j, token, N.wifi_ssid[N.wifi_count], sizeof N.wifi_ssid[0]) >= 0) N.wifi_count++;
            int end = j.tok[i].end;
            while (i + 1 < j.ntok && j.tok[i + 1].start < end) i++;
        }
        ink_ui_content_changed();
    }
    if (N.page_body && N.page_id == PAGE_NETWORK) draw_page_body();
}

static int width(void) { return INK_CANVAS_W - 2 * INK_MARGIN; }
static bool child(void) { return ink_member_current()->role == INK_ROLE_CHILD; }
static int64_t integer(const char *id) { int64_t v = 0; ink_state_get_int(ink_state_find(id), &v); return v; }
static void state_text(const char *id, char *buf, size_t cap) { if (ink_state_get_str(ink_state_find(id), buf, cap) < 0 && cap) buf[0] = 0; }

static void binding_event(lv_event_t *e) {
    binding_t *b = lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_DELETE) { osal_free(b); return; }
    if (!current_session(b->epoch)) return;
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .args_json = b->args[0] ? b->args : NULL, .reply_exec = INK_EXEC_UI,
                        .member = ink_member_current()->id};
    if (!strcmp(b->cap, "wifi.scan")) {
        if (N.scan_busy) return;
        N.scan_busy = true; o.done = wifi_receipt; o.ctx = (void *)(((uintptr_t)b->epoch << 1) | 1);
        ink_ui_busy_control(lv_event_get_target_obj(e), true);
    }
    if (!strcmp(b->cap, "text.voice_start")) ink_ui_cause(osal_now_us(), "text.voice_tap_to_panel_ms");
    ink_cap_call(b->cap, NULL, &o);
}

void ink_shell_bind(lv_obj_t *obj, const char *cap, const char *json) {
    binding_t *b = osal_calloc(1, sizeof *b, OSAL_MEM_EXTERNAL);
    if (!b) return;
    snprintf(b->cap, sizeof b->cap, "%s", cap);
    snprintf(b->args, sizeof b->args, "%s", json ? json : "");
    b->epoch = N.epoch;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj, binding_event, LV_EVENT_CLICKED, b);
    lv_obj_add_event_cb(obj, binding_event, LV_EVENT_DELETE, b);
}

const char *ink_shell_bound_cap(lv_obj_t *obj) {
    for (uint32_t i = 0; i < lv_obj_get_event_count(obj); i++) {
        lv_event_dsc_t *d = lv_obj_get_event_dsc(obj, i);
        if (lv_event_dsc_get_cb(d) == binding_event) return ((binding_t *)lv_event_dsc_get_user_data(d))->cap;
    }
    return NULL;
}

void ink_shell_time_strings(char *clock, size_t cc, char *date, size_t dc) {
    if (!ink_time_valid()) { snprintf(clock, cc, "--:--"); snprintf(date, dc, "时间还没有设置"); return; }
    ink_tm_t tm;
    ink_time_to_local(ink_time_now_s(), &tm);
    static const char *const WEEK[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    snprintf(clock, cc, "%02d:%02d", tm.hour, tm.min);
    ink_lunar_t lunar;
    char lunar_name[40] = "";
    if (ink_lunar_from_solar(tm.year, tm.month, tm.day, &lunar)) ink_lunar_name(&lunar, lunar_name, sizeof lunar_name);
    snprintf(date, dc, "%d月%d日 %s · %s", tm.month, tm.day, WEEK[tm.wday], lunar_name);
}

static void status_deleted(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target_obj(e);
    for (unsigned i = 0; i < sizeof N.status / sizeof *N.status; i++) if (N.status[i] == obj) N.status[i] = NULL;
}

lv_obj_t *ink_shell_statusbar(lv_obj_t *parent, bool show_time) {
    lv_obj_t *bar = ink_ui_box(parent, 0, 0, INK_CANVAS_W, INK_LINE_BODY, 0, INK_SURFACE_WHITE, false);
    ink_shell_bind(bar, "shell.control", NULL);
    char clock[16], date[96];
    ink_shell_time_strings(clock, sizeof clock, date, sizeof date);
    int x = INK_MARGIN;
    if (show_time) { ink_ui_label(bar, clock, INK_FONT_STATUS, INK_WEIGHT_MEDIUM, x, INK_GAP_S, 2 * INK_ICON_MEDIUM); x += 2 * INK_ICON_MEDIUM; }
    char net[32], mic[32];
    state_text("net.type", net, sizeof net);
    state_text("audio.mic", mic, sizeof mic);
    ink_ui_icon(bar, !strcmp(net, "wifi") ? "wifi" : !strcmp(net, "mobile") ? "signal" : "wifioff", INK_ICON_SMALL, INK_ICON_LINE, x, INK_GAP_M, false);
    if (integer("notify.unread")) ink_ui_icon(bar, "bell", INK_ICON_SMALL, INK_ICON_LINE, x + INK_ICON_LIST, INK_GAP_M, false);
    if (!strcmp(mic, "local")) ink_ui_icon(bar, "mic", INK_ICON_SMALL, INK_ICON_LINE, x + 2 * INK_ICON_LIST, INK_GAP_M, false);
    else if (!strcmp(mic, "sending") || !strcmp(mic, "recording")) {
        lv_obj_t *s = ink_ui_box(bar, x + 2 * INK_ICON_LIST, INK_GAP_XS, INK_CONTROL, INK_LINE_LIST, INK_RADIUS_KEY, INK_SURFACE_BLACK, false);
        ink_ui_label(s, !strcmp(mic, "recording") ? "录音" : "在听", INK_FONT_STATUS, INK_WEIGHT_MEDIUM, INK_GAP_S, INK_GAP_XS, INK_BLOCK);
    }
    char battery[12];
    snprintf(battery, sizeof battery, "%lld", (long long)integer("battery.level"));
    ink_ui_label(bar, battery, INK_FONT_STATUS, INK_WEIGHT_MEDIUM, INK_CANVAS_W - INK_MARGIN - INK_PRIMARY, INK_GAP_S, INK_LINE_LIST);
    ink_ui_icon(bar, "battery", INK_ICON_LIST, INK_ICON_LINE, INK_CANVAS_W - INK_MARGIN - INK_ICON_LIST, INK_GAP_XS, false);
    for (unsigned i = 0; i < sizeof N.status / sizeof *N.status; i++) if (!N.status[i]) { N.status[i] = bar; N.status_time[i] = show_time; break; }
    lv_obj_add_event_cb(bar, status_deleted, LV_EVENT_DELETE, NULL);
    return bar;
}

void ink_shell_topbar(lv_obj_t *parent, const char *title) {
    int y = INK_LINE_BODY + INK_GAP_S;
    lv_obj_t *back = ink_ui_circle(parent, "back", INK_MARGIN, y, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_STATIC, NULL, NULL);
    ink_shell_bind(back, "shell.back", NULL);
    ink_ui_label(parent, title, INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN + INK_CONTROL + INK_GAP,
                 y + (INK_CONTROL - INK_LINE_TITLE) / 2, width() - INK_CONTROL - INK_GAP);
}

static bool bookmark(ink_card_t *out) {
    memset(out, 0, sizeof *out);
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *a = ink_app_at(i);
        if (!ink_app_visible(a)) continue;
        for (int j = 0; a->card && a->card(j, out) == 1; j++) if (out->bookmark) return true;
    }
    return false;
}

static void draw_dots(void) {
    lv_obj_clean(N.dots);
    for (int i = 0; i < 3; i++)
        ink_ui_box(N.dots, INK_CANVAS_W / 2 - INK_RADIUS_PANEL + i * INK_RADIUS_PANEL - (i == N.home_page ? INK_GAP_M : 0),
                   INK_GAP, i == N.home_page ? INK_ICON_MEDIUM : INK_GAP_M, INK_GAP_M, -1, i == N.home_page ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, i != N.home_page);
    ink_ui_hint_obj(N.dots, INK_REFRESH_FAST);
}

static bool matches(const char *title, const char *id) { return !N.search[0] || strstr(title, N.search) || strstr(id, N.search); }
static void tile(lv_obj_t *parent, const char *id, const char *title, const char *icon, int col, int row, int y, bool compact) {
    if (N.home_page == 2 && !matches(title, id)) return;
    int cell = width() / 4, d = compact ? INK_CIRCLE : INK_CIRCLE_APP;
    int x = INK_MARGIN + col * cell + (cell - d) / 2;
    int at = y + row * (d + INK_GAP_S + INK_LINE_NOTE + INK_GAP_L);
    lv_obj_t *button = ink_ui_circle(parent, icon, x, at, d, compact ? INK_ICON_LIST : INK_ICON_APP, INK_SURFACE_STATIC, NULL, NULL);
    char json[96];
    snprintf(json, sizeof json, "{\"id\":\"%s\"}", id);
    ink_shell_bind(button, "shell.launch", json);
    lv_obj_t *label = ink_ui_label(parent, title, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + col * cell, at + d + INK_GAP_S, cell);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

static const char *const APP_IDS[] = {"listen", "news", "word", "upcoming", "clock", "camera", "gallery", "settings"};
static const char *const APP_TITLES[] = {"听", "报纸", "单词", "接下来", "时钟", "相机", "相册", "设置"};
static const char *const APP_ICONS[] = {INK_UI_ICON(headphones), INK_UI_ICON(news), INK_UI_ICON(word), INK_UI_ICON(calendar), INK_UI_ICON(clock), INK_UI_ICON(camera), INK_UI_ICON(gallery), INK_UI_ICON(gear)};
static const char *const GROUP_IDS[][4] = {{"reader", "listen", "news", "word"}, {"memo", "upcoming", "phone", "message"}};
static const char *const GROUP_TITLES[][4] = {{"阅读", "听", "报纸", "单词"}, {"备忘", "接下来", "电话", "短信"}};
static const char *const GROUP_ICONS[][4] = {{INK_UI_ICON(book), INK_UI_ICON(headphones), INK_UI_ICON(news), INK_UI_ICON(word)}, {INK_UI_ICON(memo), INK_UI_ICON(calendar), INK_UI_ICON(phone), INK_UI_ICON(message)}};
typedef struct { const char *id, *title, *icon; } tool_t;
static int tools(tool_t out[24]) {
    int count = 0;
    for (int i = 0; i < ink_app_count() && count < 16; i++) {
        const ink_app_def_t *a = ink_app_at(i);
        if (!a->entry || !ink_app_visible(a) || !strcmp(a->id, "home") || !strcmp(a->id, "reader")) continue;
        out[count++] = (tool_t){a->id, a->title, a->icon ? a->icon : INK_UI_ICON(grid)};
    }
    static const tool_t MORE[] = {{"clock", "时钟", INK_UI_ICON(clock)}, {"camera", "相机", INK_UI_ICON(camera)}, {"gallery", "相册", INK_UI_ICON(gallery)},
        {"files", "文件", INK_UI_ICON(folder)}, {"remote", "网页", INK_UI_ICON(globe)}, {"ai", "对话记录", INK_UI_ICON(ai)}, {"settings", "设置", INK_UI_ICON(gear)}};
    for (unsigned i = 0; i < sizeof MORE / sizeof *MORE; i++) {
        if (child() && (i == 3 || i == 4 || i == 6)) continue;
        bool present = false;
        for (int k = 0; k < count; k++) present |= !strcmp(out[k].id, MORE[i].id);
        if (!present) out[count++] = MORE[i];
    }
    return count;
}

static void search_entered(void *ctx, const char *value) {
    if (!current_session((uint32_t)(uintptr_t)ctx)) return;
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    if (!args) return;
    ink_args_init(args); ink_args_str(args, "query", value);
    ink_receipt_t rc; ink_cap_invoke("shell.search", args, INK_SRC_TOUCH, 0, &rc);
    osal_free(args);
}

static void draw_home_content(void) {
    if (!N.content) return;
    for (unsigned i = 0; i < sizeof N.status / sizeof *N.status; i++) {
        if (N.status[i] && lv_obj_get_parent(N.status[i]) == N.home && N.status_time[i] != (N.home_page != 1)) {
            lv_obj_delete(N.status[i]);
            lv_obj_t *bar = ink_shell_statusbar(N.home, N.home_page != 1);
            ink_ui_hint_obj(bar, INK_REFRESH_FAST);
        }
    }
    N.clock = N.date = N.message = NULL;
    lv_obj_clean(N.content);
    char clock[16], date[96];
    ink_shell_time_strings(clock, sizeof clock, date, sizeof date);
    ink_card_t card;
    bool has_book = bookmark(&card);
    int page_top = INK_GAP_S;
    if (N.home_page == 1) {
        N.date = ink_ui_label(N.content, date, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_BLOCK, width());
        lv_obj_set_style_text_align(N.date, LV_TEXT_ALIGN_CENTER, 0);
        N.clock = ink_ui_label(N.content, clock, INK_FONT_CLOCK, INK_WEIGHT_REGULAR, INK_MARGIN, INK_BLOCK + INK_LINE_LIST, width());
        lv_obj_set_style_text_align(N.clock, LV_TEXT_ALIGN_CENTER, 0);
        char text[160];
        snprintf(text, sizeof text, "%s", has_book ? card.text : "还没有读到一半的书");
        int y = INK_BLOCK + INK_LINE_LIST + INK_LINE_CLOCK + INK_GAP_M;
        lv_obj_t *resume = ink_ui_box(N.content, INK_MARGIN + INK_GAP_L, y, width() - 2 * INK_GAP_L, INK_CONTROL, -1, INK_SURFACE_STATIC, false);
        ink_ui_icon(resume, "book", INK_ICON_MEDIUM, INK_ICON_LINE, INK_GAP_L, INK_GAP_L, false);
        ink_ui_label(resume, text, INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_GAP_L + INK_ICON_MEDIUM + INK_GAP_M, (INK_CONTROL - INK_LINE_LIST) / 2, width() - 2 * INK_PRIMARY);
        ink_shell_bind(resume, "shell.resume", NULL);
        int bottom = INK_CANVAS_H - INK_LINE_BODY - INK_DOCK - INK_MARGIN - INK_LINE_LIST - INK_GAP_S;
        int grid = bottom - 2 * (INK_CIRCLE_APP + INK_GAP_S + INK_LINE_NOTE) - INK_GAP_L;
        for (int i = 0; i < 8; i++) if (!(child() && i == 7)) tile(N.content, APP_IDS[i], APP_TITLES[i], APP_ICONS[i], i % 4, i / 4, grid, false);
    } else if (N.home_page == 0) {
        ink_ui_label(N.content, "今日", INK_FONT_HEADING, INK_WEIGHT_BOLD, INK_MARGIN, page_top, 2 * INK_PRIMARY);
        const char *separator = strstr(date, " · ");
        char short_date[96]; snprintf(short_date, sizeof short_date, "%.*s", (int)(separator ? separator - date : (ptrdiff_t)strlen(date)), date);
        lv_obj_t *today_date = ink_ui_label(N.content, short_date, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_W / 2,
                                           page_top + INK_LINE_HEADING - INK_LINE_NOTE, INK_CANVAS_W / 2 - INK_MARGIN);
        lv_obj_set_style_text_align(today_date, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_t *c = ink_ui_card(N.content, INK_MARGIN, page_top + INK_PRIMARY, width(), 2 * INK_ROW_DOUBLE + INK_GAP_S, INK_SURFACE_BLACK);
        ink_ui_label(c, "继续读", INK_FONT_NOTE, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_MARGIN, width() - 2 * INK_MARGIN);
        ink_ui_label(c, has_book ? card.text : "还没有开始读书", INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_MARGIN + INK_LINE_NOTE + INK_GAP_S, width() - 2 * INK_MARGIN);
        ink_shell_bind(c, "shell.resume", NULL);
        const char *const empty[] = {"还没有在听的内容", "今天的还没到", "还没有记下的词", "还没有接下来的事"};
        const char *const titles[] = {"继续听", "今日报纸", "单词", "接下来"};
        for (int i = 0; i < 4; i++) {
            int w = (width() - INK_GAP) / 2, h = 2 * INK_ROW_DOUBLE + INK_GAP_S;
            int y = page_top + INK_PRIMARY + h + INK_GAP + (i / 2) * (h + INK_GAP);
            lv_obj_t *small = ink_ui_card(N.content, INK_MARGIN + (i % 2) * (w + INK_GAP), y, w, h, INK_SURFACE_WHITE);
            ink_ui_icon(small, APP_ICONS[i], INK_ICON_MEDIUM, INK_ICON_LINE, INK_GAP_L, INK_GAP_L, false);
            ink_ui_label(small, titles[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_L + INK_ICON_MEDIUM + INK_GAP_S, INK_GAP_L, w - INK_PRIMARY);
            const char *text = empty[i];
            char next[96];
            if (i == 3) {
                int cursor = 0; ink_alarm_t alarm;
                if (ink_alarm_next(&cursor, INK_ALARM, &alarm) == 1) {
                    snprintf(next, sizeof next, "%s %d:%02d", alarm.title, alarm.hour, alarm.min); text = next;
                }
            }
            ink_ui_label(small, text, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_GAP_L, INK_GAP_L + INK_LINE_NOTE + INK_GAP_M, w - 2 * INK_GAP_L);
            char json[64]; snprintf(json, sizeof json, "{\"id\":\"%s\"}", APP_IDS[i]); ink_shell_bind(small, "shell.launch", json);
        }
    } else {
        ink_ui_label(N.content, "全部", INK_FONT_HEADING, INK_WEIGHT_BOLD, INK_MARGIN, page_top, width());
        ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_SEARCH, .max = 64, .hint = "查找应用", .initial = N.search};
        lv_obj_t *search = ink_textfield_create(N.content, &cfg, search_entered, (void *)(uintptr_t)N.epoch, false);
        lv_obj_set_pos(search, ink_ui_px(INK_MARGIN), ink_ui_px(INK_PRIMARY));
        lv_obj_set_size(search, ink_ui_px(width()), ink_ui_px(INK_CONTROL));
        static const char *const groups[] = {"读和听", "记和联系", "工具"};
        int y = INK_PRIMARY + INK_CONTROL + INK_GAP_L;
        for (int g = 0; g < 3; g++) {
            ink_ui_label(N.content, groups[g], INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y, width());
            if (g < 2) {
                for (int i = 0; i < 4; i++) tile(N.content, GROUP_IDS[g][i], GROUP_TITLES[g][i], GROUP_ICONS[g][i], i, 0, y + INK_LINE_NOTE + INK_GAP_S, true);
            } else {
                tool_t items[24]; int count = tools(items);
                N.apps_pages = (count + 7) / 8;
                for (int i = N.apps_page * 8; i < count && i < (N.apps_page + 1) * 8; i++)
                    tile(N.content, items[i].id, items[i].title, items[i].icon, (i % 8) % 4, (i % 8) / 4, y + INK_LINE_NOTE + INK_GAP_S, true);
            }
            y += INK_LINE_NOTE + INK_GAP_S + INK_CIRCLE + INK_GAP_S + INK_LINE_NOTE + INK_GAP_L;
        }
    }
    N.message = ink_ui_notice(N.content, N.result, INK_LINE_BODY + INK_BLOCK_L + 2 * INK_LINE_CLOCK);
    ink_ui_hint_obj(N.content, INK_REFRESH_TEXT);
    draw_dots();
}

void ink_shell_home_create(lv_obj_t *screen) {
    uint32_t session = __atomic_load_n(&N.session, __ATOMIC_ACQUIRE);
    if (N.epoch != session || strcmp(N.member, ink_member_current()->id) || !ink_member_space_open()) {
        ink_shell_close();
        N.search[0] = N.result[0] = N.picked_ssid[0] = 0;
        memset(N.wifi_ssid, 0, sizeof N.wifi_ssid);
        N.home_page = 1; N.notify_page = N.apps_page = N.wifi_page = N.wifi_count = 0;
        N.apps_pages = 1; N.more = N.scan_busy = N.notify_more = false;
        N.control_count = N.notify_count = 0;
        N.epoch = session;
        snprintf(N.member, sizeof N.member, "%s", ink_member_current()->id);
    }
    N.home = screen;
    N.home_page = N.initialized ? N.home_page : 1;
    N.initialized = true;
    ink_shell_statusbar(screen, N.home_page != 1);
    int h = INK_CANVAS_H - INK_LINE_BODY - INK_DOCK - INK_MARGIN - INK_LINE_LIST;
    N.content = ink_ui_box(screen, 0, INK_LINE_BODY, INK_CANVAS_W, h, 0, INK_SURFACE_WHITE, false);
    N.dots = ink_ui_box(screen, 0, INK_LINE_BODY + h, INK_CANVAS_W, INK_LINE_LIST, 0, INK_SURFACE_WHITE, false);
    ink_shell_dock(screen);
    draw_home_content();
}

lv_obj_t *ink_shell_dock(lv_obj_t *parent) {
    lv_obj_t *dock = ink_ui_box(parent, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_DOCK, width(), INK_DOCK, -1, INK_SURFACE_STATIC, false);
    static const char *const icons[] = {INK_UI_ICON(phone), INK_UI_ICON(message), INK_UI_ICON(ai), INK_UI_ICON(book), INK_UI_ICON(memo)};
    static const char *const ids[] = {"phone", "message", "ai", "reader", "memo"};
    int dock_width = ink_ui_width() - 2 * ink_ui_px(INK_MARGIN);
    lv_obj_set_x(dock, ink_ui_px(INK_MARGIN));
    lv_obj_set_width(dock, dock_width);
    int cell = dock_width * INK_CANVAS_W / ink_ui_px(INK_CANVAS_W) / 5;
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = ink_ui_circle(dock, icons[i], i * cell + (cell - INK_PRIMARY) / 2, (INK_DOCK - INK_PRIMARY) / 2,
                                    INK_PRIMARY, INK_ICON_APP, i == 2 ? INK_SURFACE_BLACK : INK_SURFACE_STATIC, NULL, NULL);
        char json[64]; snprintf(json, sizeof json, "{\"id\":\"%s\"}", ids[i]); ink_shell_bind(b, "shell.launch", json);
    }
    return dock;
}

void ink_shell_home_destroy(void) { N.home = N.content = N.dots = N.clock = N.date = N.message = NULL; }

static void turn(int delta) {
    if (N.home_page == 2 && ((delta > 0 && N.apps_page + 1 < N.apps_pages) || (delta < 0 && N.apps_page > 0))) {
        N.apps_page += delta;
        ink_ui_content_changed(); draw_home_content(); return;
    }
    int next = N.home_page + delta;
    if (!N.content || next < 0 || next > 2) return;
    N.home_page = next;
    ink_ui_stage_next();
    ink_ui_content_changed();
    draw_home_content();
    ink_probe_count("home.page_turns");
}
bool ink_shell_home_swipe(ink_swipe_dir_t dir) {
    if (dir == INK_SWIPE_LEFT) turn(1);
    else if (dir == INK_SWIPE_RIGHT) turn(-1);
    else return false;
    return true;
}
bool ink_shell_home_key(ink_key_t key, bool long_press) {
    if (long_press) return false;
    if (key == INK_KEY_NEXT) turn(1);
    else if (key == INK_KEY_PREV) turn(-1);
    else return false;
    return true;
}

int ink_shell_home_snapshot(char *buf, size_t cap) {
    if (!current_session(N.epoch)) return snprintf(buf, cap, "空间锁定或正在切换；AI 无权读取。");
    ink_card_t card;
    bool has = bookmark(&card);
    if (N.home_page == 2) {
        tool_t items[24]; int count = tools(items);
        int n = snprintf(buf, cap, "Apps；当前=%s；查找=%s；工具页=%d/%d；", ink_member_current()->name, N.search, N.apps_page + 1, N.apps_pages);
        for (int g = 0; g < 2; g++) for (int i = 0; i < 4 && n >= 0 && (size_t)n < cap; i++)
            if (matches(GROUP_TITLES[g][i], GROUP_IDS[g][i])) n += snprintf(buf + n, cap - (size_t)n, "%s（%s）；", GROUP_TITLES[g][i], GROUP_IDS[g][i]);
        for (int i = N.apps_page * 8; i < count && i < (N.apps_page + 1) * 8 && n >= 0 && (size_t)n < cap; i++)
            if (matches(items[i].title, items[i].id)) n += snprintf(buf + n, cap - (size_t)n, "%s（%s）；", items[i].title, items[i].id);
        if (n >= 0 && (size_t)n < cap) n += snprintf(buf + n, cap - (size_t)n, "动作：shell.launch、shell.search、home.next、home.prev、shell.control。");
        return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
    }
    char clock[16], date[96], upcoming[96] = "还没有接下来的事";
    ink_shell_time_strings(clock, sizeof clock, date, sizeof date);
    int cursor = 0; ink_alarm_t alarm;
    if (ink_alarm_next(&cursor, INK_ALARM, &alarm) == 1) snprintf(upcoming, sizeof upcoming, "%s %d:%02d", alarm.title, alarm.hour, alarm.min);
    return snprintf(buf, cap, "页面=%s；当前=%s；日期=%s；时间=%s；继续读=%s；继续听=还没有在听的内容；今日报纸=今天的还没到；单词=还没有记下的词；接下来=%s；%s。动作：shell.launch、shell.resume、home.next、home.prev、shell.control。",
                    N.home_page == 0 ? "Today" : "Main", ink_member_current()->name, date, clock, has ? card.text : "没有阅读进度", upcoming, N.result);
}

static int control_snapshot(char *buf, size_t cap) {
    if (!current_session(N.epoch)) return snprintf(buf, cap, "空间锁定或正在切换；AI 无权读取。");
    char ssid[64]; state_text("wifi.ssid", ssid, sizeof ssid);
    int n = snprintf(buf, cap, "Control；当前=%s；亮度=%lld；音量=%lld；勿扰=%lld；叫名字=%lld；更多=%d；通知=%d；网络=%s；",
                     ink_member_current()->name, (long long)ink_setting_get("display.light"), (long long)ink_setting_get("audio.volume"),
                     (long long)ink_setting_get("notify.dnd"), (long long)ink_setting_get("voice.handsfree"), N.more, ink_notify_unread(), ssid[0] ? ssid : "还没有连接 Wi-Fi");
    if (N.more && n >= 0 && (size_t)n < cap) n += snprintf(buf + n, cap - (size_t)n, "暖光=%lld；", (long long)ink_setting_get("display.warmth"));
    for (int i = 0; i < N.control_count && n >= 0 && (size_t)n < cap; i++)
        n += snprintf(buf + n, cap - (size_t)n, "通知[%u]=%s，%s；已读=%d；", (unsigned)N.control_items[i].id, N.control_items[i].title, N.control_items[i].body, N.control_items[i].read);
    const char *actions = child() ? "动作：settings.set、system.set_volume、notify.read、shell.close。" : "动作：settings.set、system.set_volume、notify.read、shell.members、shell.settings、shell.network、shell.more、shell.close。";
    if (n >= 0 && (size_t)n < cap) n += snprintf(buf + n, cap - (size_t)n, "%s", actions);
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}
static bool control_key(ink_key_t key, bool lng) { if (!lng && (key == INK_KEY_HOME || key == INK_KEY_BACK)) ink_shell_close(); return true; }
static const ink_ui_overlay_t CONTROL = {.name = "Control", .level = 0, .on_key = control_key, .snapshot = control_snapshot};

static void slider_changed(lv_event_t *e) {
    const char *id = lv_event_get_user_data(e);
    lv_obj_t *o = lv_event_get_target_obj(e);
    lv_indev_t *in = lv_event_get_indev(e);
    if (!in) return;
    lv_point_t pt; lv_indev_get_point(in, &pt);
    lv_area_t a; lv_obj_get_coords(o, &a);
    int span = a.x2 - a.x1;
    int v = span > 0 ? ((pt.x - a.x1) * 10 + span / 2) / span : 0;
    if (v < 0) v = 0;
    if (v > 10) v = 10;
    char json[128];
    snprintf(json, sizeof json, "{\"id\":\"%s\",\"value\":%d}", id, v * 10);
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .args_json = json, .reply_exec = INK_EXEC_UI};
    ink_cap_call("settings.set", NULL, &opts);
}

static void bind_toggle(lv_obj_t *o, const char *id) {
    char json[128]; snprintf(json, sizeof json, "{\"id\":\"%s\",\"value\":%lld}", id, (long long)!ink_setting_get(id));
    ink_shell_bind(o, "settings.set", json);
}

static int notifications(lv_obj_t *parent, int y, int limit) {
    ink_db_cursor_t cursor = {0};
    ink_notify_view_t n;
    int count = 0, skipped = 0;
    ink_notify_view_t *visible = parent == N.page_body ? N.notify_items : N.control_items;
    while (count < limit && ink_notify_next(&cursor, &n) == 1) {
        if (skipped++ < (parent == N.page_body ? N.notify_page * limit : 0)) continue;
        visible[count] = n;
        lv_obj_t *card = ink_ui_card(parent, INK_MARGIN, y, width(), INK_ROW_DOUBLE, INK_SURFACE_WHITE);
        ink_ui_circle(card, !strcmp(n.channel, "message") ? "message" : "bell", INK_GAP, INK_GAP_M, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL);
        ink_ui_label(card, n.title, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_ROW_DOUBLE, INK_GAP, width() - INK_ROW_DOUBLE - INK_MARGIN);
        lv_obj_t *body = ink_ui_label(card, n.body, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_ROW_DOUBLE, INK_GAP + INK_LINE_LIST, width() - INK_ROW_DOUBLE - INK_MARGIN);
        lv_label_set_long_mode(body, LV_LABEL_LONG_DOT); lv_obj_set_height(body, ink_ui_px(INK_LINE_NOTE));
        char args[64]; snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)n.id); ink_shell_bind(card, "notify.read", args);
        count++; y += INK_ROW_DOUBLE + INK_GAP_M;
    }
    if (parent == N.page_body) N.notify_more = ink_notify_next(&cursor, &n) == 1;
    if (!count && parent == N.page_body && N.notify_page) { N.notify_page = 0; return notifications(parent, y, limit); }
    if (!count) ink_ui_notice(parent, "还没有通知", y + INK_GAP_L);
    if (parent == N.page_body) N.notify_count = count; else N.control_count = count;
    return y;
}

static void draw_control(void) {
    lv_obj_clean(N.control);
    ink_shell_statusbar(N.control, true);
    int y = INK_LINE_BODY + INK_GAP_S;
    lv_obj_t *avatar = ink_ui_circle(N.control, "person", INK_MARGIN, y, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_BLACK, NULL, NULL);
    ink_shell_bind(avatar, "shell.members", NULL);
    ink_ui_label(N.control, ink_member_current()->name, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_MARGIN + INK_CIRCLE + INK_GAP, y, width() - 2 * INK_CIRCLE);
    ink_ui_label(N.control, "换人", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + INK_CIRCLE + INK_GAP, y + INK_LINE_LIST, width() - 2 * INK_CIRCLE);
    if (!child()) { lv_obj_t *s = ink_ui_circle(N.control, "gear", INK_CANVAS_W - INK_MARGIN - INK_CIRCLE, y, INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_WHITE, NULL, NULL); ink_shell_bind(s, "shell.settings", NULL); }
    y += INK_CIRCLE + INK_GAP_L;
    ink_ui_slider(N.control, "亮度", (int)ink_setting_get("display.light") / 10, INK_MARGIN, y, width(), slider_changed, (void *)"display.light"); y += INK_SLIDER + INK_GAP_M;
    ink_ui_slider(N.control, "音量", (int)ink_setting_get("audio.volume") / 10, INK_MARGIN, y, width(), slider_changed, (void *)"audio.volume"); y += INK_SLIDER + INK_GAP_M;
    if (!child()) {
        const char *const labels[] = {"勿扰", "叫名字", N.more ? "收起" : "更多"};
        int w = (width() - 2 * INK_GAP_M) / 3;
        for (int i = 0; i < 3; i++) {
            lv_obj_t *p = ink_ui_pill(N.control, labels[i], INK_MARGIN + i * (w + INK_GAP_M), y, w, INK_CONTROL,
                                      i == 0 ? ink_setting_get("notify.dnd") : i == 1 ? ink_setting_get("voice.handsfree") != 2 : false, NULL, NULL);
            if (i == 0) bind_toggle(p, "notify.dnd");
            else if (i == 1) ink_shell_bind(p, "settings.set", ink_setting_get("voice.handsfree") == 2 ? "{\"id\":\"voice.handsfree\",\"value\":0}" : "{\"id\":\"voice.handsfree\",\"value\":2}");
            else ink_shell_bind(p, "shell.more", NULL);
        }
        y += INK_CONTROL + INK_GAP_M;
        if (N.more) { ink_ui_slider(N.control, "暖光", (int)ink_setting_get("display.warmth") / 10, INK_MARGIN, y, width(), slider_changed, (void *)"display.warmth"); y += INK_SLIDER + INK_GAP_M; }
        char ssid[64]; state_text("wifi.ssid", ssid, sizeof ssid);
        lv_obj_t *net = ink_ui_pill(N.control, ssid[0] ? ssid : "还没有连接 Wi-Fi", INK_MARGIN, y, width(), INK_CONTROL, false, NULL, NULL);
        ink_shell_bind(net, "shell.network", NULL); y += INK_CONTROL + INK_GAP_L;
    }
    notifications(N.control, y, 2);
    lv_obj_t *close = ink_ui_pill(N.control, "收起", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_CANVAS_H - INK_CONTROL - INK_MARGIN, INK_CIRCLE_LARGE, INK_CONTROL, false, NULL, NULL);
    ink_shell_bind(close, "shell.close", NULL);
    ink_ui_hint_obj(N.control, INK_REFRESH_FLASH_UI);
}

void ink_shell_control(void) {
    if (!ink_member_space_open() || N.control) return;
    N.control = ink_ui_box(lv_layer_sys(), 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
    lv_obj_add_flag(N.control, LV_OBJ_FLAG_CLICKABLE);
    ink_ui_overlay_add(&CONTROL, N.control);
    draw_control();
}
bool ink_shell_close(void) {
    if (!N.control) return false;
    ink_ui_overlay_remove(&CONTROL);
    lv_obj_delete(N.control); N.control = NULL;
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
    return true;
}

static void group_row(lv_obj_t *group, const char *icon, const char *title, const char *value, int row, const char *action) {
    lv_obj_t *r = ink_ui_list_row(group, icon, title, value, 0, row * INK_CONTROL, width(), false, NULL, NULL);
    if (action) {
        ink_shell_bind(r, action, NULL);
        ink_ui_icon(r, "next", INK_ICON_SMALL, INK_ICON_LINE, width() - INK_READING_MARGIN, (INK_CONTROL - INK_ICON_SMALL) / 2, false);
    }
}

static void wifi_password(void *ctx, const char *password) {
    if (!current_session((uint32_t)(uintptr_t)ctx)) return;
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    if (!args) return;
    ink_args_init(args); ink_args_str(args, "ssid", N.picked_ssid); ink_args_str(args, "password", password);
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .reply_exec = INK_EXEC_UI, .done = wifi_receipt, .ctx = (void *)((uintptr_t)N.epoch << 1), .member = ink_member_current()->id};
    ink_cap_call("wifi.connect", args, &o);
    memset(args, 0, sizeof *args); osal_free(args);
    N.picked_ssid[0] = 0; ink_text_blur(0); draw_page_body();
}
static void wifi_picked(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    char args[40]; snprintf(args, sizeof args, "{\"index\":%d}", index);
    ink_receipt_t rc; ink_cap_invoke_json("shell.wifi_pick", args, INK_SRC_TOUCH, 0, &rc);
}

/* Settings' first row: who is using the device, and the way to the family. */
static void draw_who(int y) {
    const ink_member_t *me = ink_member_current();
    lv_obj_t *who = ink_ui_pill(N.page_body, "", INK_MARGIN, y, width(), INK_ROW_DOUBLE, true, NULL, NULL);
    ink_shell_bind(who, "shell.members", NULL);
    char first[8] = {0}, family[32];
    size_t len = 1;
    while (len < sizeof first - 1 && (me->name[len] & 0xc0) == 0x80) len++;
    memcpy(first, me->name, len);
    lv_obj_t *avatar = ink_ui_pill(who, first, INK_GAP, (INK_ROW_DOUBLE - INK_CIRCLE) / 2, INK_CIRCLE, INK_CIRCLE, false, NULL, NULL);
    lv_obj_remove_flag(avatar, LV_OBJ_FLAG_CLICKABLE);
    int people = 0, x = INK_CIRCLE + 2 * INK_GAP;
    while (ink_member_at(people)) people++;
    snprintf(family, sizeof family, "家里 %d 个人", people);
    lv_obj_t *l = ink_ui_label(who, me->name, INK_FONT_LIST, INK_WEIGHT_BOLD, x, (INK_ROW_DOUBLE - INK_LINE_LIST - INK_LINE_NOTE) / 2, width() - x - INK_READING_MARGIN);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    l = ink_ui_label(who, family, INK_FONT_NOTE, INK_WEIGHT_REGULAR, x, (INK_ROW_DOUBLE - INK_LINE_LIST - INK_LINE_NOTE) / 2 + INK_LINE_LIST, width() - x - INK_READING_MARGIN);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    ink_ui_icon(who, "next", INK_ICON_SMALL, INK_ICON_LINE, width() - INK_READING_MARGIN, (INK_ROW_DOUBLE - INK_ICON_SMALL) / 2, true);
}

static void draw_page_body(void) {
    lv_obj_clean(N.page_body);
    int y = 0;
    if (N.page_id == PAGE_SETTINGS) {
        draw_who(y); y += INK_ROW_DOUBLE + INK_GAP;
        char ssid[64], light[32], volume[32];
        state_text("wifi.ssid", ssid, sizeof ssid);
        snprintf(light, sizeof light, "亮度 %lld", (long long)ink_setting_get("display.light") / 10);
        snprintf(volume, sizeof volume, "音量 %lld", (long long)ink_setting_get("audio.volume") / 10);
        lv_obj_t *g = ink_ui_card(N.page_body, INK_MARGIN, y, width(), 2 * INK_CONTROL, INK_SURFACE_STATIC);
        group_row(g, "wifi", "网络和 SIM 卡", ssid[0] ? ssid : "还没有连接", 0, "shell.network");
        group_row(g, "bluetooth", "蓝牙", integer("bt.connected") ? "已连接" : "没有连接", 1, NULL); y += 2 * INK_CONTROL + INK_GAP;
        g = ink_ui_card(N.page_body, INK_MARGIN, y, width(), 3 * INK_CONTROL, INK_SURFACE_STATIC);
        group_row(g, "sun", "显示和亮度", light, 0, "shell.display");
        group_row(g, "volume", "声音", volume, 1, "shell.sound");
        int wake = (int)ink_setting_get("voice.handsfree");
        group_row(g, "wave", "语音和 AI", wake == 2 ? "叫名字已关闭" : wake == 1 ? "一直能叫名字" : "使用时能叫名字", 2, NULL); y += 3 * INK_CONTROL + INK_GAP;
        g = ink_ui_card(N.page_body, INK_MARGIN, y, width(), 3 * INK_CONTROL, INK_SURFACE_STATIC);
        group_row(g, "lock", "锁屏和壁纸", ink_setting_get("display.lock_style") ? "时钟和待办" : "这一页", 0, "shell.display");
        group_row(g, "storage", "存储和备份", "还没有备份", 1, NULL);
        group_row(g, "info", "关于和更新", "", 2, NULL);
    } else if (N.page_id == PAGE_DISPLAY) {
        ink_ui_slider(N.page_body, "亮度", (int)ink_setting_get("display.light") / 10, INK_MARGIN, y, width(), slider_changed, (void *)"display.light"); y += INK_SLIDER + INK_GAP_M;
        ink_ui_slider(N.page_body, "暖光", (int)ink_setting_get("display.warmth") / 10, INK_MARGIN, y, width(), slider_changed, (void *)"display.warmth"); y += INK_SLIDER + INK_GAP_L;
        ink_ui_label(N.page_body, "字号", INK_FONT_LIST, INK_WEIGHT_BOLD, INK_MARGIN, y, width()); y += INK_LINE_LIST + INK_GAP_M;
        const char *const labels[] = {"小", "中", "大", "特大"};
        lv_obj_t *seg = ink_ui_segments(N.page_body, labels, 4, (int)ink_setting_get("display.font_size"), INK_MARGIN, y, width(), NULL, NULL);
        for (int i = 0; i < 4; i++) { char args[96]; snprintf(args, sizeof args, "{\"id\":\"display.font_size\",\"value\":%d}", i); ink_shell_bind(lv_obj_get_child(seg, i), "settings.set", args); }
        y += INK_SEGMENT + INK_GAP;
        lv_obj_t *sample = ink_ui_card(N.page_body, INK_MARGIN, y, width(), INK_DIGIT + 2 * INK_MARGIN, INK_SURFACE_STATIC);
        static const int px[] = {INK_FONT_LIST, INK_FONT_BODY, INK_FONT_SUBTITLE, INK_FONT_TITLE};
        ink_ui_label(sample, "天黑以后，阿木背起邮包。", px[ink_setting_get("display.font_size")], INK_WEIGHT_REGULAR, INK_MARGIN, INK_GAP_L, width() - 2 * INK_MARGIN);
        y += INK_DIGIT + 2 * INK_MARGIN + INK_GAP_L;
        lv_obj_t *refresh = ink_ui_pill(N.page_body, "刷新屏幕", INK_MARGIN, y, width(), INK_CONTROL, false, NULL, NULL); ink_shell_bind(refresh, "system.full_refresh", NULL);
        y += INK_CONTROL + INK_GAP;
        static const int seconds[] = {60, 300, 600, 1800, 3600};
        int64_t current = ink_setting_get("power.lock_after_s");
        int next = seconds[0];
        for (unsigned i = 0; i < sizeof seconds / sizeof *seconds; i++) if (seconds[i] > current) { next = seconds[i]; break; }
        char label[64], args[96];
        snprintf(label, sizeof label, "自动锁屏 %lld 分钟", (long long)current / 60);
        snprintf(args, sizeof args, "{\"id\":\"power.lock_after_s\",\"value\":%d}", next);
        lv_obj_t *lock = ink_ui_pill(N.page_body, label, INK_MARGIN, y, width(), INK_CONTROL, false, NULL, NULL); ink_shell_bind(lock, "settings.set", args);
    } else if (N.page_id == PAGE_SOUND) {
        ink_ui_slider(N.page_body, "音量", (int)ink_setting_get("audio.volume") / 10, INK_MARGIN, y, width(), slider_changed, (void *)"audio.volume"); y += INK_SLIDER + INK_GAP_S;
        ink_ui_notice(N.page_body, "听书、AI 和提示音用这个音量", y); y += INK_LINE_NOTE + INK_GAP_L;
        ink_ui_label(N.page_body, "声音从哪里出来", INK_FONT_LIST, INK_WEIGHT_BOLD, INK_MARGIN, y, width()); y += INK_LINE_LIST + INK_GAP_M;
        lv_obj_t *g = ink_ui_card(N.page_body, INK_MARGIN, y, width(), 2 * INK_CONTROL, INK_SURFACE_STATIC);
        char route[32]; state_text("audio.route", route, sizeof route);
        group_row(g, "volume", "喇叭", !strcmp(route, "speaker") ? "当前" : "", 0, "bt.disconnect"); group_row(g, "headphones", "蓝牙耳机", !strcmp(route, "bt_headset") ? "当前" : "没有连接", 1, NULL); y += 2 * INK_CONTROL + INK_GAP_L;
        lv_obj_t *toggle = ink_ui_toggle(N.page_body, "勿扰", ink_setting_get("notify.dnd"), INK_MARGIN, y, width(), NULL, NULL); bind_toggle(toggle, "notify.dnd");
        y += INK_CONTROL + INK_GAP_L;
        ink_ui_label(N.page_body, "提示", INK_FONT_LIST, INK_WEIGHT_BOLD, INK_MARGIN, y, width()); y += INK_LINE_LIST + INK_GAP_M;
        g = ink_ui_card(N.page_body, INK_MARGIN, y, width(), 2 * INK_CONTROL, INK_SURFACE_STATIC);
        group_row(g, "bell", "按键提示音", "还没有可选设置", 0, NULL); group_row(g, "wave", "振动", "还没有可选设置", 1, NULL); y += 2 * INK_CONTROL + INK_GAP_L;
        ink_ui_label(N.page_body, "铃声", INK_FONT_LIST, INK_WEIGHT_BOLD, INK_MARGIN, y, width()); y += INK_LINE_LIST + INK_GAP_M;
        g = ink_ui_card(N.page_body, INK_MARGIN, y, width(), 2 * INK_CONTROL, INK_SURFACE_STATIC);
        group_row(g, "phone", "来电", "系统提示音", 0, NULL); group_row(g, "clock", "闹钟", "系统提示音", 1, NULL);
    } else if (N.page_id == PAGE_NETWORK) {
        ink_ui_label(N.page_body, "上网方式", INK_FONT_LIST, INK_WEIGHT_BOLD, INK_MARGIN, y, width()); y += INK_LINE_LIST + INK_GAP_M;
        const char *const labels[] = {"自动", "Wi-Fi", "4G"};
        int count = (int)(sizeof labels / sizeof *labels);
        lv_obj_t *seg = ink_ui_segments(N.page_body, labels, count, (int)ink_setting_get("net.prefer"), INK_MARGIN, y, width(), NULL, NULL);
        for (int i = 0; i < 3; i++) { char args[96]; snprintf(args, sizeof args, "{\"id\":\"net.prefer\",\"value\":%d}", i); ink_shell_bind(lv_obj_get_child(seg, i), "settings.set", args); }
        y += INK_SEGMENT + INK_GAP_M;
        ink_ui_notice(N.page_body, "自动：有 Wi-Fi 用 Wi-Fi，没有就用 4G", y); y += INK_LINE_NOTE + INK_GAP_L;
        ink_ui_label(N.page_body, "Wi-Fi", INK_FONT_LIST, INK_WEIGHT_BOLD, INK_MARGIN, y, width()); y += INK_LINE_LIST + INK_GAP_M;
        char ssid[64]; state_text("wifi.ssid", ssid, sizeof ssid);
        ink_ui_notice(N.page_body, ssid[0] ? ssid : "还没有连接 Wi-Fi", y); y += INK_LINE_NOTE + INK_GAP_L;
        if (N.picked_ssid[0]) {
            ink_text_cfg_t cfg = {.type = INK_TEXT_PASSWORD, .enter = INK_ENTER_DONE, .max = 64, .hint = "输入网络密码", .ai_hidden = true};
            lv_obj_t *password_field = ink_textfield_create(N.page_body, &cfg, wifi_password, (void *)(uintptr_t)N.epoch, true);
            lv_obj_set_pos(password_field, ink_ui_px(INK_MARGIN), ink_ui_px(y));
            return;
        }
        lv_obj_t *scan = ink_ui_pill(N.page_body, N.scan_busy ? "正在找附近的 Wi-Fi" : "找附近的 Wi-Fi", INK_MARGIN, y, width(), INK_CONTROL, N.scan_busy, NULL, NULL); ink_shell_bind(scan, "wifi.scan", NULL);
        y += INK_CONTROL + INK_GAP_M;
        for (int i = N.wifi_page * 2; i < N.wifi_count && i < (N.wifi_page + 1) * 2; i++) {
            ink_ui_pill(N.page_body, N.wifi_ssid[i], INK_MARGIN, y, width(), INK_CONTROL, false, wifi_picked, (void *)(intptr_t)i);
            y += INK_CONTROL + INK_GAP_S;
        }
        if (N.wifi_count > 2) {
            lv_obj_t *prev = ink_ui_pill(N.page_body, "上一组", INK_MARGIN, y, (width() - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL);
            if (N.wifi_page) ink_shell_bind(prev, "shell.wifi_prev", NULL);
            lv_obj_t *next = ink_ui_pill(N.page_body, "下一组", INK_MARGIN + (width() + INK_GAP) / 2, y, (width() - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL);
            if ((N.wifi_page + 1) * 2 < N.wifi_count) ink_shell_bind(next, "shell.wifi_next", NULL);
            y += INK_CONTROL + INK_GAP_M;
        }
        if (!N.wifi_count) { ink_ui_notice(N.page_body, "还没有附近的网络列表", y); y += INK_LINE_NOTE + INK_GAP_L; }
        ink_ui_label(N.page_body, "SIM 卡", INK_FONT_LIST, INK_WEIGHT_BOLD, INK_MARGIN, y, width()); y += INK_LINE_LIST + INK_GAP_M;
        const char *const slots[] = {"外置卡", "内置卡"};
        lv_obj_t *sim = ink_ui_segments(N.page_body, slots, 2, (int)ink_setting_get("cell.sim_slot"), INK_MARGIN, y, width(), NULL, NULL);
        for (int i = 0; i < 2; i++) { char args[96]; snprintf(args, sizeof args, "{\"id\":\"cell.sim_slot\",\"value\":%d}", i); ink_shell_bind(lv_obj_get_child(sim, i), "settings.set", args); }
        y += INK_SEGMENT + INK_GAP_L;
        ink_ui_notice(N.page_body, integer("cell.registered") ? "4G 在线，切卡不用重启" : "4G 还没有连上", y);
    } else {
        y = notifications(N.page_body, 0, 6);
        lv_obj_t *read = ink_ui_pill(N.page_body, "全部已读", INK_MARGIN, y + INK_GAP, width(), INK_CONTROL, false, NULL, NULL); ink_shell_bind(read, "notify.read", "{\"id\":0}");
        y += INK_GAP + INK_CONTROL + INK_GAP;
        lv_obj_t *prev = ink_ui_pill(N.page_body, "上一页", INK_MARGIN, y, 2 * INK_PRIMARY, INK_CONTROL, false, NULL, NULL);
        if (N.notify_page) ink_shell_bind(prev, "shell.notify_prev", NULL);
        lv_obj_t *next = ink_ui_pill(N.page_body, "下一页", INK_CANVAS_W - INK_MARGIN - 2 * INK_PRIMARY, y, 2 * INK_PRIMARY, INK_CONTROL, false, NULL, NULL);
        if (N.notify_more) ink_shell_bind(next, "shell.notify_next", NULL);
        char page[40]; snprintf(page, sizeof page, "第 %d 页", N.notify_page + 1);
        lv_obj_t *caption = ink_ui_label(N.page_body, page, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + 2 * INK_PRIMARY,
                                         y + (INK_CONTROL - INK_LINE_NOTE) / 2, width() - 4 * INK_PRIMARY);
        lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    }
    ink_ui_hint_obj(N.page_body, INK_REFRESH_UI);
}

static void create_page(lv_obj_t *screen, void *arg) {
    N.page = screen; N.page_id = (page_t)(intptr_t)arg;
    static const char *const titles[] = {"设置", "显示和亮度", "声音", "网络和 SIM 卡", "通知"};
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, titles[N.page_id]);
    N.page_body = ink_ui_box(screen, 0, INK_LINE_BODY + INK_PRIMARY + INK_GAP_XS, INK_CANVAS_W,
                             INK_CANVAS_H - INK_LINE_BODY - INK_PRIMARY - INK_GAP_XS, 0, INK_SURFACE_WHITE, false);
    draw_page_body();
}
static void destroy_page(void) {
    if (N.page_id == PAGE_NETWORK) { N.picked_ssid[0] = 0; ink_text_blur(0); }
    N.page = N.page_body = NULL;
}
static bool page_key(ink_key_t key, bool lng) { (void)key; (void)lng; return false; }
static int page_snapshot(char *buf, size_t cap) {
    if (!current_session(N.epoch)) return snprintf(buf, cap, "空间锁定或正在切换；AI 无权读取。");
    int n = snprintf(buf, cap, "%s；当前=%s；亮度=%lld；暖光=%lld；音量=%lld；网络偏好=%lld；通知=%d。",
                    PAGES[N.page_id].name, ink_member_current()->name, (long long)ink_setting_get("display.light"), (long long)ink_setting_get("display.warmth"),
                    (long long)ink_setting_get("audio.volume"), (long long)ink_setting_get("net.prefer"), ink_notify_unread());
    if (N.page_id != PAGE_NOTIFICATIONS && n >= 0 && (size_t)n < cap) n += snprintf(buf + n, cap - (size_t)n, "字号=%lld；锁屏画面=%lld；自动锁屏秒=%lld；勿扰=%lld；叫名字=%lld；",
        (long long)ink_setting_get("display.font_size"), (long long)ink_setting_get("display.lock_style"), (long long)ink_setting_get("power.lock_after_s"),
        (long long)ink_setting_get("notify.dnd"), (long long)ink_setting_get("voice.handsfree"));
    if (N.page_id == PAGE_NETWORK && n >= 0 && (size_t)n < cap) {
        char ssid[64]; state_text("wifi.ssid", ssid, sizeof ssid);
        n += snprintf(buf + n, cap - (size_t)n, "已连网络=%s；选中网络=%s；网络组=%d；SIM=%lld；4G在线=%lld；",
            ssid, N.picked_ssid, N.wifi_page + 1, (long long)ink_setting_get("cell.sim_slot"), (long long)integer("cell.registered"));
        for (int i = N.wifi_page * 2; i < N.wifi_count && i < (N.wifi_page + 1) * 2 && n >= 0 && (size_t)n < cap; i++)
            n += snprintf(buf + n, cap - (size_t)n, "网络[%d]=%s；", i, N.wifi_ssid[i]);
    }
    if (N.page_id == PAGE_NOTIFICATIONS) {
        for (int i = 0; i < N.notify_count && n >= 0 && (size_t)n < cap; i++)
            n += snprintf(buf + n, cap - (size_t)n, "%u：%s，%s；已读=%d；", (unsigned)N.notify_items[i].id, N.notify_items[i].title, N.notify_items[i].body, N.notify_items[i].read);
    }
    const char *actions = N.page_id == PAGE_NOTIFICATIONS ? "动作：notify.read、notify.dismiss、shell.back、shell.control、shell.notify_next、shell.notify_prev。"
                          : N.page_id == PAGE_NETWORK ? "动作：settings.set、wifi.scan、wifi.list、shell.back、shell.control、shell.wifi_next、shell.wifi_prev、shell.wifi_pick。"
                          : "动作：settings.set、shell.back、shell.control。";
    if (n >= 0 && (size_t)n < cap) n += snprintf(buf + n, cap - (size_t)n, "%s", actions);
    return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
}

void ink_shell_open_page(const char *name) {
    for (unsigned i = 0; i < sizeof PAGES / sizeof *PAGES; i++) if (!strcmp(name, PAGES[i].name)) {
        if (child() && i != PAGE_NOTIFICATIONS) return;
        ink_shell_close(); ink_ui_push(&PAGES[i], (void *)(intptr_t)i); return;
    }
}

const char *ink_shell_board(void) {
    if (N.control) return "Control";
    if (N.home && !strcmp(ink_ui_page_name(), "home")) return N.home_page == 0 ? "Today" : N.home_page == 1 ? "Main" : "Apps";
    return ink_ui_top_name();
}

static void cap_nav(ink_call_t *c) {
    if (!strcmp(c->cap->id, "shell.control")) ink_shell_control();
    else if (!strcmp(c->cap->id, "shell.lock")) { ink_shell_close(); ink_power_lock(); }
    else if (!strcmp(c->cap->id, "shell.close")) ink_shell_close();
    else if (!strcmp(c->cap->id, "shell.back")) { if (!ink_shell_close()) ink_ui_pop(); }
    else if (!strcmp(c->cap->id, "shell.members")) ink_shell_members_open(false);
    else if (!strcmp(c->cap->id, "shell.more")) { N.more = !N.more; if (N.control) draw_control(); }
    else if (!strcmp(c->cap->id, "home.next")) turn(1);
    else if (!strcmp(c->cap->id, "home.prev")) turn(-1);
    else if (!strcmp(c->cap->id, "shell.search")) {
        ink_args_get_str(&c->args, "query", N.search, sizeof N.search);
        N.apps_page = 0;
        ink_text_blur(0);
        if (N.content) { N.home_page = 2; ink_ui_content_changed(); draw_home_content(); }
    }
    else if (!strcmp(c->cap->id, "shell.settings")) ink_shell_open_page("Settings");
    else if (!strcmp(c->cap->id, "shell.display")) ink_shell_open_page("SetDisplay");
    else if (!strcmp(c->cap->id, "shell.sound")) ink_shell_open_page("SetSound");
    else if (!strcmp(c->cap->id, "shell.network")) ink_shell_open_page("SetNetwork");
    else if (!strcmp(c->cap->id, "shell.notifications")) ink_shell_open_page("Notifications");
    else if (!strcmp(c->cap->id, "shell.notify_next") || !strcmp(c->cap->id, "shell.notify_prev")) {
        if (N.page_body && N.page_id == PAGE_NOTIFICATIONS) {
            if (!strcmp(c->cap->id, "shell.notify_next") && N.notify_more) N.notify_page++;
            else if (!strcmp(c->cap->id, "shell.notify_prev") && N.notify_page) N.notify_page--;
            ink_ui_content_changed(); ink_ui_stage_next(); draw_page_body();
        }
    } else if (!strcmp(c->cap->id, "shell.wifi_next") || !strcmp(c->cap->id, "shell.wifi_prev")) {
        if (N.page_body && N.page_id == PAGE_NETWORK && !N.picked_ssid[0]) {
            if (!strcmp(c->cap->id, "shell.wifi_next") && (N.wifi_page + 1) * 2 < N.wifi_count) N.wifi_page++;
            else if (!strcmp(c->cap->id, "shell.wifi_prev") && N.wifi_page) N.wifi_page--;
            ink_ui_content_changed(); ink_ui_stage_next(); draw_page_body();
        }
    } else if (!strcmp(c->cap->id, "shell.wifi_pick")) {
        int64_t index = -1; ink_args_get_int(&c->args, "index", &index);
        if (!N.page_body || N.page_id != PAGE_NETWORK || index < 0 || index >= N.wifi_count) c->rc.status = INK_RC_REJECTED;
        else { snprintf(N.picked_ssid, sizeof N.picked_ssid, "%s", N.wifi_ssid[index]); draw_page_body(); }
    }
    snprintf(c->rc.object, sizeof c->rc.object, "screen:%s", ink_shell_board());
}

static void cap_launch(ink_call_t *c) {
    char id[32]; ink_args_get_str(&c->args, "id", id, sizeof id);
    if (!strcmp(id, "settings")) { if (child()) c->rc.status = INK_RC_REJECTED; else ink_shell_open_page("Settings"); return; }
    if (!strcmp(id, "ai")) { ink_ai_tap(); return; }
    if (c->src == INK_SRC_TOUCH) ink_probe_count("home.taps");
    for (int i = 0; i < ink_app_count(); i++) if (!strcmp(ink_app_at(i)->id, id) && ink_app_at(i)->entry) {
        uint64_t cause = osal_now_us();
        if (ink_app_open(id, NULL, NULL) != 0) c->rc.status = INK_RC_REJECTED;
        else ink_ui_cause(cause, "ui.open_to_stable_ms");
        return;
    }
    c->rc.status = INK_RC_REJECTED;
    snprintf(N.result, sizeof N.result, "这个应用还没有可打开的内容");
    snprintf(c->rc.say, sizeof c->rc.say, "%s", N.result);
    if (N.message) lv_label_set_text(N.message, N.result);
}
static void cap_resume(ink_call_t *c) {
    ink_card_t card;
    if (bookmark(&card) && card.action[0]) {
        ink_call_opts_t opts = {.src = c->src, .reply_exec = INK_EXEC_UI};
        if (ink_cap_call(card.action, NULL, &opts) == 0) return;
    }
    if (ink_app_open("reader", NULL, NULL) != 0) c->rc.status = INK_RC_REJECTED;
    ink_probe_count("home.taps");
}

#define NAV(id_, title_, child_) {.id = id_, .title = title_, .effect = INK_FX_NAVIGATE, .child_ok = child_, .run = cap_nav}
static const ink_cap_t CAPS[] = {
    NAV("shell.control", "打开控制中心", true), NAV("shell.close", "收起面板", true), NAV("shell.back", "返回上一层", true),
    NAV("shell.lock", "锁屏", true),
    NAV("shell.members", "选择家人", true), NAV("shell.more", "更多控制", false),
    NAV("shell.settings", "打开设置", false), NAV("shell.display", "显示和亮度", false), NAV("shell.sound", "声音", false),
    NAV("shell.network", "网络和 SIM 卡", false), NAV("shell.notifications", "查看通知", true),
    NAV("shell.notify_next", "通知下一页", true), NAV("shell.notify_prev", "通知上一页", true),
    NAV("shell.wifi_next", "附近网络下一组", false), NAV("shell.wifi_prev", "附近网络上一组", false),
    {.id = "shell.search", .title = "查找应用", .activity = "home", .input = "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"maxLength\":64}},\"required\":[\"query\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_nav},
    {.id = "shell.wifi_pick", .title = "选附近网络（密码由人在设备上填写）", .input = "{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\",\"minimum\":0}},\"required\":[\"index\"]}", .effect = INK_FX_NAVIGATE, .child_ok = false, .run = cap_nav},
    {.id = "home.next", .title = "主页下一屏", .activity = "home", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_nav},
    {.id = "home.prev", .title = "主页上一屏", .activity = "home", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_nav},
    {.id = "shell.launch", .title = "打开主页上的应用", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_launch},
    {.id = "shell.resume", .title = "接着读", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_resume}
};

static void refresh(void *unused) {
    (void)unused;
    if (ink_power_state() == INK_PWR_LOCKED) return;
    if (!current_session(N.epoch)) return;
    bool body = __atomic_exchange_n(&N.need_body, false, __ATOMIC_ACQ_REL);
    if (body && N.control && ink_member_space_open()) draw_control();
    if (body && N.page_body && ink_member_space_open() && !N.picked_ssid[0]) draw_page_body();
    for (unsigned i = 0; i < sizeof N.status / sizeof *N.status; i++) if (N.status[i]) {
        lv_obj_t *parent = lv_obj_get_parent(N.status[i]);
        bool show_time = N.status_time[i];
        lv_obj_delete(N.status[i]);
        lv_obj_t *bar = ink_shell_statusbar(parent, show_time);
        ink_ui_hint_obj(bar, INK_REFRESH_FAST);
    }
    if (N.clock) {
        char clock[16], date[96]; ink_shell_time_strings(clock, sizeof clock, date, sizeof date);
        if (strcmp(lv_label_get_text(N.clock), clock)) {
            lv_label_set_text(N.clock, clock);
            ink_ui_hint_obj(N.clock, INK_REFRESH_TEXT);
        }
        if (strcmp(lv_label_get_text(N.date), date)) {
            lv_label_set_text(N.date, date);
            ink_ui_hint_obj(N.date, INK_REFRESH_UI);
        }
    }
}
static void changed(void *ctx, int h, const char *id) {
    (void)h;
    if (ctx && (!strcmp(id, "member.current") || (!strcmp(id, "member.authed") && !ink_member_space_open()))) {
        __atomic_add_fetch(&N.session, 1, __ATOMIC_ACQ_REL);
        return;
    }
    if (ctx && strncmp(id, "time.", 5) && strcmp(id, "power.state") && strcmp(id, "notify.unread") && strcmp(id, "audio.mic") && strcmp(id, "net.type") && strncmp(id, "wifi.", 5) && strcmp(id, "bt.connected")) return;
    if (!ctx || !strcmp(id, "notify.unread") || !strncmp(id, "wifi.", 5) || !strcmp(id, "bt.connected")) __atomic_store_n(&N.need_body, true, __ATOMIC_RELEASE);
    ink_exec_post(INK_EXEC_UI, &N.refresh, refresh, NULL);
}
static const char *const FONT_CHOICES[] = {"小", "中", "大", "特大", NULL};
static const char *const LOCK_CHOICES[] = {"这一页", "时钟和待办", NULL};
static const ink_setting_def_t SETTINGS[] = {
    {.id = "display.font_size", .title = "字号", .type = INK_SET_ENUM, .def = 1, .choices = FONT_CHOICES, .child_may_change = true},
    {.id = "display.lock_style", .title = "锁屏画面", .type = INK_SET_ENUM, .def = 0, .choices = LOCK_CHOICES, .child_may_change = true}
};

int ink_shell_ai_init(void);

int ink_shell_init(void) {
    if (ink_shell_ai_init() != 0) return -1;
    if (ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_settings_define(SETTINGS, sizeof SETTINGS / sizeof *SETTINGS)) return -1;
    if (ink_settings_watch("", changed, NULL, INK_EXEC_UI)) return -1;
    return ink_state_watch("", changed, (void *)1, INK_EXEC_DIRECT);
}
