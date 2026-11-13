/* ui-scale-runtime: dimension width Canvas width minus theme margins, shared by all shell pages. */
/* ui-scale-runtime: dimension bb_x,bb_y The battery icon's position, passed from the theme-sized status bar. */
/* ui-scale-runtime: dimension x,w,left Right-to-left positions measured from theme-sized text and icons. */
#include "shell_internal.h"

/* The status bar after MuditaOS (module-gui/gui/widgets/StatusBar.cpp:30-105, 2cd0e472): the time on the left; on the
 * right only what is real, a connection or a fault (Bluetooth only while connected, airplane mode in place of the
 * network); in the middle what needs attention now (a call, the microphone). Each bar redraws itself: a new minute
 * changes its time alone, anything else the icons beside it, and nothing while what it shows stays the same (D-127). */

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

/* What the icons beside the time show (zeroed whole, so two can be compared bytewise). cell: bars while registered. */
typedef struct { int battery; signed char cell; char mic; bool plane, wifi, bt, bell, call; } bar_state_t;
enum { BARS_MAX = 6, CELL_NO_SERVICE = -1, CELL_NO_SIM = -2, CELL_OFF = -3 };
/* A bar is the time and a box beside it (icons[], beginning at left[]) with the rest: each redraws without the other. */
static struct { lv_obj_t *bar[BARS_MAX], *time[BARS_MAX], *icons[BARS_MAX]; int left[BARS_MAX]; bar_state_t shown[BARS_MAX]; ink_exec_job_t job; } B INK_PSRAM_BSS;

static void read_state(bar_state_t *s) {
    memset(s, 0, sizeof *s);
    char type[8], mic[16], who[8];
    state_text("net.type", type, sizeof type), state_text("audio.mic", mic, sizeof mic);
    s->plane = ink_setting_get("net.airplane") != 0, s->wifi = !strcmp(type, "wifi") && integer("net.online");
    s->cell = s->plane ? CELL_OFF : integer("cell.registered") ? (signed char)cell_bars(integer("cell.signal"))
                                  : integer("cell.sim_missing") ? CELL_NO_SIM : CELL_NO_SERVICE;
    s->bt = integer("bt.connected") != 0, s->bell = integer("notify.unread") > 0, s->call = ink_phone_activity(who, sizeof who);
    s->mic = !strcmp(mic, "recording") ? 'r' : !strcmp(mic, "sending") ? 's' : !strcmp(mic, "local") ? 'l' : 0;
    s->battery = (int)integer("battery.level");
}

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

