/* ui-scale-runtime: dimension width Canvas width minus theme margins, shared by all shell pages. */
/* ui-scale-runtime: dimension bb_x,bb_y The battery icon's position, passed from the theme-sized status bar. */
#include "shell_internal.h"

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
