/* ui-scale-runtime: dimension width Canvas width minus theme margins, shared by all shell pages. */
/* ui-scale-runtime: dimension notifications Returns the bottom of theme-sized notification rows. */
/* ui-scale-runtime: dimension y,ink_activity_card_draw Rows accept theme positions; activity cards return theme-sized heights. */
/* ui-scale-runtime: dimension top Child volume marker uses the configured percentage. */
#include "shell_internal.h"

static void kid_volume_mark(lv_obj_t *slider, int top);
static int kid_control_snapshot(char *buf, size_t cap);
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

void slider_changed(lv_event_t *e) {
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

void bind_toggle(lv_obj_t *o, const char *id) {
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

void draw_control(void) {
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

static int kid_control_snapshot(char *buf, size_t cap) {
    int64_t volume = ink_setting_get("audio.volume"), max = ink_setting_get("audio.child_max");
    int n = snprintf(buf, cap, "KidControl；当前=%s；亮度=%lld；暖光=%lld；音量=%lld；音量上限=%lld；通知=%d；动作：settings.set、system.set_volume、shell.notify_open、notify.read、shell.close。",
        ink_member_current()->name, (long long)ink_setting_get("display.light"), (long long)ink_setting_get("display.warmth"),
        (long long)(volume > max ? max : volume), (long long)max, ink_notify_unread());
    for (int i = 0; i < N.control_count && n >= 0 && (size_t)n < cap; i++)
        n += snprintf(buf + n, cap - (size_t)n, "通知[%u]=%s，%s；", (unsigned)N.control_items[i].id, N.control_items[i].title, N.control_items[i].body);
    return n;
}

static void kid_volume_mark(lv_obj_t *slider, int top) {
    ink_ui_box(slider, width() * top / 100, 0, INK_STROKE, INK_SLIDER, 0, INK_SURFACE_BLACK, false);
}