/* Signal bars in a small icon's square, the left edge at x. */
static void signal_bars(lv_obj_t *bar, int x, int bars) {
    int s = ink_ui_px(INK_ICON_SMALL), step = s / 4, top = ink_ui_px((INK_STATUS_BAR - INK_ICON_SMALL) / 2);
    for (int i = 0; i < 4; i++) {
        lv_obj_t *o = lv_obj_create(bar);
        int h = step * (i + 1);
        lv_obj_remove_style_all(o);
        lv_obj_set_style_border_color(o, ink_ui_ink(), 0), lv_obj_set_style_border_width(o, ink_ui_px(INK_STROKE) / 2, 0);
        lv_obj_set_style_bg_color(o, ink_ui_ink(), 0), lv_obj_set_style_bg_opa(o, i < bars ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_pos(o, ink_ui_px(x) + i * step, top + s - h), lv_obj_set_size(o, step - ink_ui_px(INK_STROKE), h);
    }
}
/* Right to left: what is drawn ends at x; returns where the next one ends. */
static int put_icon(lv_obj_t *bar, int x, const char *icon) {
    ink_ui_icon(bar, icon, INK_ICON_SMALL, INK_ICON_LINE, x - INK_ICON_SMALL, (INK_STATUS_BAR - INK_ICON_SMALL) / 2, false);
    return x - INK_ICON_SMALL - INK_GAP_S;
}
static int put_text(lv_obj_t *bar, int x, const char *text) {
    int w = ink_ui_text_width(text, INK_FONT_STATUS, INK_WEIGHT_MEDIUM);
    ink_ui_line(bar, text, INK_FONT_STATUS, INK_WEIGHT_MEDIUM, x - w, (INK_STATUS_BAR - ink_ui_line_height(INK_FONT_STATUS)) / 2, w);
    return x - w - INK_GAP_S;
}
/* Into the box that begins at `left` on the bar: unread notifications first, after the time. */
static void draw_icons(lv_obj_t *bar, const bar_state_t *s, int left) {
    int x = INK_CANVAS_W - INK_MARGIN - INK_ICON_LIST - left, battery_y = (INK_STATUS_BAR - INK_ICON_LIST) / 2;
    ink_ui_icon(bar, "battery", INK_ICON_LIST, INK_ICON_LINE, x, battery_y, false);
    battery_bars(bar, x, battery_y, s->battery);
    char text[12];
    snprintf(text, sizeof text, "%d", s->battery), x = put_text(bar, x - INK_GAP_S, text) - INK_GAP_S;
    if (s->plane) x = put_icon(bar, x, "plane"); /* airplane mode in place of the network, as the phone mode there */
    else if (s->cell >= 0) { int at = put_text(bar, x - INK_ICON_SMALL - INK_GAP_S, "4G"); signal_bars(bar, x - INK_ICON_SMALL, s->cell); x = at; } /* label, then bars */
    else x = put_text(bar, x, s->cell == CELL_NO_SIM ? TR("无卡") : TR("无服务"));
    if (s->wifi) x = put_icon(bar, x, "wifi");
    if (s->bt) put_icon(bar, x, "bluetooth"); /* only while headphones are connected */
    int y = (INK_STATUS_BAR - INK_ICON_SMALL) / 2;
    if (s->bell) ink_ui_icon(bar, "bell", INK_ICON_SMALL, INK_ICON_LINE, 0, y, false);
    const char *now = s->call ? TR("通话中") : s->mic == 'r' ? TR("录音") : s->mic == 's' ? TR("在听") : NULL;
    if (now) {
        int pill_w = ink_ui_text_width(now, INK_FONT_STATUS, INK_WEIGHT_MEDIUM) + 2 * INK_GAP_S, h = ink_ui_line_height(INK_FONT_STATUS) + INK_GAP_S;
        lv_obj_t *pill = ink_ui_box(bar, (INK_CANVAS_W - pill_w) / 2 - left, (INK_STATUS_BAR - h) / 2, pill_w, h, INK_RADIUS_KEY, INK_SURFACE_BLACK, false);
        lv_obj_set_style_text_align(ink_ui_line(pill, now, INK_FONT_STATUS, INK_WEIGHT_MEDIUM, 0, INK_GAP_S / 2, pill_w), LV_TEXT_ALIGN_CENTER, 0);
    } else if (s->mic == 'l')
        ink_ui_icon(bar, "mic", INK_ICON_SMALL, INK_ICON_LINE, (INK_CANVAS_W - INK_ICON_SMALL) / 2 - left, y, false);
}
static void bar_deleted(lv_event_t *e) {
    for (int i = 0; i < BARS_MAX; i++)
        if (B.bar[i] == lv_event_get_target_obj(e)) B.bar[i] = B.time[i] = B.icons[i] = NULL;
}

lv_obj_t *ink_shell_statusbar(lv_obj_t *parent, bool show_time) {
    lv_obj_t *bar = ink_ui_box(parent, 0, 0, INK_CANVAS_W, INK_STATUS_BAR, 0, INK_SURFACE_WHITE, false);
    ink_shell_bind(bar, "shell.control", NULL);
    lv_obj_t *time = NULL;
    static const char WIDEST[] = "00:00";
    int left = INK_MARGIN + (show_time ? ink_ui_text_width(WIDEST, INK_FONT_STATUS, INK_WEIGHT_MEDIUM) + INK_GAP_S : 0); /* the bell begins the box */
    if (show_time) {
        char clock[16], date[96];
        ink_shell_time_strings(clock, sizeof clock, date, sizeof date);
        time = ink_ui_line(bar, clock, INK_FONT_STATUS, INK_WEIGHT_MEDIUM, INK_MARGIN,
                           (INK_STATUS_BAR - ink_ui_line_height(INK_FONT_STATUS)) / 2, 0);
    }
    lv_obj_t *icons = ink_ui_box(bar, left, 0, INK_CANVAS_W - left, INK_STATUS_BAR, 0, INK_SURFACE_WHITE, false);
    bar_state_t s;
    lv_obj_set_style_bg_opa(icons, LV_OPA_TRANSP, 0), read_state(&s), draw_icons(icons, &s, left);
    __atomic_store_n(&N.cell_bars_shown, s.cell > 0 ? s.cell : 0, __ATOMIC_RELEASE);
    for (int i = 0; i < BARS_MAX; i++)
        if (!B.bar[i]) {
            B.bar[i] = bar, B.time[i] = time, B.icons[i] = icons, B.left[i] = left, B.shown[i] = s;
            lv_obj_add_event_cb(bar, bar_deleted, LV_EVENT_DELETE, NULL);
            break;
        }
    return bar;
}

/* UI task: a new minute changes the time label alone; other changes redraw the icons, if what they show changed. */
static void bar_refresh(void *arg) {
    (void)arg;
    if (ink_power_state() == INK_PWR_LOCKED) return;
    char clock[16], date[96];
    bar_state_t s;
    ink_shell_time_strings(clock, sizeof clock, date, sizeof date), read_state(&s);
    __atomic_store_n(&N.cell_bars_shown, s.cell > 0 ? s.cell : 0, __ATOMIC_RELEASE);
    for (int i = 0; i < BARS_MAX; i++) {
        if (!B.bar[i]) continue;
        if (B.time[i] && strcmp(lv_label_get_text(B.time[i]), clock)) /* where it was, then where it is */
            ink_ui_hint_obj(B.time[i], INK_REFRESH_FAST), lv_label_set_text(B.time[i], clock), ink_ui_hint_obj(B.time[i], INK_REFRESH_FAST);
        if (!memcmp(&s, &B.shown[i], sizeof s)) continue;
        lv_obj_clean(B.icons[i]), draw_icons(B.icons[i], &s, B.left[i]), B.shown[i] = s;
        ink_ui_hint_obj(B.icons[i], INK_REFRESH_FAST);
    }
    if (N.control) draw_control(); /* its tiles read the same states */
}
static void bar_changed(void *ctx, int h, const char *id) { /* any task: a state or the airplane setting */
    (void)ctx; (void)h;
    static const char *const SHOWN[] = {"time.", "battery.level", "net.", "wifi.", "cell.", "bt.", "audio.mic", "notify.unread", "call."};
    for (unsigned i = 0; i < sizeof SHOWN / sizeof *SHOWN; i++)
        if (!strncmp(id, SHOWN[i], strlen(SHOWN[i]))) {
            ink_exec_post(INK_EXEC_UI, &B.job, bar_refresh, NULL);
            return;
        }
}
static void call_event(void *ctx, const char *id, const ink_args_t *args) { (void)args; bar_changed(ctx, 0, id); } /* call.: began, ended */
int ink_shell_bar_init(void) {
    return ink_state_watch("", bar_changed, NULL, INK_EXEC_DIRECT) || ink_settings_watch("net.airplane", bar_changed, NULL, INK_EXEC_UI) ||
        ink_cap_subscribe("call.", call_event, NULL, INK_EXEC_UI) ? -1 : 0;
}

void ink_shell_topbar(lv_obj_t *parent, const char *title) {
    int y = INK_STATUS_BAR + INK_GAP_S;
    lv_obj_t *back =
        ink_ui_circle(parent, "back", INK_MARGIN, y, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_STATIC, NULL, NULL);
    ink_shell_bind(back, "shell.back", NULL);
    ink_ui_label(parent, title, INK_FONT_TITLE, INK_WEIGHT_BOLD, INK_MARGIN + INK_CONTROL + INK_GAP,
                 y + (INK_CONTROL - ink_ui_line_height(INK_FONT_TITLE)) / 2, width() - INK_CONTROL - INK_GAP);
}
