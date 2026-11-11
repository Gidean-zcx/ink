/* Three home pages, control centre and the first settings pages. */
/* ui-scale-runtime: dimension width Canvas width minus theme margins, shared by all shell pages. */
/* ui-scale-runtime: dimension notifications Returns the bottom of theme-sized notification rows. */
/* ui-scale-runtime: dimension y tile and notifications accept a theme-derived row position. */
/* ui-scale-runtime: dimension ink_activity_card_draw,ink_activity_home_offset F6 returns theme-sized row heights and offsets. */
/* ui-scale-runtime: dimension top Child volume marker uses the configured percentage. */
/* ui-scale-runtime: dimension bb_x,bb_y The battery icon's position, passed from the theme-sized status bar. */
/* ui-scale-runtime: dimension F.y,heading_h,notice_h,home_room,below_heading Page flow: theme sizes added up, plus text measured at the current size. */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/ai.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/camera.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/sys/json.h"
#include "ink/sys/notify.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/search.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/sys/ui_semantics.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include "activity_card.h"

typedef struct {
    char cap[48], args[192];
    uint32_t epoch, member_epoch, version, generation;
    bool versioned;
} binding_t;
typedef enum { PAGE_SETTINGS, PAGE_DISPLAY, PAGE_SOUND, PAGE_NETWORK, PAGE_NOTIFICATIONS, PAGE_CHANNELS, PAGE_KNOWN } page_t;
static const char *const CHANNEL_NOTE = TR_NOOP("关闭后不再收进新通知，已有通知保留。电话和闹钟照常。");
static void create_page(lv_obj_t *screen, void *arg);
static void destroy_page(void);
static int page_snapshot(char *buf, size_t cap);
static bool page_key(ink_key_t key, bool long_press);
static const ink_activity_t PAGES[] = {
    {.name = "Settings", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key},
    {.name = "SetDisplay",
     .create = create_page,
     .destroy = destroy_page,
     .snapshot = page_snapshot,
     .on_key = page_key},
    {.name = "SetSound", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key},
    {.name = "SetNetwork",
     .create = create_page,
     .destroy = destroy_page,
     .snapshot = page_snapshot,
     .on_key = page_key},
    {.name = "Notifications",
     .create = create_page,
     .destroy = destroy_page,
     .snapshot = page_snapshot,
     .on_key = page_key},
    {.name = "SetNotifications", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key},
    {.name = "KnownNetworks", .create = create_page, .destroy = destroy_page, .snapshot = page_snapshot, .on_key = page_key}};

#define KNOWN_NETWORKS_MAX 8

static struct {
    lv_obj_t *home, *content, *dots, *clock, *date, *message;
    lv_obj_t *control, *page, *page_body;
    lv_obj_t *status[3];
    bool status_time[3];
    int home_page, apps_page, apps_pages, wifi_page, body_page, body_pages, wifi_forget_page;
    page_t page_id;
    bool more, initialized, need_body;
    int cell_bars_shown; /* the signal bars the status bar last drew (atomic): only a different count redraws it */
    char member[16];
    uint32_t session, epoch, icon_version, icon_generation;
    char result[160], search[80];
    char wifi_ssid[6][40], picked_ssid[40];
    int wifi_count;
    char known_ssid[KNOWN_NETWORKS_MAX][33];
    int known_count;
    uint32_t known_epoch;
    bool scan_busy;
    ink_notify_view_t control_items[2], notify_items[6];
    int control_count, notify_capacity;
    int channel_first, channel_end;
    ink_ui_list_t *notify_list;
    ink_ui_list_position_t notify_position;
    char notify_ids[6][32], notify_args[6][64];
    ink_exec_job_t refresh;
    ink_card_t cards[5];
    ink_ui_field_spec_t card_fields[5];
    ink_ui_action_spec_t card_actions[5];
    char card_objects[5][48];
    ink_ui_page_spec_t today;
    lv_timer_t *cards_timer;
    lv_timer_t *note_timer; /* clears a passing note on the home page */
    int64_t cards_rev;
    uint64_t cards_at;
    bool cards_ok;
    char kid_time[80];
    lv_obj_t *kid_time_label;
} N INK_PSRAM_BSS;
static void draw_page_body(void);
static void draw_home_content(void);
static void today_load(void);
static bool today_match(const ink_json_t *j, int slot);
static void today_model(void);
static int today_slots(void);
static const char *today_icon(int slot);
static void icon_bind(lv_obj_t *obj, const char *id);
static void icon_event(lv_event_t *e);
static bool icon_close(void);
static int today_init(void);
static void kid_today_model(void);
static void kid_volume_mark(lv_obj_t *slider, int top);
static bool current_session(uint32_t epoch);

static void known_reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != N.known_epoch || !current_session(N.epoch) || !N.page_body || N.page_id != PAGE_KNOWN) return;
    N.known_count = -2;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        N.known_count = 0;
        for (int i = 1; i < j->ntok && N.known_count < KNOWN_NETWORKS_MAX; i++)
            if (ink_json_str(j, i, N.known_ssid[N.known_count], sizeof N.known_ssid[0]) >= 0) N.known_count++;
    }
    osal_free(j);
    ink_ui_content_changed();
    draw_page_body();
}
static void known_load(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (!N.page_body || N.page_id != PAGE_KNOWN) return;
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_UI, .done = known_reply,
        .ctx = (void *)(uintptr_t)N.known_epoch};
    if (ink_cap_call("wifi.list", NULL, &o)) { N.known_count = -2; draw_page_body(); }
}

static bool current_session(uint32_t epoch) {
    return epoch == __atomic_load_n(&N.session, __ATOMIC_ACQUIRE) && !strcmp(N.member, ink_member_current()->id) &&
           ink_member_space_open();
}

static void wifi_receipt(void *ctx, const ink_receipt_t *rc) {
    uintptr_t request = (uintptr_t) ctx;
    if (!current_session((uint32_t) (request >> 1))) return;
    bool scan = (request & 1) != 0;
    N.scan_busy = false;
    snprintf(N.result, sizeof N.result, "%s",
             rc->say[0]                  ? rc->say
             : rc->status == INK_RC_DONE ? TR("已经找过附近的网络")
                                         : TR("没能完成，可以再试一次"));
    if (scan && rc->status == INK_RC_DONE && rc->result) {
        ink_json_t j;
        N.wifi_count = 0;
        N.wifi_page = 0;
        if (ink_json_parse(&j, rc->result, strlen(rc->result)) == 0)
            for (int i = 1; i < j.ntok && N.wifi_count < 6; i++) {
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

static int width(void) {
    return INK_CANVAS_W - 2 * INK_MARGIN;
}
static bool child(void) {
    return ink_member_current()->role == INK_ROLE_CHILD;
}
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
static void kid_time_read(void) {
    ink_call_opts_t opts = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_UI, .done = kid_time_reply,
        .ctx = (void *)(uintptr_t)N.epoch, .member = ink_member_current()->id};
    ink_cap_call("member.time_get", NULL, &opts);
}
static int64_t integer(const char *id) {
    int64_t v = 0;
    ink_state_get_int(ink_state_find(id), &v);
    return v;
}
static void state_text(const char *id, char *buf, size_t cap) {
    if (ink_state_get_str(ink_state_find(id), buf, cap) < 0 && cap) buf[0] = 0;
}
static const char *wifi_line(char *ssid, size_t cap) { /* the Wi-Fi network in use, or why there is none */
    state_text("wifi.ssid", ssid, cap);
    return !ink_setting_get("wifi.enabled") ? TR("Wi-Fi 已关闭") : ssid[0] ? ssid : TR("还没有连接 Wi-Fi");
}

static void refused(void *ctx, const ink_receipt_t *rc) { (void)ctx; ink_shell_refused(rc); } /* never silent */

static void airplane_done(void *ctx, const ink_receipt_t *rc) {
    if (!current_session((uint32_t)(uintptr_t)ctx)) return;
    if (rc->status == INK_RC_DONE && !ink_setting_get("net.airplane")) {
        if (!strcmp(ink_ui_top_name(), "AiOffline")) ink_ai_dismiss();
        ink_shell_close();
    }
    ink_shell_refused(rc);
}

static void binding_event(lv_event_t *e) {
    binding_t *b = lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_DELETE) {
        osal_free(b);
        return;
    }
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        b->version = ink_ui_content_version(); b->generation = ink_ui_generation();
        return;
    }
    if (b->versioned && b->version && (b->version != ink_ui_content_version() || b->generation != ink_ui_generation())) return;
    const ink_cap_t *cap = ink_cap_find(b->cap);
    bool locked_action = ink_ui_locked_activity() ? ink_ui_locked_allows(b->cap) : cap && cap->while_locked;
    if (!current_session(b->epoch) && (b->member_epoch != ink_member_epoch() || !locked_action)) return;
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .page_gen = b->versioned ? ink_ui_content_version() : 0,
                         .args_json = b->args[0] ? b->args : NULL,
                         .reply_exec = INK_EXEC_UI,
                         .member = ink_member_current()->id, .done = refused};
    if (!strcmp(b->cap, "wifi.scan")) {
        if (N.scan_busy) return;
        N.scan_busy = true;
        o.done = wifi_receipt;
        o.ctx = (void *) (((uintptr_t) b->epoch << 1) | 1);
        ink_ui_busy_control(lv_event_get_target_obj(e), true);
    }
    if (!strcmp(b->cap, "text.voice_start")) ink_ui_cause(osal_now_us(), "text.voice_tap_to_panel_ms");
    if (!strcmp(b->cap, "net.set_airplane")) { o.done = airplane_done; o.ctx = (void *)(uintptr_t)b->epoch; }
    ink_cap_call(b->cap, NULL, &o);
}

void ink_shell_bind(lv_obj_t *obj, const char *cap, const char *json) {
    binding_t *b = osal_calloc(1, sizeof *b, OSAL_MEM_EXTERNAL);
    if (!b) return;
    snprintf(b->cap, sizeof b->cap, "%s", cap);
    snprintf(b->args, sizeof b->args, "%s", json ? json : "");
    b->epoch = N.epoch;
    b->member_epoch = ink_member_epoch();
    const char *page = ink_ui_top_name();
    b->versioned = !strcmp(page, "home") || !strcmp(page, "Ai") || !strcmp(page, "BatteryLow") ||
                   !strcmp(page, "KidLimit") || !strcmp(page, "KidTimeUp") || !strncmp(cap, "text.", 5);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    if (lv_obj_get_style_radius(obj, 0) == LV_RADIUS_CIRCLE &&
        lv_obj_get_style_width(obj, 0) == lv_obj_get_style_height(obj, 0) &&
        ink_ui_surface_kind(obj) == INK_SURFACE_WHITE)
        lv_obj_set_style_border_width(obj, ink_ui_px(INK_STROKE), 0);
    lv_obj_add_event_cb(obj, binding_event, LV_EVENT_CLICKED, b);
    lv_obj_add_event_cb(obj, binding_event, LV_EVENT_PRESSED, b);
    lv_obj_add_event_cb(obj, binding_event, LV_EVENT_DELETE, b);
}

const char *ink_shell_bound_cap(lv_obj_t *obj) {
    for (uint32_t i = 0; i < lv_obj_get_event_count(obj); i++) {
        lv_event_dsc_t *d = lv_obj_get_event_dsc(obj, i);
        if (lv_event_dsc_get_cb(d) == icon_event) return "shell.launch";
        if (lv_event_dsc_get_cb(d) == binding_event) return ((binding_t *) lv_event_dsc_get_user_data(d))->cap;
    }
    return NULL;
}

void ink_shell_time_strings(char *clock, size_t cc, char *date, size_t dc) {
    if (!ink_time_valid()) {
        snprintf(clock, cc, "--:--");
        snprintf(date, dc, TR("时间还没有设置"));
        return;
    }
    ink_tm_t tm;
    ink_time_to_local(ink_time_now_s(), &tm);
    static const char *const WEEK[] = {TR_NOOP("周日"), TR_NOOP("周一"), TR_NOOP("周二"), TR_NOOP("周三"), TR_NOOP("周四"), TR_NOOP("周五"), TR_NOOP("周六")};
    snprintf(clock, cc, "%02d:%02d", tm.hour, tm.min);
    ink_lunar_t lunar;
    char lunar_name[40] = "";
    if (ink_lunar_from_solar(tm.year, tm.month, tm.day, &lunar)) ink_lunar_name(&lunar, lunar_name, sizeof lunar_name);
    snprintf(date, dc, TR("%d月%d日 %s · %s"), tm.month, tm.day, TR(WEEK[tm.wday]), lunar_name);
}

static void status_deleted(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target_obj(e);
    for (unsigned i = 0; i < sizeof N.status / sizeof *N.status; i++)
        if (N.status[i] == obj) N.status[i] = NULL;
}

/* Signal bars from the modem's reading in dBm (0: none). */
static int cell_bars(int64_t dbm) { return dbm == 0 ? 0 : dbm >= -75 ? 4 : dbm >= -85 ? 3 : dbm >= -95 ? 2 : 1; }

/* The battery's bars, in quarters: the outline alone said nothing at a glance (trial report 2026-11-11). Placed in
 * pixels from the drawn icon, as the icon itself is, so they sit centred in its body: inside the stroke the body is
 * 5.4..38.6 x 15.4..32.6 of the icon's 48 units; two pixels clear all round and between the bars. */
static void battery_bars(lv_obj_t *bar, int bb_x, int bb_y, int64_t level) {
    int bars = level >= 88 ? 4 : level >= 63 ? 3 : level >= 38 ? 2 : level >= 13 ? 1 : 0;
    /* All in 48ths of the drawn icon, around its body's centre (22, 24): no rounding drift to one side. */
    int s = ink_ui_px(INK_ICON_LIST), w = s * 6 / 48, gap = s * 2 / 48, h = s * 14 / 48;
    int x0 = ink_ui_px(bb_x) + s * 22 / 48 - (4 * w + 3 * gap) / 2, y0 = ink_ui_px(bb_y) + s * 24 / 48 - h / 2;
    for (int i = 0; i < bars; i++) {
        lv_obj_t *o = lv_obj_create(bar);
        lv_obj_remove_style_all(o);
        lv_obj_set_style_bg_color(o, ink_ui_ink(), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_set_pos(o, x0 + i * (w + gap), y0);
        lv_obj_set_size(o, w, h);
    }
}

lv_obj_t *ink_shell_statusbar(lv_obj_t *parent, bool show_time) {
    lv_obj_t *bar = ink_ui_box(parent, 0, 0, INK_CANVAS_W, INK_STATUS_BAR, 0, INK_SURFACE_WHITE, false);
    ink_shell_bind(bar, "shell.control", NULL);
    char clock[16], date[96];
    ink_shell_time_strings(clock, sizeof clock, date, sizeof date);
    int x = INK_MARGIN;
    int bar_h = INK_STATUS_BAR, text_y = (bar_h - ink_ui_line_height(INK_FONT_STATUS)) / 2;
    int icon_y = (bar_h - INK_ICON_SMALL) / 2;
    int time_w = ink_ui_text_width(clock, INK_FONT_STATUS, INK_WEIGHT_MEDIUM);
    if (show_time) {
        ink_ui_line(bar, clock, INK_FONT_STATUS, INK_WEIGHT_MEDIUM, x, text_y, time_w);
        x += time_w + INK_GAP_S;
    }
    char net[32], mic[32];
    state_text("net.type", net, sizeof net);
    state_text("audio.mic", mic, sizeof mic);
    bool plane = ink_setting_get("net.airplane"), wifi = !strcmp(net, "wifi");
    const char *cell = integer("cell.registered") ? "4G" : integer("cell.sim_missing") ? TR("无卡") : TR("无服务");
    int cell_w = ink_ui_text_width(cell, INK_FONT_STATUS, INK_WEIGHT_MEDIUM);
    if (plane) { ink_ui_icon(bar, "plane", INK_ICON_SMALL, INK_ICON_LINE, x, icon_y, false); x += INK_ICON_LIST; }
    ink_ui_icon(bar, wifi ? "wifi" : "wifioff", INK_ICON_SMALL, INK_ICON_LINE, x, icon_y, false);
    x += INK_ICON_LIST;
    if (!plane) {
        ink_ui_line(bar, cell, INK_FONT_STATUS, INK_WEIGHT_MEDIUM, x, text_y, cell_w);
        x += cell_w + INK_GAP_S;
        if (integer("cell.registered")) {
            int bars = cell_bars(integer("cell.signal"));
            __atomic_store_n(&N.cell_bars_shown, bars, __ATOMIC_RELEASE);
            int s = ink_ui_px(INK_ICON_SMALL), step = s / 4;
            for (int i = 0; i < 4; i++) {
                lv_obj_t *o = lv_obj_create(bar);
                lv_obj_remove_style_all(o);
                lv_obj_set_style_border_color(o, ink_ui_ink(), 0);
                lv_obj_set_style_border_width(o, ink_ui_px(INK_STROKE) / 2, 0);
                lv_obj_set_style_bg_color(o, ink_ui_ink(), 0);
                lv_obj_set_style_bg_opa(o, i < bars ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
                int h = step * (i + 1);
                lv_obj_set_pos(o, ink_ui_px(x) + i * step, ink_ui_px(icon_y) + s - h);
                lv_obj_set_size(o, step - ink_ui_px(INK_STROKE), h);
            }
            x += INK_ICON_LIST;
        }
    }
    if (integer("notify.unread"))
        ink_ui_icon(bar, "bell", INK_ICON_SMALL, INK_ICON_LINE, x, icon_y, false);
    if (!strcmp(mic, "local"))
        ink_ui_icon(bar, "mic", INK_ICON_SMALL, INK_ICON_LINE, x + INK_ICON_LIST, icon_y, false);
    else if (!strcmp(mic, "sending") || !strcmp(mic, "recording")) {
        int pill_h = ink_ui_line_height(INK_FONT_STATUS) + INK_GAP_S;
        if (pill_h > bar_h) pill_h = bar_h;
        lv_obj_t *s = ink_ui_box(bar, x + INK_ICON_LIST, (bar_h - pill_h) / 2, INK_CONTROL, pill_h, INK_RADIUS_KEY,
                                 INK_SURFACE_BLACK, false);
        lv_obj_t *on_air =
            ink_ui_line(s, !strcmp(mic, "recording") ? TR("录音") : TR("在听"), INK_FONT_STATUS, INK_WEIGHT_MEDIUM, 0,
                        (pill_h - ink_ui_line_height(INK_FONT_STATUS)) / 2, INK_CONTROL);
        lv_obj_set_style_text_align(on_air, LV_TEXT_ALIGN_CENTER, 0);
    }
    char battery[12];
    snprintf(battery, sizeof battery, "%lld", (long long) integer("battery.level"));
    int battery_w = ink_ui_text_width(battery, INK_FONT_STATUS, INK_WEIGHT_MEDIUM);
    ink_ui_line(bar, battery, INK_FONT_STATUS, INK_WEIGHT_MEDIUM,
                INK_CANVAS_W - INK_MARGIN - INK_ICON_LIST - INK_GAP_S - battery_w, text_y, battery_w);
    int battery_x = INK_CANVAS_W - INK_MARGIN - INK_ICON_LIST, battery_y = (bar_h - INK_ICON_LIST) / 2;
    ink_ui_icon(bar, "battery", INK_ICON_LIST, INK_ICON_LINE, battery_x, battery_y, false);
    battery_bars(bar, battery_x, battery_y, integer("battery.level"));
    for (unsigned i = 0; i < sizeof N.status / sizeof *N.status; i++)
        if (!N.status[i]) {
            N.status[i] = bar;
            N.status_time[i] = show_time;
            break;
        }
    lv_obj_add_event_cb(bar, status_deleted, LV_EVENT_DELETE, NULL);
    return bar;
}

void ink_shell_topbar(lv_obj_t *parent, const char *title) {
    int y = INK_STATUS_BAR + INK_GAP_S;
    lv_obj_t *back =
        ink_ui_circle(parent, "back", INK_MARGIN, y, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_STATIC, NULL, NULL);
    ink_shell_bind(back, "shell.back", NULL);
    ink_ui_label(parent, title, INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN + INK_CONTROL + INK_GAP,
                 y + (INK_CONTROL - ink_ui_line_height(INK_FONT_TITLE)) / 2, width() - INK_CONTROL - INK_GAP);
}

static bool bookmark(ink_card_t *out) {
    memset(out, 0, sizeof *out);
    for (int i = 0; i < ink_app_count(); i++) {
        const ink_app_def_t *a = ink_app_at(i);
        if (!ink_app_visible(a)) continue;
        for (int j = 0; a->card && a->card(j, out) == 1; j++)
            if (out->bookmark && ink_app_card_visible(out)) return true;
    }
    return false;
}

static const char *const CARD_APPS[] = {"reader", "listen", "news", "word", "upcoming"};
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
static void home_note(const char *text) {
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
                   INK_CANVAS_W / 2 - INK_RADIUS_PANEL + i * INK_RADIUS_PANEL - (i == N.home_page ? INK_GAP_M : 0),
                   INK_GAP, i == N.home_page ? INK_ICON_MEDIUM : INK_GAP_M, INK_GAP_M, -1,
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
static const char *const APP_ICONS[] = {INK_UI_ICON(headphones), INK_UI_ICON(news),  INK_UI_ICON(word),
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
    return INK_CANVAS_H - INK_STATUS_BAR - INK_DOCK - INK_MARGIN - ink_ui_line_height(INK_FONT_LIST);
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

static void draw_home_content(void) {
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
        static const int kid_order[] = {0, 1, 2, 5, 6, 3, 4};
        for (int i = 0; i < (child() ? 7 : 8); i++) {
            int at = child() ? kid_order[i] : i;
            tile(N.content, APP_IDS[at], APP_TITLES[at], APP_ICONS[at], i % 4, i / 4, grid, false, 4);
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
        lv_obj_t *c = ink_ui_card(N.content, INK_MARGIN, first, width(), h, INK_SURFACE_BLACK);
        ink_ui_line(c, N.card_fields[0].label, INK_FONT_NOTE, INK_WEIGHT_MEDIUM, INK_MARGIN, INK_GAP_L, width() - 2 * INK_MARGIN);
        lv_obj_t *reading =
            ink_ui_label(c, N.card_fields[0].value, INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN,
                         INK_GAP_L + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_S, width() - 2 * INK_MARGIN);
        lv_label_set_long_mode(reading, LV_LABEL_LONG_DOT);
        lv_obj_set_height(reading, ink_ui_px(2 * ink_ui_line_height(INK_FONT_TITLE)));
        bind_card(c, 0);
        for (int i = 0; i < today_slots(); i++) {
            int w = (width() - INK_GAP) / 2;
            int y = first + h + INK_GAP + (i / 2) * (h + INK_GAP);
            lv_obj_t *small = ink_ui_card(N.content, INK_MARGIN + (i % 2) * (w + INK_GAP), y, w, h, INK_SURFACE_WHITE);
            ink_ui_icon(small, today_icon(i), INK_ICON_MEDIUM, INK_ICON_LINE, INK_GAP_L, INK_GAP_L, false);
            ink_ui_line(small, N.card_fields[i + 1].label, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_GAP_L + INK_ICON_MEDIUM + INK_GAP_S,
                        INK_GAP_L, w - 2 * INK_GAP_L - INK_ICON_MEDIUM - INK_GAP_S);
            lv_obj_t *what = ink_ui_label(small, N.card_fields[i + 1].value, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_GAP_L,
                                          INK_GAP_L + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_M, w - 2 * INK_GAP_L);
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
    N.dots = ink_ui_box(screen, 0, INK_STATUS_BAR + h, INK_CANVAS_W, ink_ui_line_height(INK_FONT_LIST), 0,
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

static void turn(int delta) {
    if (N.home_page == 2 && ((delta > 0 && N.apps_page + 1 < N.apps_pages) || (delta < 0 && N.apps_page > 0))) {
        N.apps_page += delta;
        ink_ui_content_changed();
        ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
        draw_home_content();
        return;
    }
    int next = N.home_page + delta;
    if (!N.content || next < 0 || next > 2) return;
    N.home_page = next;
    ink_ui_content_changed();
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI); /* D-108: all of it is new, no ghost of the last page */
    draw_home_content();
    ink_probe_count("home.page_turns");
}
bool ink_shell_home_swipe(ink_swipe_dir_t dir) {
    if (dir == INK_SWIPE_LEFT)
        turn(1);
    else if (dir == INK_SWIPE_RIGHT)
        turn(-1);
    else
        return false;
    return true;
}
bool ink_shell_home_key(ink_key_t key, bool long_press) {
    if (long_press) return false;
    if (key == INK_KEY_NEXT)
        turn(1);
    else if (key == INK_KEY_PREV)
        turn(-1);
    else
        return false;
    return true;
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

static int kid_control_snapshot(char *buf, size_t cap);
static void notify_open(ink_call_t *c);
static int control_snapshot(char *buf, size_t cap) {
    if (!current_session(N.epoch)) return snprintf(buf, cap, "空间锁定或正在切换；AI 无权读取。");
    if (child()) return kid_control_snapshot(buf, cap);
    char activity[512]; ink_activity_card_snapshot(activity, sizeof activity);
    char ssid[64];
    const char *net = wifi_line(ssid, sizeof ssid);
    int n = snprintf(buf, cap,
                     "Control；当前=%s；亮度=%lld；音量=%lld；勿扰=%lld；叫名字=%lld；更多=%d；通知=%d；网络=%s；",
                     ink_member_current()->name, (long long) ink_setting_get("display.light"),
                     (long long) ink_setting_get("audio.volume"), (long long) ink_setting_get("notify.dnd"),
                     (long long) ink_setting_get("voice.handsfree"), N.more, ink_notify_unread(), net);
    if (N.more && n >= 0 && (size_t) n < cap)
        n += snprintf(buf + n, cap - (size_t) n, "暖光=%lld；飞行模式=%lld（net.set_airplane）；", (long long) ink_setting_get("display.warmth"),
                      (long long) ink_setting_get("net.airplane"));
    for (int i = 0; i < N.control_count && n >= 0 && (size_t) n < cap; i++)
        n += snprintf(buf + n, cap - (size_t) n, "通知[%u]=%s，%s；已读=%d；", (unsigned) N.control_items[i].id,
                      N.control_items[i].title, N.control_items[i].body, N.control_items[i].read);
    const char *actions = child() ? "动作：settings.set、system.set_volume、shell.notify_open、notify.read、shell.close。"
                                  : "动作：settings.set、system.set_volume、notify.read、shell.members、shell."
                                    "settings、shell.network、shell.more、shell.close。";
    if (n >= 0 && (size_t) n < cap) n += snprintf(buf + n, cap - (size_t) n, "%s%s；shell.tasks。", actions, activity);
    return n < 0 ? 0 : (size_t) n < cap ? n : (int) cap - 1;
}
static bool control_key(ink_key_t key, bool lng) {
    if (!lng && (key == INK_KEY_HOME || key == INK_KEY_BACK)) ink_shell_close();
    return true;
}
static const ink_ui_overlay_t CONTROL = {
    .name = "Control", .level = 0, .on_key = control_key, .snapshot = control_snapshot};
static const ink_ui_overlay_t KID_CONTROL = {.name = "KidControl", .level = 0, .on_key = control_key, .snapshot = control_snapshot};

static void slider_changed(lv_event_t *e) {
    const char *id = lv_event_get_user_data(e);
    lv_obj_t *o = lv_event_get_target_obj(e);
    lv_indev_t *in = lv_event_get_indev(e);
    if (!in) return;
    lv_point_t pt;
    lv_indev_get_point(in, &pt);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
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
    char json[128];
    snprintf(json, sizeof json, "{\"id\":\"%s\",\"value\":%lld}", id, (long long) !ink_setting_get(id));
    ink_shell_bind(o, "settings.set", json);
}

static int notifications(lv_obj_t *parent, int y, int limit) {
    ink_db_cursor_t cursor = {0};
    ink_notify_view_t n;
    int count = 0;
    ink_notify_view_t *visible = N.control_items;
    while (count < limit && ink_notify_next(&cursor, &n) == 1) {
        visible[count] = n;
        int h = ink_ui_row_double(),
            text_y = (h - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
        lv_obj_t *card = ink_ui_card(parent, INK_MARGIN, y, width(), h, INK_SURFACE_WHITE);
        ink_ui_circle(card, !strcmp(n.channel, "message") ? "message" : "bell", INK_GAP, (h - INK_CIRCLE) / 2,
                      INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL);
        ink_ui_line(card, n.title, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_ROW_DOUBLE, text_y,
                    width() - INK_ROW_DOUBLE - INK_MARGIN);
        ink_ui_line(card, n.body, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_ROW_DOUBLE,
                    text_y + ink_ui_line_height(INK_FONT_LIST), width() - INK_ROW_DOUBLE - INK_MARGIN);
        char args[64];
        snprintf(args, sizeof args, "{\"id\":%u}", (unsigned) n.id);
        ink_shell_bind(card, "shell.notify_open", args);
        count++;
        y += ink_ui_row_double() + INK_GAP_M;
    }
    if (!count) ink_ui_notice(parent, TR("还没有通知"), y + INK_GAP_L);
    N.control_count = count;
    return y;
}

static void draw_control(void) {
    lv_obj_clean(N.control);
    ink_shell_statusbar(N.control, true);
    int y = INK_STATUS_BAR + INK_GAP_S;
    int names = ink_ui_line_height(INK_FONT_LIST) + ink_ui_line_height(INK_FONT_NOTE);
    int head = names > INK_CIRCLE ? names : INK_CIRCLE;
    lv_obj_t *avatar = ink_ui_circle(N.control, "person", INK_MARGIN, y + (head - INK_CIRCLE) / 2, INK_CIRCLE,
                                     INK_ICON_LIST, INK_SURFACE_BLACK, NULL, NULL);
    ink_shell_bind(avatar, "shell.members", NULL);
    ink_ui_line(N.control, ink_member_current()->name, INK_FONT_LIST, INK_WEIGHT_BOLD,
                INK_MARGIN + INK_CIRCLE + INK_GAP, y + (head - names) / 2, width() - 2 * INK_CIRCLE - 2 * INK_GAP);
    ink_ui_line(N.control, TR("换人"), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + INK_CIRCLE + INK_GAP,
                y + (head - names) / 2 + ink_ui_line_height(INK_FONT_LIST), width() - 2 * INK_CIRCLE - 2 * INK_GAP);
    if (!child()) {
        lv_obj_t *s =
            ink_ui_circle(N.control, "gear", INK_CANVAS_W - INK_MARGIN - INK_CIRCLE, y + (head - INK_CIRCLE) / 2,
                          INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_WHITE, NULL, NULL);
        ink_shell_bind(s, "shell.settings", NULL);
    }
    y += head + INK_GAP_L;
    if (!child()) y += ink_activity_card_draw(N.control, y); /* KidControl has only light, warmth, volume and notices */
    ink_ui_slider(N.control, TR("亮度"), (int) ink_setting_get("display.light") / 10, INK_MARGIN, y, width(),
                  slider_changed, (void *) "display.light");
    y += INK_SLIDER + INK_GAP_M;
    int volume = (int)ink_setting_get("audio.volume"), top = (int)ink_setting_get("audio.child_max");
    lv_obj_t *slider = ink_ui_slider(N.control, TR("音量"), (child() && volume > top ? top : volume) / 10, INK_MARGIN, y, width(), slider_changed, (void *)"audio.volume");
    if (child()) kid_volume_mark(slider, top);
    y += INK_SLIDER + INK_GAP_M;
    if (child()) {
        ink_ui_slider(N.control, TR("暖光"), (int)ink_setting_get("display.warmth") / 10, INK_MARGIN, y, width(), slider_changed, (void *)"display.warmth");
        y += INK_SLIDER + INK_GAP_M;
    }
    if (!child()) {
        const char *const labels[] = {TR("勿扰"), TR("叫名字"), N.more ? TR("收起") : TR("更多")};
        int w = (width() - 2 * INK_GAP_M) / 3;
        for (int i = 0; i < 3; i++) {
            lv_obj_t *p = ink_ui_pill(N.control, labels[i], INK_MARGIN + i * (w + INK_GAP_M), y, w, INK_CONTROL,
                                      i == 0   ? ink_setting_get("notify.dnd")
                                      : i == 1 ? ink_setting_get("voice.handsfree") != 2
                                               : false,
                                      NULL, NULL);
            if (i == 0)
                bind_toggle(p, "notify.dnd");
            else if (i == 1)
                ink_shell_bind(p, "settings.set",
                               ink_setting_get("voice.handsfree") == 2 ? "{\"id\":\"voice.handsfree\",\"value\":0}"
                                                                       : "{\"id\":\"voice.handsfree\",\"value\":2}");
            else
                ink_shell_bind(p, "shell.more", NULL);
        }
        y += INK_CONTROL + INK_GAP_M;
        if (N.more) {
            ink_ui_slider(N.control, TR("暖光"), (int) ink_setting_get("display.warmth") / 10, INK_MARGIN, y, width(),
                          slider_changed, (void *) "display.warmth");
            y += INK_SLIDER + INK_GAP_M;
            bool plane = ink_setting_get("net.airplane");
            ink_shell_bind(ink_ui_pill(N.control, TR("飞行模式"), INK_MARGIN, y, w, INK_CONTROL, plane, NULL, NULL), "net.set_airplane",
                           plane ? "{\"enabled\":false}" : "{\"enabled\":true}");
            y += INK_CONTROL + INK_GAP_M;
        }
        char ssid[64];
        lv_obj_t *net = ink_ui_pill(N.control, wifi_line(ssid, sizeof ssid), INK_MARGIN, y, width(), INK_CONTROL,
                                    false, NULL, NULL);
        ink_shell_bind(net, "shell.network", NULL);
        y += INK_CONTROL + INK_GAP_L;
    }
    int close_y = INK_CANVAS_H - INK_CONTROL - INK_MARGIN, fits = (close_y - y) / (ink_ui_row_double() + INK_GAP_M);
    if (fits > 0) notifications(N.control, y, fits < 2 ? fits : 2);
    if (!child()) ink_shell_bind(ink_ui_pill(N.control, TR("正在做的事"), INK_MARGIN, close_y, 2 * INK_PRIMARY, INK_CONTROL, false, NULL, NULL), "shell.tasks", NULL);
    lv_obj_t *close = ink_ui_pill(N.control, TR("收起"), child() ? (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2 : INK_CANVAS_W - INK_MARGIN - INK_CIRCLE_LARGE,
                                  close_y, INK_CIRCLE_LARGE, INK_CONTROL, false, NULL, NULL);
    ink_shell_bind(close, "shell.close", NULL);
    ink_ui_hint_obj(N.control, INK_REFRESH_FLASH_UI);
}

void ink_shell_control(void) {
    if (!ink_member_space_open() || N.control) return;
    N.control = ink_ui_box(lv_layer_sys(), 0, 0, INK_CANVAS_W, INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
    lv_obj_add_flag(N.control, LV_OBJ_FLAG_CLICKABLE);
    ink_ui_overlay_add(child() ? &KID_CONTROL : &CONTROL, N.control);
    draw_control();
}
bool ink_shell_close(void) {
    if (icon_close()) return true;
    if (!N.control) return false;
    ink_ui_overlay_remove(&CONTROL); ink_ui_overlay_remove(&KID_CONTROL);
    lv_obj_delete(N.control);
    N.control = NULL;
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
    return true;
}

static void group_row(lv_obj_t *group, const char *icon, const char *title, const char *value, int row,
                      const char *action) {
    lv_obj_t *r = ink_ui_list_row(group, icon, title, value, 0, row * INK_CONTROL, width(), false, NULL, NULL);
    if (action) {
        ink_shell_bind(r, action, NULL);
        ink_ui_icon(r, "next", INK_ICON_SMALL, INK_ICON_LINE, width() - INK_READING_MARGIN,
                    (INK_CONTROL - INK_ICON_SMALL) / 2, false);
    }
}

static void wifi_password(void *ctx, const char *password) {
    if (!current_session((uint32_t) (uintptr_t) ctx)) return;
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    if (!args) return;
    ink_args_init(args);
    ink_args_str(args, "ssid", N.picked_ssid);
    ink_args_str(args, "password", password);
    ink_call_opts_t o = {.src = INK_SRC_TOUCH,
                         .reply_exec = INK_EXEC_UI,
                         .done = wifi_receipt,
                         .ctx = (void *) ((uintptr_t) N.epoch << 1),
                         .member = ink_member_current()->id};
    ink_cap_call("wifi.connect", args, &o);
    memset(args, 0, sizeof *args);
    osal_free(args);
    N.picked_ssid[0] = 0;
    ink_text_blur(0);
    draw_page_body();
}
static void wifi_picked(lv_event_t *e) {
    int index = (int) (intptr_t) lv_event_get_user_data(e);
    char args[40];
    snprintf(args, sizeof args, "{\"index\":%d}", index);
    ink_receipt_t rc;
    ink_cap_invoke_json("shell.wifi_pick", args, INK_SRC_TOUCH, 0, &rc);
}

/* Settings' first row: who is using the device, and the way to the family. */
static void draw_who(int y) {
    const ink_member_t *me = ink_member_current();
    int h = ink_ui_row_double();
    lv_obj_t *who = ink_ui_pill(N.page_body, "", INK_MARGIN, y, width(), h, true, NULL, NULL);
    ink_shell_bind(who, "shell.family", NULL);
    char first[8] = {0}, family[32];
    size_t len = 1;
    while (len < sizeof first - 1 && (me->name[len] & 0xc0) == 0x80) len++;
    memcpy(first, me->name, len);
    lv_obj_t *avatar =
        ink_ui_pill(who, first, INK_GAP_L, (h - INK_CIRCLE) / 2, INK_CIRCLE, INK_CIRCLE, false, NULL, NULL);
    lv_obj_remove_flag(avatar, LV_OBJ_FLAG_CLICKABLE);
    int people = 0, x = INK_GAP_L + INK_CIRCLE + INK_GAP;
    while (ink_member_at(people)) people++;
    snprintf(family, sizeof family, TR("家里 %d 个人"), people);
    int text_y = (h - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
    lv_obj_t *l =
        ink_ui_line(who, me->name, INK_FONT_LIST, INK_WEIGHT_BOLD, x, text_y, width() - x - INK_READING_MARGIN);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    l = ink_ui_line(who, family, INK_FONT_NOTE, INK_WEIGHT_REGULAR, x, text_y + ink_ui_line_height(INK_FONT_LIST),
                    width() - x - INK_READING_MARGIN);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    ink_ui_icon(who, "next", INK_ICON_SMALL, INK_ICON_LINE, width() - INK_READING_MARGIN, (h - INK_ICON_SMALL) / 2,
                true);
}

/* Pages do not scroll (spec section 1): a block that does not fit goes to the next page. */
static struct {
    int y, page, room;
    bool draw;
} F;
static bool block(int h) {
    if (F.y && F.y + h > F.room) {
        F.page++;
        F.y = 0;
    }
    return F.draw && F.page == N.body_page;
}
static lv_obj_t *group(int rows) {
    return ink_ui_card(N.page_body, INK_MARGIN, F.y, width(), rows * INK_CONTROL, INK_SURFACE_STATIC);
}
static int heading_h(void) {
    return ink_ui_line_height(INK_FONT_LIST) + INK_GAP_M;
}
static void heading(const char *text) {
    ink_ui_line(N.page_body, text, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_MARGIN, F.y, width());
}
static int notice_h(const char *text) {
    return ink_ui_text_height(text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, width());
}
static void bind_choices(lv_obj_t *row, int count, const char *id) {
    for (int i = 0; i < count; i++) {
        char args[96];
        snprintf(args, sizeof args, "{\"id\":\"%s\",\"value\":%d}", id, i);
        ink_shell_bind(lv_obj_get_child(row, i), "settings.set", args);
    }
}

static void body_blocks(void) {
    if (N.page_id == PAGE_SETTINGS) {
        if (block(ink_ui_row_double())) draw_who(F.y);
        F.y += ink_ui_row_double() + INK_GAP;
        if (block(2 * INK_CONTROL)) {
            char ssid[64];
            state_text("wifi.ssid", ssid, sizeof ssid);
            lv_obj_t *g = group(2);
            group_row(g, "wifi", TR("网络和 SIM 卡"), ink_setting_get("net.airplane") ? TR("飞行模式") : ssid[0] ? ssid : TR("还没有连接"), 0, "shell.network");
            group_row(g, "bluetooth", TR("蓝牙"), integer("bt.connected") ? TR("已连接") : TR("没有连接"), 1, "shell.bluetooth");
        }
        F.y += 2 * INK_CONTROL + INK_GAP;
        if (block(3 * INK_CONTROL)) {
            char light[32], volume[32];
            snprintf(light, sizeof light, TR("亮度 %lld"), (long long) ink_setting_get("display.light") / 10);
            snprintf(volume, sizeof volume, TR("音量 %lld"), (long long) ink_setting_get("audio.volume") / 10);
            lv_obj_t *g = group(3);
            group_row(g, "sun", TR("显示和亮度"), light, 0, "shell.display");
            group_row(g, "volume", TR("声音"), volume, 1, "shell.sound");
            int wake = (int) ink_setting_get("voice.handsfree");
            group_row(g, "wave", TR("语音和 AI"),
                      wake == 2   ? TR("叫名字已关闭")
                      : wake == 1 ? TR("一直能叫名字")
                                  : TR("使用时能叫名字"),
                      2, "shell.voice");
        }
        F.y += 3 * INK_CONTROL + INK_GAP;
        if (block(3 * INK_CONTROL)) {
            lv_obj_t *g = group(3);
            char lock_after[32]; snprintf(lock_after, sizeof lock_after, TR("%lld 分钟"), (long long)ink_setting_get("power.lock_after_s") / 60);
            group_row(g, "lock", TR("锁屏和壁纸"), lock_after, 0, "shell.wallpaper");
            group_row(g, "storage", TR("存储和备份"), "", 1, "shell.storage");
            group_row(g, "info", TR("关于和更新"), "", 2, "shell.about");
        }
    } else if (N.page_id == PAGE_CHANNELS) {
        const char *note = TR(CHANNEL_NOTE);
        int h = notice_h(note);
        if (block(h)) ink_ui_notice(N.page_body, note, F.y);
        F.y += h + INK_GAP_L;
        if (F.draw) N.channel_first = N.channel_end = -1;
        const ink_notify_channel_t *ch;
        for (int i = 0; (ch = ink_notify_channel_at(i)); i++) {
            if (block(INK_CONTROL)) {
                bool on = ink_notify_channel_on(ch->id);
                char args[96];
                snprintf(args, sizeof args, "{\"id\":\"%s\",\"enabled\":%s}", ch->id, on ? "false" : "true");
                ink_shell_bind(ink_ui_toggle(N.page_body, TR(ch->title), on, INK_MARGIN, F.y, width(), NULL, NULL),
                    "notify.set_channel", args);
                if (N.channel_first < 0) N.channel_first = i;
                N.channel_end = i + 1;
            }
            F.y += INK_CONTROL + INK_GAP_M;
        }
    } else if (N.page_id == PAGE_DISPLAY) {
        if (block(INK_SLIDER))
            ink_ui_slider(N.page_body, TR("亮度"), (int) ink_setting_get("display.light") / 10, INK_MARGIN, F.y, width(),
                          slider_changed, (void *) "display.light");
        F.y += INK_SLIDER + INK_GAP_M;
        if (block(INK_SLIDER))
            ink_ui_slider(N.page_body, TR("暖光"), (int) ink_setting_get("display.warmth") / 10, INK_MARGIN, F.y, width(),
                          slider_changed, (void *) "display.warmth");
        F.y += INK_SLIDER + INK_GAP_L;
        const char *const sample_text = TR("天黑以后，阿木背起邮包。");
        int sample_h = 2 * INK_GAP_L +
                       ink_ui_text_height(sample_text, INK_FONT_BODY, INK_WEIGHT_REGULAR, width() - 2 * INK_MARGIN);
        int h = heading_h() + INK_SEGMENT + INK_GAP + sample_h;
        if (block(h)) {
            heading(TR("字号"));
            const char *const labels[] = {TR("小"), TR("中"), TR("大"), TR("特大")};
            lv_obj_t *seg = ink_ui_segments(N.page_body, labels, 4, (int) ink_setting_get("display.font_size"),
                                            INK_MARGIN, F.y + heading_h(), width(), NULL, NULL);
            bind_choices(seg, 4, "display.font_size");
            lv_obj_t *sample = ink_ui_card(N.page_body, INK_MARGIN, F.y + heading_h() + INK_SEGMENT + INK_GAP, width(),
                                           sample_h, INK_SURFACE_STATIC);
            ink_ui_label(sample, sample_text, INK_FONT_BODY, INK_WEIGHT_REGULAR, INK_MARGIN, INK_GAP_L,
                         width() - 2 * INK_MARGIN);
        }
        F.y += h + INK_GAP_L;
        if (block(INK_CONTROL)) {
            lv_obj_t *refresh =
                ink_ui_pill(N.page_body, TR("刷新屏幕"), INK_MARGIN, F.y, width(), INK_CONTROL, false, NULL, NULL);
            ink_shell_bind(refresh, "system.full_refresh", NULL);
        }
        F.y += INK_CONTROL + INK_GAP;
        if (block(INK_CONTROL)) {
            static const int seconds[] = {60, 300, 600, 1800, 3600};
            int64_t current = ink_setting_get("power.lock_after_s");
            int next = seconds[0];
            for (unsigned i = 0; i < sizeof seconds / sizeof *seconds; i++)
                if (seconds[i] > current) {
                    next = seconds[i];
                    break;
                }
            char label[64], args[96];
            snprintf(label, sizeof label, TR("自动锁屏 %lld 分钟"), (long long) current / 60);
            snprintf(args, sizeof args, "{\"id\":\"power.lock_after_s\",\"value\":%d}", next);
            lv_obj_t *lock = ink_ui_pill(N.page_body, label, INK_MARGIN, F.y, width(), INK_CONTROL, false, NULL, NULL);
            ink_shell_bind(lock, "settings.set", args);
        }
        F.y += INK_CONTROL + INK_GAP;
        if (block(INK_CONTROL))
            group_row(group(1), "grid", TR("今日屏放什么"), "", 0, "shell.today_edit");
        F.y += INK_CONTROL;
    } else if (N.page_id == PAGE_SOUND) {
        const char *const about = TR("听书、AI 和提示音用这个音量");
        int h = INK_SLIDER + INK_GAP_S + notice_h(about);
        if (block(h)) {
            ink_ui_slider(N.page_body, TR("音量"), (int) ink_setting_get("audio.volume") / 10, INK_MARGIN, F.y, width(),
                          slider_changed, (void *) "audio.volume");
            ink_ui_notice(N.page_body, about, F.y + INK_SLIDER + INK_GAP_S);
        }
        F.y += h + INK_GAP_L;
        h = heading_h() + 2 * INK_CONTROL;
        if (block(h)) {
            heading(TR("声音从哪里出来"));
            F.y += heading_h();
            char route[32];
            state_text("audio.route", route, sizeof route);
            lv_obj_t *g = group(2);
            group_row(g, "volume", TR("喇叭"), !strcmp(route, "speaker") ? TR("当前") : "", 0, "bt.disconnect");
            group_row(g, "headphones", TR("蓝牙耳机"), !strcmp(route, "bt_headset") ? TR("当前") : TR("没有连接"), 1, "shell.bluetooth");
            F.y -= heading_h();
        }
        F.y += h + INK_GAP_L;
        if (block(INK_CONTROL)) {
            lv_obj_t *toggle =
                ink_ui_toggle(N.page_body, TR("勿扰"), ink_setting_get("notify.dnd"), INK_MARGIN, F.y, width(), NULL, NULL);
            bind_toggle(toggle, "notify.dnd");
        }
        F.y += INK_CONTROL + INK_GAP_L;
        h = heading_h() + 3 * INK_CONTROL;
        if (block(h)) {
            heading(TR("提示"));
            F.y += heading_h();
            lv_obj_t *g = group(3);
            const char *ids[] = {"audio.key_tone", "audio.vibration"}, *labels[] = {TR_NOOP("按键提示音"), TR_NOOP("振动")};
            for (int i = 0; i < 2; i++) {
                ink_ui_icon(g, i ? "wave" : "bell", INK_ICON_LIST, INK_ICON_LINE, INK_GAP_L, i * INK_CONTROL + (INK_CONTROL - INK_ICON_LIST) / 2, false);
                lv_obj_t *toggle = ink_ui_toggle(g, TR(labels[i]), ink_setting_get(ids[i]), INK_ROW_DOUBLE, i * INK_CONTROL, width() - INK_ROW_DOUBLE - INK_GAP_L, NULL, NULL);
                lv_obj_set_style_bg_opa(toggle, LV_OPA_TRANSP, 0);
                bind_toggle(toggle, ids[i]);
            }
            const char *levels[] = {TR("弱"), TR("中"), TR("强")};
            int level = (int)ink_setting_get("audio.vibration_level");
            char args[80];
            snprintf(args, sizeof args, "{\"id\":\"audio.vibration_level\",\"value\":%d}", (level + 1) % 3);
            lv_obj_t *row = ink_ui_list_row(g, "wave", TR("振动强度"), levels[level], 0, 2 * INK_CONTROL, width(), false, NULL, NULL);
            ink_shell_bind(row, "settings.set", args);
            F.y -= heading_h();
        }
        F.y += h + INK_GAP_L;
        h = heading_h() + 2 * INK_CONTROL;
        if (block(h)) {
            heading(TR("铃声"));
            F.y += heading_h();
            lv_obj_t *g = group(2);
            const char *ids[] = {"audio.call_ring", "audio.alarm_ring"}, *labels[] = {TR_NOOP("来电"), TR_NOOP("闹钟")};
            const char *names[] = {TR_NOOP("清音"), TR_NOOP("双音"), TR_NOOP("柔音")};
            for (int i = 0; i < 2; i++) {
                int v = (int)ink_setting_get(ids[i]); char value[64], args[96];
                snprintf(value, sizeof value, TR("%s · 点按换音试听"), TR(names[v]));
                lv_obj_t *row = ink_ui_list_row(g, i ? "clock" : "phone", TR(labels[i]), value, 0, i * INK_CONTROL, width(), false, NULL, NULL);
                snprintf(args, sizeof args, "{\"id\":\"%s\",\"value\":%d}", ids[i], (v + 1) % 3);
                ink_shell_bind(row, "settings.set", args);
            }
            F.y -= heading_h();
        }
        F.y += h + INK_GAP_L;
        if (block(INK_CONTROL)) group_row(group(1), "bell", TR("通知"), "", 0, "shell.notify_settings");
        F.y += INK_CONTROL;
    } else if (N.page_id == PAGE_KNOWN) {
        char connected[33], state[24];
        state_text("wifi.ssid", connected, sizeof connected);
        state_text("wifi.state", state, sizeof state);
        if (N.known_count <= 0) {
            const char *text = TR(N.known_count == -1 ? TR_NOOP("正在读取网络") : N.known_count == -2 ? TR_NOOP("网络列表暂时读不出来") : TR_NOOP("还没有已存的网络"));
            if (block(notice_h(text))) ink_ui_notice(N.page_body, text, F.y);
        }
        for (int i = 0; i < N.known_count; i++) {
            int row = ink_ui_row_double(), h = row + INK_CONTROL;
            if (block(h)) {
                bool same = !strcmp(connected, N.known_ssid[i]);
                const char *status = same && !strcmp(state, "connected") ? TR("已连接") : same && !strcmp(state, "joining") ? TR("正在连接网络") : TR("已存");
                ink_ui_list_row(N.page_body, "wifi", N.known_ssid[i], status, INK_MARGIN, F.y, width(), false, NULL, NULL);
                char args[224]; ink_jw_t w; ink_jw_init(&w, args, sizeof args);
                ink_jw_raw(&w, "{\"ssid\":"); ink_jw_str(&w, N.known_ssid[i]); ink_jw_raw(&w, "}");
                lv_obj_t *join = ink_ui_pill(N.page_body, TR("连接"), INK_MARGIN, F.y + row, (width() - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL);
                ink_shell_bind(join, "wifi.connect", args);
                if (!ink_setting_get("wifi.enabled") || (same && (!strcmp(state, "connected") || !strcmp(state, "joining")))) lv_obj_add_state(join, LV_STATE_DISABLED);
                ink_shell_bind(ink_ui_pill(N.page_body, TR("忘掉"), INK_MARGIN + (width() + INK_GAP) / 2, F.y + row,
                    (width() - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL), "wifi.forget", args);
            }
            F.y += h + INK_GAP_L;
        }
    } else if (N.page_id == PAGE_NETWORK) {
        if (block(2 * INK_CONTROL)) {
            lv_obj_t *g = group(2);
            const char *labels[] = {TR("飞行模式"), "Wi-Fi"}, *icons[] = {"plane", "wifi"};
            bool plane = ink_setting_get("net.airplane");
            for (int i = 0; i < 2; i++) {
                ink_ui_icon(g, icons[i], INK_ICON_LIST, INK_ICON_LINE, INK_GAP_L, i * INK_CONTROL + (INK_CONTROL - INK_ICON_LIST) / 2, false);
                lv_obj_t *toggle = ink_ui_toggle(g, labels[i], i ? ink_setting_get("wifi.enabled") : plane, INK_ROW_DOUBLE, i * INK_CONTROL, width() - INK_ROW_DOUBLE - INK_GAP_L, NULL, NULL);
                lv_obj_set_style_bg_opa(toggle, LV_OPA_TRANSP, 0);
                if (i) bind_toggle(toggle, "wifi.enabled");
                else ink_shell_bind(toggle, "net.set_airplane", plane ? "{\"enabled\":false}" : "{\"enabled\":true}");
            }
        }
        F.y += 2 * INK_CONTROL + INK_GAP_L;
        const char *const about = TR("自动：有 Wi-Fi 用 Wi-Fi，没有就用 4G");
        int h = heading_h() + INK_SEGMENT + INK_GAP_M + notice_h(about);
        if (block(h)) {
            heading(TR("上网方式"));
            const char *const labels[] = {TR("自动"), "Wi-Fi", "4G"};
            lv_obj_t *seg = ink_ui_segments(N.page_body, labels, 3, (int) ink_setting_get("net.prefer"), INK_MARGIN,
                                            F.y + heading_h(), width(), NULL, NULL);
            bind_choices(seg, 3, "net.prefer");
            ink_ui_notice(N.page_body, about, F.y + heading_h() + INK_SEGMENT + INK_GAP_M);
        }
        F.y += h + INK_GAP_L;
        char ssid[64];
        const char *const connected = wifi_line(ssid, sizeof ssid);
        h = heading_h() + notice_h(connected);
        if (block(h)) {
            heading("Wi-Fi");
            ink_ui_notice(N.page_body, connected, F.y + heading_h());
        }
        F.y += h + INK_GAP_L;
        if (ssid[0]) {
            if (block(INK_CONTROL)) {
                char args[256]; ink_jw_t w; ink_jw_init(&w, args, sizeof args);
                ink_jw_raw(&w, "{\"ssid\":"); ink_jw_str(&w, ssid); ink_jw_raw(&w, "}");
                ink_shell_bind(ink_ui_pill(N.page_body, TR("忘掉这个网络"), INK_MARGIN, F.y, width(), INK_CONTROL,
                    false, NULL, NULL), "wifi.forget", args);
            }
            N.wifi_forget_page = F.page;
            F.y += INK_CONTROL + INK_GAP_M;
        }
        if (N.picked_ssid[0]) {
            if (block(ink_textfield_height(INK_FIELD_PASSWORD))) {
                ink_text_cfg_t cfg = {.type = INK_TEXT_PASSWORD,
                                      .enter = INK_ENTER_DONE,
                                      .max = 64,
                                      .hint = TR("输入网络密码"),
                                      .ai_hidden = true};
                lv_obj_t *password_field =
                    ink_textfield_create(N.page_body, &cfg, wifi_password, (void *) (uintptr_t) N.epoch, false);
                lv_obj_set_pos(password_field, ink_ui_px(INK_MARGIN), ink_ui_px(F.y));
                ink_textfield_style(password_field, INK_FIELD_PASSWORD, N.picked_ssid);
                ink_textfield_short_focus(password_field);
            }
            return;
        }
        if (block(INK_CONTROL)) {
            lv_obj_t *scan = ink_ui_pill(N.page_body, N.scan_busy ? TR("正在找附近的 Wi-Fi") : TR("找附近的 Wi-Fi"), INK_MARGIN,
                                         F.y, (width() - INK_GAP) / 2, INK_CONTROL, N.scan_busy, NULL, NULL);
            ink_shell_bind(scan, "wifi.scan", NULL);
            ink_shell_bind(ink_ui_pill(N.page_body, TR("设置设备"), INK_MARGIN + (width() + INK_GAP) / 2, F.y,
                (width() - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL), "shell.launch", "{\"id\":\"boot\"}");
        }
        F.y += INK_CONTROL + INK_GAP_M;
        if (block(INK_CONTROL)) group_row(group(1), "wifi", TR("已存的网络"), "", 0, "shell.known_networks");
        F.y += INK_CONTROL + INK_GAP_M;
        for (int i = N.wifi_page * 2; i < N.wifi_count && i < (N.wifi_page + 1) * 2; i++) {
            if (block(INK_CONTROL))
                ink_ui_pill(N.page_body, N.wifi_ssid[i], INK_MARGIN, F.y, width(), INK_CONTROL, false, wifi_picked,
                            (void *) (intptr_t) i);
            F.y += INK_CONTROL + INK_GAP_S;
        }
        if (N.wifi_count > 2) {
            if (block(INK_CONTROL)) {
                lv_obj_t *prev = ink_ui_pill(N.page_body, TR("上一组"), INK_MARGIN, F.y, (width() - INK_GAP) / 2,
                                             INK_CONTROL, false, NULL, NULL);
                if (N.wifi_page) ink_shell_bind(prev, "shell.wifi_prev", NULL);
                lv_obj_t *next = ink_ui_pill(N.page_body, TR("下一组"), INK_MARGIN + (width() + INK_GAP) / 2, F.y,
                                             (width() - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL);
                if ((N.wifi_page + 1) * 2 < N.wifi_count) ink_shell_bind(next, "shell.wifi_next", NULL);
            }
            F.y += INK_CONTROL + INK_GAP_M;
        }
        if (!N.wifi_count) {
            const char *const none = TR("还没有附近的网络列表");
            if (block(notice_h(none))) ink_ui_notice(N.page_body, none, F.y);
            F.y += notice_h(none) + INK_GAP_L;
        }
        const char *const sim_state = integer("cell.config_readonly") ? TR("暂不能更改 SIM 卡") : integer("cell.registered") ? TR("4G 在线，切卡不用重启") : TR("4G 还没有连上");
        h = heading_h() + INK_SEGMENT + INK_GAP_L + notice_h(sim_state);
        if (block(h)) {
            heading(TR("SIM 卡"));
            const char *const slots[] = {TR("外置卡"), TR("内置卡")};
            lv_obj_t *sim = ink_ui_segments(N.page_body, slots, 2, (int) ink_setting_get("cell.sim_slot"), INK_MARGIN,
                                            F.y + heading_h(), width(), NULL, NULL);
            if (integer("cell.config_readonly")) lv_obj_add_state(sim, LV_STATE_DISABLED);
            else bind_choices(sim, 2, "cell.sim_slot");
            ink_ui_notice(N.page_body, sim_state, F.y + heading_h() + INK_SEGMENT + INK_GAP_L);
        }
        F.y += h;
        if (block(INK_CONTROL + INK_GAP_L)) group_row(group(1), "phone", TR("移动网络"), "", 0, "shell.mobile");
        F.y += INK_CONTROL + INK_GAP_L;
    }
}

static void body_page_clicked(lv_event_t *e) {
    ink_receipt_t rc;
    ink_cap_invoke(lv_event_get_user_data(e) == (void *) (intptr_t) 1 ? "shell.page_next" : "shell.page_prev", NULL,
                   INK_SRC_TOUCH, 0, &rc);
}

static void notify_bind(lv_obj_t *slot, size_t index, ink_ui_field_spec_t *field,
                        ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    if (!(index % (size_t)N.notify_capacity)) memset(N.notify_ids, 0, sizeof N.notify_ids);
    unsigned at = index % 6;
    ink_db_cursor_t cursor = {0};
    for (size_t i = 0; i <= index; i++) if (ink_notify_next(&cursor, &N.notify_items[at]) != 1) return;
    ink_notify_view_t *n = &N.notify_items[at];
    snprintf(N.notify_ids[at], sizeof N.notify_ids[at], "notify:%u", (unsigned)n->id);
    snprintf(N.notify_args[at], sizeof N.notify_args[at], "{\"id\":%u}", (unsigned)n->id);
    *field = (ink_ui_field_spec_t){.id = N.notify_ids[at], .role = "listitem", .label = n->title,
        .value = n->body, .object = {N.notify_ids[at], (uint32_t)n->read}};
    *action = (ink_ui_action_spec_t){.id = N.notify_ids[at], .label = TR("打开通知"), .capability = "shell.notify_open",
        .args_json = N.notify_args[at], .object = field->object, .enabled = true};
    int h = ink_ui_row_double(), text_y = (h - ink_ui_line_height(INK_FONT_LIST) - ink_ui_line_height(INK_FONT_NOTE)) / 2;
    int delete_w = ink_ui_text_width(TR("删除"), INK_FONT_LIST, INK_WEIGHT_MEDIUM) + 2 * INK_GAP;
    if (delete_w < INK_PRIMARY) delete_w = INK_PRIMARY;
    int text_w = width() - INK_ROW_DOUBLE - delete_w - 2 * INK_GAP_L;
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_line(slot, "", INK_FONT_LIST, INK_WEIGHT_BOLD, INK_ROW_DOUBLE, text_y, text_w);
        ink_ui_line(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_ROW_DOUBLE,
            text_y + ink_ui_line_height(INK_FONT_LIST), text_w);
    }
    lv_label_set_text(lv_obj_get_child(slot, 0), n->title); lv_label_set_text(lv_obj_get_child(slot, 1), n->body);
    while (lv_obj_get_child_count(slot) > 2) lv_obj_delete(lv_obj_get_child(slot, 2));
    ink_ui_circle(slot, !strcmp(n->channel, "message") ? "message" : "bell", INK_GAP, (h - INK_CIRCLE) / 2,
        INK_CIRCLE, INK_ICON_LIST, INK_SURFACE_STATIC, NULL, NULL);
    ink_shell_bind(ink_ui_pill(slot, TR("删除"), width() - delete_w - INK_GAP_M, (h - INK_CONTROL) / 2,
        delete_w, INK_CONTROL, false, NULL, NULL), "notify.dismiss", N.notify_args[at]);
}
static void draw_page_body(void) {
    if (N.page_id == PAGE_NOTIFICATIONS) {
        const ink_ui_action_spec_t actions[] = {
            {.id = "prev", .label = TR("上一页"), .capability = "shell.notify_prev"},
            {.id = "next", .label = TR("下一页"), .capability = "shell.notify_next"},
            {.id = "read_all", .label = TR("全部已读"), .capability = "notify.read", .args_json = "{\"id\":0}", .enabled = true}};
        const ink_ui_page_spec_t page = {.id = "Notifications", .title = TR("通知"), .object = {"notify:inbox", 0},
            .actions = actions, .action_count = 3};
        if (!N.notify_list) {
            int fits = (INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_GAP_M - 2 * (INK_CONTROL + INK_GAP)) /
                (ink_ui_row_double() + INK_GAP_M);
            N.notify_capacity = fits < 4 ? fits : 4; /* Keep the actions inside the production AI snapshot. */
            N.notify_list = ink_ui_list_create(N.page_body, &page, &N.notify_position, N.notify_capacity, 1,
                ink_ui_row_double(), TR("还没有通知"), notify_bind, NULL);
            lv_obj_t *read = ink_ui_pill(N.page_body, TR("全部已读"), INK_MARGIN,
                INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_CONTROL - INK_GAP_M,
                (width() - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL);
            ink_shell_bind(read, "notify.read", "{\"id\":0}");
            ink_shell_bind(ink_ui_pill(N.page_body, TR("全部清除"), INK_MARGIN + (width() + INK_GAP) / 2,
                INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_CONTROL - INK_GAP_M,
                (width() - INK_GAP) / 2, INK_CONTROL, false, NULL, NULL), "notify.clear", NULL);
        }
        ink_db_cursor_t cursor = {0}; ink_notify_view_t n; size_t total = 0;
        while (ink_notify_next(&cursor, &n) == 1) total++;
        /* the rows' ids are filled again as rows are bound; an empty list binds none, so clear them here */
        memset(N.notify_ids, 0, sizeof N.notify_ids);
        if (N.notify_list) ink_ui_list_refresh(N.notify_list, total);
        return;
    }
    lv_obj_clean(N.page_body);
    if (N.page_id != PAGE_NOTIFICATIONS) {
        int room = INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_GAP_M;
        F.y = F.page = 0;
        F.room = room;
        F.draw = false;
        body_blocks();
        if (F.page) { /* room for the page turner */
            room -= INK_CONTROL + INK_GAP;
            F.y = F.page = 0;
            F.room = room;
            body_blocks();
        }
        N.body_pages = F.page + 1;
        if (N.body_page >= N.body_pages) N.body_page = N.body_pages - 1;
        F.y = F.page = 0;
        F.room = room;
        F.draw = true;
        body_blocks();
        if (N.body_pages > 1)
            ink_ui_pagebar(N.page_body, N.body_page + 1, N.body_pages, room + INK_GAP, body_page_clicked);
        ink_ui_hint_obj(N.page_body, INK_REFRESH_UI);
        return;
    }
}

static void create_page(lv_obj_t *screen, void *arg) {
    N.page = screen;
    N.page_id = (page_t) (intptr_t) arg;
    N.body_page = 0;
    static const char *const titles[] = {TR_NOOP("设置"), TR_NOOP("显示和亮度"), TR_NOOP("声音"), TR_NOOP("网络和 SIM 卡"), TR_NOOP("通知"), TR_NOOP("通知"), TR_NOOP("已存的网络")};
    _Static_assert(sizeof titles / sizeof *titles == sizeof PAGES / sizeof *PAGES, "page titles");
    ink_shell_statusbar(screen, true);
    ink_shell_topbar(screen, TR(titles[N.page_id]));
    N.page_body = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS, INK_CANVAS_W,
                             INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS, 0, INK_SURFACE_WHITE, false);
    if (N.page_id == PAGE_KNOWN) { N.known_count = -1; N.known_epoch++; }
    draw_page_body();
    if (N.page_id == PAGE_KNOWN) known_load(NULL, NULL, NULL);
}
static void destroy_page(void) {
    if (N.page_id == PAGE_KNOWN) N.known_epoch++;
    if (N.page_id == PAGE_NETWORK) {
        N.picked_ssid[0] = 0;
        ink_text_blur(0);
    }
    N.page = N.page_body = NULL;
    N.notify_list = NULL;
}
static void turn_body(int delta) {
    int page = N.body_page + delta;
    if (!N.page_body || page < 0 || page >= N.body_pages) return;
    N.body_page = page;
    ink_ui_content_changed();
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
    draw_page_body();
}
static bool page_key(ink_key_t key, bool lng) {
    if (N.notify_list) return ink_ui_list_key(N.notify_list, key, lng);
    if (lng || N.body_pages < 2 || (key != INK_KEY_NEXT && key != INK_KEY_PREV)) return false;
    turn_body(key == INK_KEY_NEXT ? 1 : -1);
    return true;
}
static int page_snapshot(char *buf, size_t cap) {
    if (!current_session(N.epoch)) return snprintf(buf, cap, "空间锁定或正在切换；AI 无权读取。");
    if (N.notify_list) {
        int n = ink_ui_list_snapshot(N.notify_list, buf, cap);
        if (n <= 0) return n;
        ink_jw_t w = {.buf = buf, .cap = cap, .len = (size_t)n - 1};
        ink_jw_raw(&w, ",\"clear_action\":{\"label\":"); ink_jw_str(&w, TR("全部清除"));
        ink_jw_raw(&w, ",\"capability\":\"notify.clear\",\"args_json\":\"{}\"},\"dismiss_actions\":[");
        bool comma = false;
        for (unsigned i = 0; i < sizeof N.notify_ids / sizeof *N.notify_ids; i++) if (N.notify_ids[i][0]) {
            ink_jw_raw(&w, comma ? ",{\"object\":" : "{\"object\":");
            ink_jw_str(&w, N.notify_ids[i]);
            ink_jw_raw(&w, ",\"label\":"); ink_jw_str(&w, TR("删除"));
            ink_jw_raw(&w, ",\"capability\":\"notify.dismiss\",\"args_json\":"); ink_jw_str(&w, N.notify_args[i]);
            ink_jw_raw(&w, "}"); comma = true;
        }
        ink_jw_raw(&w, "]}");
        return w.overflow ? -1 : (int)w.len;
    }
    if (N.page_id == PAGE_CHANNELS) {
        ink_jw_t w; ink_jw_init(&w, buf, cap);
        ink_jw_raw(&w, "{\"page\":\"SetNotifications\",\"page_number\":"); ink_jw_int(&w, N.body_page + 1);
        ink_jw_raw(&w, ",\"pages\":"); ink_jw_int(&w, N.body_pages);
        ink_jw_raw(&w, ",\"note\":"); ink_jw_str(&w, TR(CHANNEL_NOTE));
        ink_jw_raw(&w, ",\"navigation\":[\"shell.page_prev\",\"shell.page_next\",\"shell.back\"],\"channels\":[");
        for (int i = N.channel_first; i >= 0 && i < N.channel_end; i++) {
            const ink_notify_channel_t *ch = ink_notify_channel_at(i);
            bool on = ink_notify_channel_on(ch->id);
            char args[96]; snprintf(args, sizeof args, "{\"id\":\"%s\",\"enabled\":%s}", ch->id, on ? "false" : "true");
            ink_jw_raw(&w, i == N.channel_first ? "{\"id\":" : ",{\"id\":"); ink_jw_str(&w, ch->id);
            ink_jw_raw(&w, ",\"label\":"); ink_jw_str(&w, TR(ch->title));
            ink_jw_raw(&w, on ? ",\"enabled\":true" : ",\"enabled\":false");
            ink_jw_raw(&w, ",\"capability\":\"notify.set_channel\",\"args_json\":"); ink_jw_str(&w, args);
            ink_jw_raw(&w, "}");
        }
        ink_jw_raw(&w, "]}"); return w.overflow ? -1 : (int)w.len;
    }
    if (N.page_id == PAGE_KNOWN) {
        char ssid[33], state[24]; state_text("wifi.ssid", ssid, sizeof ssid); state_text("wifi.state", state, sizeof state);
        int n = snprintf(buf, cap, "KnownNetworks；已存=%d；第 %d 页，共 %d 页；", N.known_count, N.body_page + 1, N.body_pages);
        for (int i = 0; i < N.known_count && n >= 0 && (size_t)n < cap; i++)
            n += snprintf(buf + n, cap - (size_t)n, "%s=%s；", N.known_ssid[i], !strcmp(ssid, N.known_ssid[i]) ? state : "saved");
        if (n >= 0 && (size_t)n < cap) n += snprintf(buf + n, cap - (size_t)n, "动作：wifi.connect(ssid)、wifi.forget(ssid)、system.undo(object,revision)、shell.page_next、shell.page_prev、shell.back。");
        return n < 0 ? 0 : (size_t)n < cap ? n : (int)cap - 1;
    }
    if (N.page_id == PAGE_SOUND)
        return snprintf(buf, cap, TR("SetSound；音量=%lld；勿扰=%lld；按键提示音(audio.key_tone)=%lld；振动(audio.vibration)=%lld；振动强度(audio.vibration_level)=%lld（0弱35ms、1中50ms、2强70ms）；来电铃声(audio.call_ring)=%lld；闹钟铃声(audio.alarm_ring)=%lld；铃声：0清音、1双音、2柔音；settings.get(id)、settings.set(id,value)；通知：shell.notify_settings；shell.page_next、shell.page_prev、shell.back。"), (long long)ink_setting_get("audio.volume"), (long long)ink_setting_get("notify.dnd"), (long long)ink_setting_get("audio.key_tone"), (long long)ink_setting_get("audio.vibration"), (long long)ink_setting_get("audio.vibration_level"), (long long)ink_setting_get("audio.call_ring"), (long long)ink_setting_get("audio.alarm_ring"));
    int n = snprintf(buf, cap, "%s；当前=%s；亮度=%lld；暖光=%lld；音量=%lld；网络偏好=%lld；通知=%d。",
                     PAGES[N.page_id].name, ink_member_current()->name, (long long) ink_setting_get("display.light"),
                     (long long) ink_setting_get("display.warmth"), (long long) ink_setting_get("audio.volume"),
                     (long long) ink_setting_get("net.prefer"), ink_notify_unread());
    if (N.page_id != PAGE_NOTIFICATIONS && n >= 0 && (size_t) n < cap)
        n += snprintf(buf + n, cap - (size_t) n, "字号=%lld；锁屏画面=%lld；自动锁屏秒=%lld；拿起亮屏(power.lift_wake)=%lld；勿扰=%lld；叫名字=%lld；语言=%s；",
                      (long long) ink_setting_get("display.font_size"),
                      (long long) ink_setting_get("display.lock_style"),
                      (long long) ink_setting_get("power.lock_after_s"), (long long) ink_setting_get("power.lift_wake"),
                      (long long) ink_setting_get("notify.dnd"),
                      (long long) ink_setting_get("voice.handsfree"), ink_lang_code());
    if (N.body_pages > 1 && N.page_id != PAGE_NOTIFICATIONS && n >= 0 && (size_t) n < cap)
        n += snprintf(buf + n, cap - (size_t) n, "这一组第 %d 页，共 %d 页；动作：shell.page_next、shell.page_prev；",
                      N.body_page + 1, N.body_pages);
    if (N.page_id == PAGE_NETWORK && n >= 0 && (size_t) n < cap) {
        char ssid[64];
        state_text("wifi.ssid", ssid, sizeof ssid);
        n += snprintf(buf + n, cap - (size_t) n, "飞行模式=%lld；Wi-Fi=%lld；已连网络=%s；选中网络=%s；网络组=%d；SIM=%lld；4G在线=%lld；",
                      (long long) ink_setting_get("net.airplane"), (long long) ink_setting_get("wifi.enabled"), ssid,
                      N.picked_ssid, N.wifi_page + 1, (long long) ink_setting_get("cell.sim_slot"),
                      (long long) integer("cell.registered"));
        if (integer("cell.config_readonly") && n >= 0 && (size_t)n < cap)
            n += snprintf(buf + n, cap - (size_t)n, "SIM 卡暂不能更改；");
        for (int i = N.wifi_page * 2; i < N.wifi_count && i < (N.wifi_page + 1) * 2 && n >= 0 && (size_t) n < cap; i++)
            n += snprintf(buf + n, cap - (size_t) n, "网络[%d]=%s；", i, N.wifi_ssid[i]);
        if (ssid[0] && N.body_page == N.wifi_forget_page && n >= 0 && (size_t)n < cap) {
            char args[256]; ink_jw_t w; ink_jw_init(&w, args, sizeof args);
            ink_jw_raw(&w, "{\"ssid\":"); ink_jw_str(&w, ssid); ink_jw_raw(&w, "}");
            n += snprintf(buf + n, cap - (size_t)n, "忘掉这个网络：wifi.forget(%s)；", args);
        }
    }
    const char *actions =
        N.page_id == PAGE_NETWORK ? "动作：shell.known_networks、net.set_airplane、settings.set、wifi.scan、wifi.list、shell.back、shell.control、shell.wifi_"
                                      "next、shell.wifi_prev、shell.wifi_pick、shell.mobile；设置设备：shell.launch(id=boot)。"
                                    : N.page_id == PAGE_DISPLAY
                                        ? "动作：settings.set、shell.today_edit、shell.back、shell.control。"
                                    : "动作：settings.set、shell.back、shell.control。";
    if (n >= 0 && (size_t) n < cap) n += snprintf(buf + n, cap - (size_t) n, "%s", actions);
    return n < 0 ? 0 : (size_t) n < cap ? n : (int) cap - 1;
}

void ink_shell_open_page(const char *name) {
    if (!strcmp(name, "SetBluetooth")) { ink_bluetooth_page_open(); return; }
    ink_settings_page_open(name);
    for (unsigned i = 0; i < sizeof PAGES / sizeof *PAGES; i++)
        if (!strcmp(name, PAGES[i].name)) {
            if (child() && i != PAGE_NOTIFICATIONS) return;
            ink_shell_close();
            ink_ui_push(&PAGES[i], (void *) (intptr_t) i);
            return;
        }
    ink_settings_family_open(name);
}

const char *ink_shell_board(void) {
    if (N.control) return child() ? "KidControl" : "Control";
    if (N.home && !strcmp(ink_ui_page_name(), "home")) {
        if (child()) return N.home_page == 0 ? "KidToday" : N.home_page == 1 ? "KidHome" : "KidApps";
        return N.home_page == 0 ? "Today" : N.home_page == 1 ? "Main" : "Apps";
    }
    return ink_ui_top_name();
}

static void cap_nav(ink_call_t *c) {
    if (!strcmp(c->cap->id, "shell.control"))
        ink_shell_control();
    else if (!strcmp(c->cap->id, "shell.lock")) {
        ink_shell_close();
        ink_power_lock();
    } else if (!strcmp(c->cap->id, "shell.close"))
        ink_shell_close();
    else if (!strcmp(c->cap->id, "shell.back")) {
        const ink_activity_t *a = ink_ui_locked_activity();
        if (!(a && a->on_key && a->on_key(INK_KEY_BACK, false)) && !ink_shell_close()) ink_ui_pop();
    } else if (!strcmp(c->cap->id, "shell.members"))
        ink_shell_members_open(false);
    else if (!strcmp(c->cap->id, "shell.more")) {
        N.more = !N.more;
        if (N.control) draw_control();
    } else if (!strcmp(c->cap->id, "home.next"))
        turn(1);
    else if (!strcmp(c->cap->id, "home.prev"))
        turn(-1);
    else if (!strcmp(c->cap->id, "shell.search")) {
        ink_args_get_str(&c->args, "query", N.search, sizeof N.search);
        N.apps_page = 0;
        ink_text_blur(0);
        if (N.content) {
            N.home_page = 2;
            ink_ui_content_changed();
            draw_home_content();
        }
    } else if (!strcmp(c->cap->id, "shell.settings"))
        ink_shell_open_page("Settings");
    else if (!strcmp(c->cap->id, "shell.display"))
        ink_shell_open_page("SetDisplay");
    else if (!strcmp(c->cap->id, "shell.sound"))
        ink_shell_open_page("SetSound");
    else if (!strcmp(c->cap->id, "shell.network"))
        ink_shell_open_page("SetNetwork");
    else if (!strcmp(c->cap->id, "shell.known_networks"))
        ink_shell_open_page("KnownNetworks");
    else if (!strcmp(c->cap->id, "shell.notifications"))
        ink_shell_open_page("Notifications");
    else if (!strcmp(c->cap->id, "shell.notify_settings"))
        ink_shell_open_page("SetNotifications");
    else if (!strcmp(c->cap->id, "shell.page_next"))
        turn_body(1);
    else if (!strcmp(c->cap->id, "shell.page_prev"))
        turn_body(-1);
    else if (!strcmp(c->cap->id, "shell.notify_next") || !strcmp(c->cap->id, "shell.notify_prev")) {
        if (N.notify_list) ink_ui_list_turn(N.notify_list, !strcmp(c->cap->id, "shell.notify_next") ? 1 : -1);
    } else if (!strcmp(c->cap->id, "shell.wifi_next") || !strcmp(c->cap->id, "shell.wifi_prev")) {
        if (N.page_body && N.page_id == PAGE_NETWORK && !N.picked_ssid[0]) {
            if (!strcmp(c->cap->id, "shell.wifi_next") && (N.wifi_page + 1) * 2 < N.wifi_count)
                N.wifi_page++;
            else if (!strcmp(c->cap->id, "shell.wifi_prev") && N.wifi_page)
                N.wifi_page--;
            ink_ui_content_changed();
            ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
            draw_page_body();
        }
    } else if (!strcmp(c->cap->id, "shell.wifi_pick")) {
        int64_t index = -1;
        ink_args_get_int(&c->args, "index", &index);
        if (!N.page_body || N.page_id != PAGE_NETWORK || index < 0 || index >= N.wifi_count)
            c->rc.status = INK_RC_REJECTED;
        else {
            snprintf(N.picked_ssid, sizeof N.picked_ssid, "%s", N.wifi_ssid[index]);
            draw_page_body();
        }
    }
    snprintf(c->rc.object, sizeof c->rc.object, "screen:%s", ink_shell_board());
}

static void cap_launch(ink_call_t *c) {
    if (ink_boot_flow_launch(c)) return;
    char id[32];
    ink_args_get_str(&c->args, "id", id, sizeof id);
    if (!strcmp(id, "settings")) {
        if (child())
            c->rc.status = INK_RC_REJECTED;
        else
            ink_shell_open_page("Settings");
        return;
    }
    if (!strcmp(id, "ai")) {
        ink_cap_invoke("shell.ai_home", NULL, c->src, 0, &c->rc);
        return;
    }
    if (child() && !ink_member_app_mode(NULL, id)) { c->rc.status = INK_RC_REJECTED; snprintf(c->rc.detail, sizeof c->rc.detail, "%s", TR("家长没有打开这个应用")); return; }
    if (c->src == INK_SRC_TOUCH) ink_probe_count("home.taps");
    for (int i = 0; i < ink_app_count(); i++)
        if (!strcmp(ink_app_at(i)->id, id) && ink_app_at(i)->entry) {
            uint64_t cause = osal_now_us();
            if (ink_app_open(id, NULL, NULL) != 0)
                c->rc.status = INK_RC_REJECTED;
            else
                ink_ui_cause(cause, "ui.open_to_stable_ms");
            return;
        }
    c->rc.status = INK_RC_REJECTED;
    home_note(TR("这个应用还没有可打开的内容"));
    snprintf(c->rc.say, sizeof c->rc.say, "%s", N.result);
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

#define NAV(id_, title_, child_)                                                                                       \
    {.id = id_, .title = title_, .effect = INK_FX_NAVIGATE, .child_ok = child_, .run = cap_nav}
static const ink_cap_t CAPS[] = {
    NAV("shell.control", "打开控制中心", true),
    NAV("shell.close", "收起面板", true),
    NAV("shell.back", "返回上一层", true),
    NAV("shell.lock", "锁屏", true),
    NAV("shell.members", "选择家人", true),
    NAV("shell.more", "更多控制", false),
    NAV("shell.settings", "打开设置", false),
    NAV("shell.display", "显示和亮度", false),
    NAV("shell.sound", "声音", false),
    NAV("shell.network", "网络和 SIM 卡", false),
    NAV("shell.known_networks", "已存的网络", false),
    NAV("shell.notifications", "查看通知", true),
    NAV("shell.notify_settings", "通知渠道设置", false),
    NAV("shell.page_next", "设置页的下一页", true),
    NAV("shell.page_prev", "设置页的上一页", true),
    NAV("shell.notify_next", "通知下一页", true),
    NAV("shell.notify_prev", "通知上一页", true),
    NAV("shell.wifi_next", "附近网络下一组", false),
    NAV("shell.wifi_prev", "附近网络上一组", false),
    {.id = "shell.search",
     .title = "查找应用",
     .activity = "home",
     .input = "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"maxLength\":64}},\"required\":["
              "\"query\"]}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .run = cap_nav},
    {.id = "shell.wifi_pick",
     .title = "选附近网络（密码由人在设备上填写）",
     .input = "{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\",\"minimum\":0}},\"required\":["
              "\"index\"]}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = false,
     .run = cap_nav},
    {.id = "home.next", .app = "home",
     .title = "主页下一屏",
     .activity = "home",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .run = cap_nav},
    {.id = "home.prev", .app = "home",
     .title = "主页上一屏",
     .activity = "home",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .run = cap_nav},
    {.id = "shell.launch",
     .title = "打开主页上的应用",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .run = cap_launch},
    {.id = "shell.resume", .title = "接着读", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_resume}};

static void refresh(void *unused) {
    (void) unused;
    if (ink_power_state() == INK_PWR_LOCKED) return;
    if (!current_session(N.epoch)) return;
    if (child() && N.home && N.home_page != 2) kid_time_read();
    bool body = __atomic_exchange_n(&N.need_body, false, __ATOMIC_ACQ_REL);
    if (body && N.control && ink_member_space_open()) draw_control();
    if (body && N.page_body && ink_member_space_open() && !N.picked_ssid[0]) draw_page_body();
    for (unsigned i = 0; i < sizeof N.status / sizeof *N.status; i++)
        if (N.status[i]) {
            lv_obj_t *parent = lv_obj_get_parent(N.status[i]);
            bool show_time = N.status_time[i];
            lv_obj_delete(N.status[i]);
            lv_obj_t *bar = ink_shell_statusbar(parent, show_time);
            ink_ui_hint_obj(bar, INK_REFRESH_FAST);
        }
    if (N.clock) {
        char clock[16], date[96];
        ink_shell_time_strings(clock, sizeof clock, date, sizeof date);
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
    (void) h;
    if (ctx && !strcmp(id, "power.state")) return; /* No status-bar field changes. */
    if (ctx && !strcmp(id, "app.cards_rev")) {
        ink_ui_kick();
        return;
    }
    if (!ctx && (!strcmp(id, "display.font_size") || !strcmp(id, "system.language"))) {
        ink_ui_redraw();
        return;
    }
    if (ctx && (!strcmp(id, "member.current") || (!strcmp(id, "member.authed") && !ink_member_space_open()))) {
        __atomic_add_fetch(&N.session, 1, __ATOMIC_ACQ_REL);
        return;
    }
    /* A new signal reading redraws the status bar only when its bar count changes: every redraw is a refresh on the
     * panel, and the user wants as few as possible (D-127); a reading that moved a few dBm changes nothing seen. */
    bool cell_status = !strcmp(id, "cell.registered") || !strcmp(id, "cell.sim_missing") ||
        (!strcmp(id, "cell.signal") && integer("cell.registered") &&
         cell_bars(integer("cell.signal")) != __atomic_load_n(&N.cell_bars_shown, __ATOMIC_ACQUIRE));
    bool cell_config = !strcmp(id, "cell.config_readonly") && integer("cell.config_readonly") &&
        N.page_body && N.page_id == PAGE_NETWORK;
    if (cell_config) __atomic_store_n(&N.need_body, true, __ATOMIC_RELEASE);
    if (ctx && strncmp(id, "time.", 5) && strcmp(id, "notify.unread") && !cell_status && !cell_config &&
        strcmp(id, "audio.mic") && strcmp(id, "net.type") && strncmp(id, "wifi.", 5) && strcmp(id, "bt.connected"))
        return;
    if (!ctx || !strcmp(id, "notify.unread") || !strncmp(id, "wifi.", 5) || !strcmp(id, "bt.connected"))
        __atomic_store_n(&N.need_body, true, __ATOMIC_RELEASE);
    ink_exec_post(INK_EXEC_UI, &N.refresh, refresh, NULL);
}
static const char *const FONT_CHOICES[] = {TR_NOOP("小"), TR_NOOP("中"), TR_NOOP("大"), TR_NOOP("特大"), NULL};
static const char *const LOCK_CHOICES[] = {TR_NOOP("这一页"), TR_NOOP("时钟和待办"), TR_NOOP("壁纸"), TR_NOOP("简约时钟"), TR_NOOP("待机时钟"), TR_NOOP("月历"), NULL};
static const ink_setting_def_t SETTINGS[] = {
    {.id = "display.font_size", .title = TR_NOOP("字号"), .type = INK_SET_ENUM, .def = 1, .choices = FONT_CHOICES},
    {.id = "display.lock_style",
     .title = TR_NOOP("锁屏画面"),
     .type = INK_SET_ENUM,
     .def = 0,
     .choices = LOCK_CHOICES,
     .child_may_change = true}};

int ink_shell_ai_init(void);
static void notify_changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)args;
    if (current_session(N.epoch) && N.page_body &&
        ((N.page_id == PAGE_NOTIFICATIONS && !strcmp(id, "notify.changed")) ||
         (N.page_id == PAGE_CHANNELS && !strcmp(id, "notify.channels_changed"))))
        draw_page_body();
}

int ink_shell_init(void) {
    if (ink_cap_subscribe("wifi.changed", known_load, NULL, INK_EXEC_UI)) return -1;
    if (ink_activity_card_init() != 0) return -1;
    if (today_init() != 0) return -1;
    static const ink_cap_t notify = {.id = "shell.notify_open", .title = "打开通知并标为已读", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = notify_open};
    if (ink_search_init() != 0 || ink_cap_register(&notify, 1) != 0) return -1;
    if (ink_settings_pages_init() != 0 || ink_bluetooth_page_init() != 0 || ink_time_settings_init() != 0 || ink_mobile_network_init() != 0) return -1;
    if (ink_boot_flow_init() != 0) return -1;
    if (ink_shell_ai_init() != 0 || ink_kid_pages_init() != 0) return -1;
    if (ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) ||
        ink_settings_define(SETTINGS, sizeof SETTINGS / sizeof *SETTINGS))
        return -1;
    if (ink_settings_watch("", changed, NULL, INK_EXEC_UI)) return -1;
    if (ink_cap_subscribe("notify.changed", notify_changed, NULL, INK_EXEC_UI)) return -1;
    if (ink_cap_subscribe("notify.channels_changed", notify_changed, NULL, INK_EXEC_UI)) return -1;
    return ink_state_watch("", changed, (void *) 1, INK_EXEC_DIRECT);
}

void ink_shell_activity_refresh(void) {
    if (!current_session(N.epoch)) return;
    ink_ui_content_changed();
    if (N.home && N.home_page == 1) draw_home_content();
    if (N.control) draw_control();
}

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
static void today_load(void) {
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
static int today_slots(void) { return T.saved ? T.count : 4; }
static bool today_match(const ink_json_t *j, int slot) {
    if (!T.saved || !slot) return ink_json_eq(j, ink_json_find(j, 0, "app"), CARD_APPS[slot]) ||
        (slot == 4 && ink_json_eq(j, ink_json_find(j, 0, "app"), "home") && ink_json_eq(j, ink_json_find(j, 0, "id"), "upcoming"));
    char app[32] = "", id[16] = "", key[48];
    ink_json_str(j, ink_json_find(j, 0, "app"), app, sizeof app);
    ink_json_str(j, ink_json_find(j, 0, "id"), id, sizeof id);
    snprintf(key, sizeof key, "%s:%s", app, id);
    return slot <= T.count && !strcmp(key, T.selected[slot - 1]);
}
static const char *today_icon(int slot) {
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
static void today_model(void) {
    for (int i = 0; i <= today_slots(); i++) {
        if (T.saved && i) {
            snprintf(N.card_objects[i], sizeof N.card_objects[i], "%s", T.selected[i - 1]);
            N.card_fields[i].id = N.card_actions[i].id = T.selected[i - 1];
            if (!N.cards[i].id[0]) N.card_fields[i].label = N.card_fields[i].value = TR("卡片暂不可用");
        }
    }
    N.today.field_count = N.today.action_count = today_slots() + 1;
}
static void kid_today_model(void) {
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
static bool icon_close(void) {
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
static void icon_event(lv_event_t *e) {
    if (!current_session(N.epoch)) return;
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) { N.icon_version = ink_ui_content_version(); N.icon_generation = ink_ui_generation(); return; }
    if (N.icon_version && (N.icon_version != ink_ui_content_version() || N.icon_generation != ink_ui_generation())) return;
    char args[96]; snprintf(args, sizeof args, "{\"id\":\"%s\"}", (const char *)lv_event_get_user_data(e));
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .page_gen = ink_ui_content_version(), .args_json = args, .reply_exec = INK_EXEC_UI};
    ink_cap_call(lv_event_get_code(e) == LV_EVENT_SHORT_CLICKED ? "shell.launch" : "shell.icon_menu", NULL, &opts);
}
static void icon_bind(lv_obj_t *obj, const char *id) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(obj, icon_event, LV_EVENT_PRESSED, (void *)id);
    lv_obj_add_event_cb(obj, icon_event, LV_EVENT_SHORT_CLICKED, (void *)id);
    lv_obj_add_event_cb(obj, icon_event, LV_EVENT_LONG_PRESSED, (void *)id);
}
static int today_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "shell.today_edit", .title = "今日屏放什么", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = today_call},
        {.id = "shell.icon_menu", .title = "应用快捷动作", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"}},\"required\":[\"id\"]}", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = today_call},
        {.id = "shell.today_card", .title = "选择今日卡片或上移", .input = "{\"type\":\"object\",\"properties\":{\"card\":{\"type\":\"string\"},\"up\":{\"type\":\"boolean\"}},\"required\":[\"card\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .run = today_call},
        {.id = "shell.today_prev", .title = "今日卡片上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = today_call},
        {.id = "shell.today_next", .title = "今日卡片下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = today_call}};
    return ink_db_register(&TODAY_DB) || ink_cap_register(caps, sizeof caps / sizeof *caps);
}

static int kid_control_snapshot(char *buf, size_t cap) {
    int64_t volume = ink_setting_get("audio.volume"), max = ink_setting_get("audio.child_max");
    int n = snprintf(buf, cap, "KidControl；当前=%s；亮度=%lld；暖光=%lld；音量=%lld；音量上限=%lld；通知=%d；动作：settings.set、system.set_volume、shell.notify_open、notify.read、shell.close。",
        ink_member_current()->name, (long long)ink_setting_get("display.light"), (long long)ink_setting_get("display.warmth"),
        (long long)(volume > max ? max : volume), (long long)max, ink_notify_unread());
    for (int i = 0; i < N.control_count && n >= 0 && (size_t)n < cap; i++)
        n += snprintf(buf + n, cap - (size_t)n, "通知[%u]=%s，%s；", (unsigned)N.control_items[i].id, N.control_items[i].title, N.control_items[i].body);
    return n;
}

static void notify_open(ink_call_t *c) {
    int64_t id = 0; ink_args_get_int(&c->args, "id", &id);
    ink_db_cursor_t cursor = {0}; ink_notify_view_t n;
    while (ink_notify_next(&cursor, &n) == 1) if (n.id == (uint32_t)id) {
        ink_notify_mark_read(n.id);
        if (n.action[0]) {
            ink_shell_close();
            ink_call_opts_t opts = {.src = c->src, .args_json = n.action_args, .reply_exec = INK_EXEC_UI};
            if (ink_cap_call(n.action, NULL, &opts) != 0) c->rc.status = INK_RC_FAILED;
        }
        return;
    }
    c->rc.status = INK_RC_STALE;
}

static void kid_volume_mark(lv_obj_t *slider, int top) {
    ink_ui_box(slider, width() * top / 100, 0, INK_STROKE, INK_SLIDER, 0, INK_SURFACE_BLACK, false);
}
