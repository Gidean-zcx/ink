/* ui-scale-runtime: dimension width Canvas width minus theme margins, shared by all shell pages. */
/* ui-scale-runtime: dimension y,x,w,h,col,room,name_w,who_w,top,half,card,tiles,header,ink_activity_card_draw,ink_notification_list_draw Rows take theme positions; names and the date are measured text; the activity card and notification rows return theme-sized heights. */
#include <stdarg.h>
#include "shell_internal.h"
#include "notification_list.h"

/* The control centre after AOSP quick settings (Lineage android_frameworks_base 22de88b7, packages/SystemUI qs/): a first
 * pull shows who uses the device, four tiles and the notifications; a second pull, or 展开, every tile and the light
 * sliders. The stages switch as a whole, a changed tile redraws alone (D-127). A tile says its name and current value;
 * a tap runs the capability the AI calls, a long press opens the page with that switch. */
enum { TILES = 8, FIRST = 4, SLIDERS = 3 };
enum { ST_ON, ST_OFF, ST_UNAVAILABLE, ST_CONNECTING, ST_FAILED, ST_LIMITED, ST_ACTION };
/* AOSP's default tiles once the internet tile is split (wifi, bt, cell, dnd, airplane, battery saver), then ours.
 * leave: an action or another page, so the panel goes first (as AOSP collapses the shade for them). */
static const struct { const char *name, *icon, *page; bool child, leave; } TILE[TILES] = {
    {"Wi-Fi", "wifi", "shell.network", true, false},          {TR_NOOP("蓝牙"), "bluetooth", "shell.bluetooth", true, false},
    {TR_NOOP("移动网络"), "signal", "shell.mobile", false, true}, {TR_NOOP("勿扰"), "moon", "shell.sound", false, false},
    {TR_NOOP("飞行模式"), "plane", "shell.network", false, false}, {TR_NOOP("省电"), "battery", "shell.display", false, false},
    {TR_NOOP("叫名字"), "mic", "shell.voice", false, false},      {TR_NOOP("刷新屏幕"), "display", "shell.display", true, true}};
static const struct { const char *id, *name; } SLIDER[SLIDERS] = {
    {"display.light", TR_NOOP("亮度")}, {"display.warmth", TR_NOOP("暖光")}, {"audio.volume", TR_NOOP("音量")}};
typedef struct { char value[48], args[56]; const char *cap; int state; bool on, usable; } tile_t;
/* tile[] and slider[] are fixed holders, each redrawn inside: a new object first lands at its parent's corner, and
 * that corner must be its own, or a redraw would refresh whatever is at the panel's corner too. */
static struct {
    lv_obj_t *tile[TILES], *slider[SLIDERS], *date, *card;
    int w[TILES], level[SLIDERS], limit;
    char shown[TILES][64], date_text[48], activity[512], member[16];
    bool kid, expanded, airplane;
    lv_dir_t gesture;
    ink_exec_job_t job;
} C INK_PSRAM_BSS;

static void kid_volume_mark(lv_obj_t *slider, int top);
static void refused(void *ctx, const ink_receipt_t *rc) { (void)ctx; ink_shell_refused(rc); }
static void call(const char *cap, const char *args) { /* a person's touch, as ink_shell_bind calls it */
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .args_json = args && args[0] ? args : NULL, .reply_exec = INK_EXEC_UI,
                         .member = ink_member_current()->id, .done = refused};
    ink_cap_call(cap, NULL, &o);
}
static void tile_set(tile_t *t, bool on, const char *value, int state) { t->on = on, t->state = state; snprintf(t->value, sizeof t->value, "%s", value); }
static void tile_switch(tile_t *t, const char *cap, const char *setting, bool on) { /* on or off, the tap turns it over */
    tile_set(t, on, on ? TR("开") : TR("关"), on ? ST_ON : ST_OFF);
    t->cap = cap;
    if (setting) snprintf(t->args, sizeof t->args, "{\"id\":\"%s\",\"value\":%d}", setting, !on);
    else snprintf(t->args, sizeof t->args, "{\"enabled\":%s}", on ? "false" : "true");
}
/* What a tile says now, every state in its own words. A request taken is not shown as done: the setting moves when the
 * service took it, the connection only when the link is up. */
static void tile_read(int i, tile_t *t) {
    memset(t, 0, sizeof *t);
    t->usable = true;
    char a[40], b[40];
    int64_t v = 0;
    switch (i) {
    case 0: /* Wi-Fi: connecting while the link in use is Wi-Fi and not up yet; a failed join as the Wi-Fi service says */
        tile_switch(t, "wifi.set_enabled", NULL, ink_setting_get("wifi.enabled") != 0);
        state_text("net.type", a, sizeof a), state_text("wifi.state", b, sizeof b), v = !strcmp(a, "wifi");
        state_text("wifi.ssid", a, sizeof a);
        if (t->on && v && integer("net.online")) tile_set(t, true, a[0] ? a : TR("已连接"), ST_ON);
        else if (t->on && (v || !strcmp(b, "joining"))) tile_set(t, true, TR("连接中"), ST_CONNECTING);
        else if (t->on && !strcmp(b, "failed")) tile_set(t, true, TR("没连上"), ST_FAILED);
        break;
    case 1: /* Bluetooth: the headphones' name while they are connected */
        tile_switch(t, "settings.set", "bt.enabled", ink_setting_get("bt.enabled") != 0);
        state_text("bt.device", a, sizeof a);
        if (!integer("bt.available")) tile_set(t, false, TR("不可用"), ST_UNAVAILABLE), t->usable = false;
        else if (t->on) tile_set(t, true, integer("bt.connected") ? (a[0] ? a : TR("已连接")) : TR("未连接"), ST_ON);
        break;
    case 2: /* mobile network: what the modem says (the system has no operator name); its page has the switches */
        v = integer("cell.registered");
        tile_set(t, v, v ? "4G" : TR("无服务"), v ? ST_ON : ST_FAILED), t->cap = "shell.mobile";
        if (ink_setting_get("net.airplane") || integer("cell.sim_missing"))
            tile_set(t, false, ink_setting_get("net.airplane") ? TR("关") : TR("无卡"), ST_UNAVAILABLE), t->usable = false;
        break;
    case 3: tile_switch(t, "settings.set", "notify.dnd", ink_setting_get("notify.dnd") != 0); break;
    case 4: tile_switch(t, "net.set_airplane", NULL, ink_setting_get("net.airplane") != 0); break;
    case 5: tile_switch(t, "power.set_saving", NULL, integer("power.saving") != 0); break;
    case 6: /* the wake word: 0 while the screen is on, 1 always, 2 off; the tap as the old pill did */
        v = ink_setting_get("voice.handsfree");
        tile_set(t, v != 2, v == 1 ? TR("一直") : v == 0 ? TR("亮屏时") : TR("关"), v != 2 ? ST_ON : ST_OFF), t->cap = "settings.set";
        snprintf(t->args, sizeof t->args, "{\"id\":\"voice.handsfree\",\"value\":%d}", v == 2 ? 0 : 2);
        break;
    default: tile_set(t, false, TR("清残影"), ST_ACTION), t->cap = "system.full_refresh"; break;
    }
    const ink_cap_t *c = ink_cap_find(t->cap);
    if (child() && t->usable && (!c || !ink_member_allows(c->id, c->child_ok))) /* the family's rules, as the gate keeps them */
        tile_set(t, false, TR("受家人限制"), ST_LIMITED), t->usable = false;
}
static bool page_allowed(int i) { const ink_cap_t *p = ink_cap_find(TILE[i].page); return p && ink_member_allows(p->id, p->child_ok); }

__attribute__((format(printf, 4, 5))) static void add(char *buf, size_t cap, int *n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (*n >= 0 && (size_t)*n < cap) { int k = vsnprintf(buf + *n, cap - (size_t)*n, fmt, ap); *n = k < 0 ? -1 : *n + k; }
    va_end(ap);
}
/* For the AI: what is on screen, each tile with its name, value, state and what a tap and a long press do. */
static int control_snapshot(char *buf, size_t cap) {
    static const char *const STATE[] = {"开", "关", "不可用", "连接中", "没连上", "受家人限制", "动作"};
    if (!current_session(N.epoch)) return snprintf(buf, cap, "空间锁定或正在切换；AI 无权读取。");
    int n = 0, volume = (int)ink_setting_get("audio.volume"), top = (int)ink_setting_get("audio.child_max");
    if (C.kid)
        add(buf, cap, &n, "KidControl；当前=%s；亮度=%lld；暖光=%lld；音量=%lld；音量上限=%lld；通知=%d；格：", ink_member_current()->name,
            (long long)ink_setting_get("display.light"), (long long)ink_setting_get("display.warmth"), (long long)(volume > top ? top : volume), (long long)top, ink_notify_unread());
    else
        add(buf, cap, &n, "Control；%s；当前=%s（换人 shell.members）；日期=%s；更多=%d；亮度=%lld；暖光=%lld；勿扰=%lld；叫名字=%lld；飞行模式=%lld；通知=%d；格：",
            C.expanded ? "第二段：全部开关，亮度和暖光滑杆" : "第一段：四个开关和通知", ink_member_current()->name, C.date_text, C.expanded,
            (long long)ink_setting_get("display.light"), (long long)ink_setting_get("display.warmth"), (long long)ink_setting_get("notify.dnd"),
            (long long)ink_setting_get("voice.handsfree"), (long long)ink_setting_get("net.airplane"), ink_notify_unread());
    for (int i = 0; i < TILES; i++) {
        tile_t t;
        if (!C.tile[i]) continue;
        tile_read(i, &t);
        add(buf, cap, &n, "%s=%s（%s", TR(TILE[i].name), t.value, STATE[t.state]);
        if (t.usable) add(buf, cap, &n, "；点按=%s%s%s", t.cap, t.args[0] ? " " : "", t.args);
        if (t.usable && page_allowed(i)) add(buf, cap, &n, "；长按=%s", TILE[i].page);
        add(buf, cap, &n, "）；");
    }
    add(buf, cap, &n, "%s", C.kid || C.expanded ? "滑杆用 settings.set，两端减加一格；" : "展开（shell.more）后还有飞行模式、省电、叫名字、刷新屏幕和亮度、暖光滑杆；");
    if (n >= 0 && (size_t)n < cap) n += ink_notification_list_snapshot(buf + n, cap - (size_t)n, !C.kid);
    char activity[512];
    ink_activity_card_snapshot(activity, sizeof activity);
    if (C.kid) add(buf, cap, &n, "设置（齿轮）=settings.open {}；动作：settings.open、settings.set、system.set_volume、system.full_refresh、shell.bluetooth、shell.notify_open、notify.read、shell.close。");
    else add(buf, cap, &n, "动作：shell.more、shell.close、shell.members、shell.settings、settings.set、wifi.set_enabled、net.set_airplane、power.set_saving、"
             "system.full_refresh、shell.network、shell.bluetooth、shell.mobile、shell.sound、shell.display、shell.voice、shell.notify_open、notify.read、shell.tasks。%s", activity);
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

static void tile_leave(lv_event_t *e) {
    tile_t t;
    tile_read((int)(intptr_t)lv_event_get_user_data(e), &t), ink_shell_close(), call(t.cap, t.args);
}
static void tile_hold(lv_event_t *e) { /* AOSP: a long press opens the page with this switch; its release is no tap */
    const char *page = lv_event_get_user_data(e);
    lv_indev_wait_release(lv_indev_active()), ink_shell_close(), call(page, NULL);
}
static lv_obj_t *holder(int x, int y, int w, int h) {
    lv_obj_t *o = ink_ui_box(N.control, x, y, w, h, 0, INK_SURFACE_WHITE, false);
    return lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0), o;
}
static void tile_make(int i, const tile_t *t, int w) {
    lv_obj_t *o = ink_ui_switch_tile(C.tile[i], TILE[i].icon, TR(TILE[i].name), t->value, t->on, t->usable, 0, 0, w, NULL, NULL);
    if (t->usable && TILE[i].leave) lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE), lv_obj_add_event_cb(o, tile_leave, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    else if (t->usable) ink_shell_bind(o, t->cap, t->args);
    if (t->usable && page_allowed(i)) lv_obj_add_event_cb(o, tile_hold, LV_EVENT_LONG_PRESSED, (void *)TILE[i].page);
    snprintf(C.shown[i], sizeof C.shown[i], "%d%d%s", t->on, t->usable, t->value);
}
/* Two columns, 16 apart (design-lang-1, item 9); a name too long for half a row (English, a larger text size) takes the
 * whole row rather than being cut. */
static int tiles(int y) {
    int col = (width() - INK_GAP_M) / 2, room = col - 2 * INK_GAP_M - INK_ICON_SMALL - INK_GAP_M, right = 0;
    for (int i = 0; i < TILES; i++) {
        if (C.kid ? !TILE[i].child : !C.expanded && i >= FIRST) continue;
        bool wide = ink_ui_text_width(TR(TILE[i].name), INK_FONT_NOTE, INK_WEIGHT_MEDIUM) > room;
        if (wide && right) y += INK_CONTROL + INK_GAP_M, right = 0;
        C.w[i] = wide ? width() : col;
        C.tile[i] = holder(INK_MARGIN + right * (col + INK_GAP_M), y, C.w[i], INK_CONTROL);
        tile_t t;
        tile_read(i, &t);
        tile_make(i, &t, C.w[i]);
        if (wide || right) y += INK_CONTROL + INK_GAP_M, right = 0;
        else right = 1;
    }
    return right ? y + INK_CONTROL + INK_GAP_M : y;
}

/* The shared slider; a tap on − or + moves one step (BrightnessSlider.kt takes the value where it is let go). */
static void slider_step(lv_event_t *e) {
    lv_indev_t *in = lv_event_get_indev(e);
    lv_point_t p;
    lv_area_t a;
    if (!in) return;
    lv_indev_get_point(in, &p), lv_obj_get_coords(lv_event_get_target_obj(e), &a);
    int end = ink_ui_px(INK_CONTROL), step = p.x < a.x1 + end ? -10 : p.x > a.x2 - end ? 10 : 0;
    if (!step) { slider_changed(e); return; } /* on the track: where it was let go, as on the settings pages */
    const char *id = lv_event_get_user_data(e);
    int v = (int)ink_setting_get(id), top = (int)ink_setting_get("audio.child_max");
    v = (child() && !strcmp(id, "audio.volume") && v > top ? top : v) / 10 * 10 + step;
    char json[96];
    snprintf(json, sizeof json, "{\"id\":\"%s\",\"value\":%d}", id, v < 0 ? 0 : v > 100 ? 100 : v);
    call("settings.set", json);
}
static int slider_level(int i) { /* the volume slider is a child's, under its cap */
    int v = (int)ink_setting_get(SLIDER[i].id), top = (int)ink_setting_get("audio.child_max");
    return (i == 2 && v > top ? top : v) / 10;
}
static void slider_make(int i) {
    C.level[i] = slider_level(i);
    lv_obj_t *o = ink_ui_slider(C.slider[i], TR(SLIDER[i].name), C.level[i], 0, 0, width(), slider_step, (void *)SLIDER[i].id);
    if (i == 2) kid_volume_mark(o, (int)ink_setting_get("audio.child_max"));
}

static void date_text(char *out, size_t cap) { /* the day without the lunar date: the time is in the bar above */
    char clock[16], date[96];
    ink_shell_time_strings(clock, sizeof clock, date, sizeof date);
    const char *lunar = strstr(date, " · ");
    snprintf(out, cap, "%.*s", lunar ? (int)(lunar - date) : (int)sizeof date, date);
}
/* Kindle's head row: who uses it (换人), the day, the settings; the status bar above has the time and the battery. */
static int header(int y) {
    const char *person = ink_member_current()->name;
    int room = width() - INK_CONTROL;
    int name_w = LV_MIN(ink_ui_text_width(person, INK_FONT_LIST, INK_WEIGHT_BOLD), room / 2 - INK_ICON_LIST - INK_GAP_S);
    int who_w = LV_MAX(INK_CONTROL, INK_ICON_LIST + INK_GAP_S + name_w);
    lv_obj_t *who = ink_ui_box(N.control, INK_MARGIN, y, who_w, INK_CONTROL, 0, INK_SURFACE_WHITE, false);
    ink_ui_icon(who, "person", INK_ICON_LIST, INK_ICON_LINE, 0, (INK_CONTROL - INK_ICON_LIST) / 2, false);
    ink_ui_line(who, person, INK_FONT_LIST, INK_WEIGHT_BOLD, INK_ICON_LIST + INK_GAP_S, (INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2, name_w);
    if (!C.kid) ink_shell_bind(who, "shell.members", NULL);
    date_text(C.date_text, sizeof C.date_text);
    C.date = ink_ui_line(N.control, C.date_text, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN + who_w + INK_GAP_M,
                         y + (INK_CONTROL - ink_ui_line_height(INK_FONT_NOTE)) / 2, room - who_w - INK_GAP_M);
    lv_obj_set_style_text_align(C.date, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *gear = ink_ui_circle(N.control, "gear", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, y, INK_CONTROL, INK_ICON_LIST, INK_SURFACE_WHITE, NULL, NULL);
    ink_shell_bind(gear, C.kid ? "settings.open" : "shell.settings", C.kid ? "{}" : NULL); /* D-137: a child's home lists a child's rows */
    return y + INK_CONTROL;
}

/* The whole panel, for a new stage, member or notification: one block (FLASH_UI), as tall as what is in it. */
static void build(void) {
    lv_obj_clean(N.control);
    memset(C.tile, 0, sizeof C.tile), memset(C.slider, 0, sizeof C.slider);
    C.card = NULL, C.limit = N.control_count = 0;
    C.kid = child(), C.expanded = N.more && !C.kid, C.airplane = ink_setting_get("net.airplane") != 0;
    snprintf(C.member, sizeof C.member, "%s", ink_member_current()->id);
    ink_shell_statusbar(N.control, true);
    int y = tiles(header(INK_STATUS_BAR) + INK_GAP_M);
    for (int i = 0; i < SLIDERS; i++)
        if (C.kid || (C.expanded && i < 2)) /* a child's panel keeps its volume */
            C.slider[i] = holder(INK_MARGIN, y, width(), INK_SLIDER), slider_make(i), y += INK_SLIDER + INK_GAP_M;
    if (!C.expanded && !C.kid) { /* what is going on, under the first tiles as AOSP's media; children never had it */
        C.card = holder(0, y, INK_CANVAS_W, ink_ui_row_double() + INK_GAP_M);
        int card = ink_activity_card_draw(C.card, 0);
        if (card) y += card;
        else lv_obj_delete(C.card), C.card = NULL;
    }
    ink_activity_card_snapshot(C.activity, sizeof C.activity);
    if (!C.expanded) { /* the notifications that fit above the buttons; none, no section */
        int fits = (INK_CANVAS_H - INK_MARGIN - INK_CONTROL - y) / (ink_ui_row_double() + INK_GAP_M);
        if ((C.limit = fits < 2 ? fits : 2) > 0) y = ink_notification_list_draw(N.control, y, C.limit);
    }
    int half = (width() - INK_GAP_M) / 2;
    if (!C.expanded && !C.kid) ink_shell_bind(ink_ui_text_button(N.control, TR("展开"), INK_MARGIN, y, half, NULL, NULL), "shell.more", NULL);
    ink_shell_bind(ink_ui_text_button(N.control, TR("收起"), C.expanded || C.kid ? INK_MARGIN + half / 2 : INK_MARGIN + half + INK_GAP_M, y, half, NULL, NULL), "shell.close", NULL);
    y += INK_CONTROL;
    ink_ui_box(N.control, 0, y, INK_CANVAS_W, INK_STROKE_FOCUS, 0, INK_SURFACE_BLACK, false); /* the panel ends in a line */
    lv_obj_set_height(N.control, ink_ui_px(y + INK_STROKE_FOCUS));
    ink_ui_hint_obj(N.control, INK_REFRESH_FLASH_UI);
    ink_ui_hint_frame(INK_REFRESH_FLASH_UI);
}
static bool notes_moved(void) { /* the rows it would draw now are not the ones drawn; a row read since only says so */
    ink_db_cursor_t cursor = {0};
    ink_notify_view_t n;
    int count = 0;
    bool moved = false;
    for (; count < C.limit && ink_notify_next(&cursor, &n) == 1; count++) {
        moved |= count >= N.control_count || N.control_items[count].id != n.id;
        if (!moved) N.control_items[count].read = n.read;
    }
    return moved || count != N.control_count;
}

/* Called for every change that may show here: what changed is redrawn alone (a tile, a slider, the day, the card). */
static void control_redraw(void *unused) {
    (void)unused;
    if (N.control) draw_control();
}
void control_redraw_later(void) { ink_ui_after_press(control_redraw, NULL); }

void draw_control(void) {
    if (!N.control) return;
    if (C.airplane && !ink_setting_get("net.airplane")) { /* off again: back to the page, by touch or by the AI */
        ink_shell_close();
        return;
    }
    char activity[sizeof C.activity], date[sizeof C.date_text], shown[sizeof C.shown[0]];
    ink_activity_card_snapshot(activity, sizeof activity);
    bool card = strcmp(activity, C.activity) != 0;
    if (C.kid != child() || C.expanded != (N.more && !child()) || strcmp(C.member, ink_member_current()->id) || notes_moved() || (card && !C.card)) {
        build();
        return;
    }
    C.airplane = ink_setting_get("net.airplane") != 0;
    for (int i = 0; i < TILES; i++) {
        tile_t t;
        if (!C.tile[i]) continue;
        tile_read(i, &t);
        snprintf(shown, sizeof shown, "%d%d%s", t.on, t.usable, t.value);
        if (!strcmp(shown, C.shown[i])) continue;
        lv_obj_clean(C.tile[i]), tile_make(i, &t, C.w[i]);
        ink_ui_hint_obj(C.tile[i], INK_REFRESH_FAST);
    }
    for (int i = 0; i < SLIDERS; i++)
        if (C.slider[i] && slider_level(i) != C.level[i]) lv_obj_clean(C.slider[i]), slider_make(i), ink_ui_hint_obj(C.slider[i], INK_REFRESH_FAST);
    date_text(date, sizeof date);
    if (strcmp(date, C.date_text)) snprintf(C.date_text, sizeof C.date_text, "%s", date), lv_label_set_text(C.date, date), ink_ui_hint_obj(C.date, INK_REFRESH_FAST);
    if (!card) return;
    lv_obj_clean(C.card);
    if (!ink_activity_card_draw(C.card, 0)) { build(); return; }
    ink_activity_card_snapshot(C.activity, sizeof C.activity);
    ink_ui_hint_obj(C.card, INK_REFRESH_FAST);
}

/* A drag on the panel: down shows everything, up goes back a stage, run once the finger is off (the drag is no tap). */
static void gesture_job(void *arg) {
    (void)arg;
    if (N.control && C.gesture == LV_DIR_BOTTOM && !C.expanded && !C.kid) call("shell.more", NULL);
    else if (N.control && C.gesture == LV_DIR_TOP) call(C.expanded ? "shell.more" : "shell.close", NULL);
}
static void gesture(lv_event_t *e) {
    (void)e;
    C.gesture = lv_indev_get_gesture_dir(lv_indev_active());
    if (C.gesture != LV_DIR_TOP && C.gesture != LV_DIR_BOTTOM) return;
    lv_indev_wait_release(lv_indev_active());
    ink_exec_post(INK_EXEC_UI, &C.job, gesture_job, NULL);
}
static void outside(lv_event_t *e) { /* AOSP: a tap under the panel puts it away */
    lv_point_t p;
    lv_area_t a;
    lv_indev_get_point(lv_indev_active(), &p), lv_obj_get_coords(lv_event_get_target_obj(e), &a);
    if (p.y > a.y2) call("shell.close", NULL);
}

void ink_shell_control(void) {
    if (!ink_member_space_open() || N.control) return;
    N.more = false; /* every pull starts with the first stage */
    N.control = ink_ui_box(lv_layer_sys(), 0, 0, INK_CANVAS_W, INK_STATUS_BAR, 0, INK_SURFACE_WHITE, false);
    lv_obj_add_flag(N.control, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(N.control, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_ext_click_area(N.control, ink_ui_height()); /* the page under it takes no taps while it is out */
    lv_obj_add_event_cb(N.control, outside, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(N.control, gesture, LV_EVENT_GESTURE, NULL);
    ink_ui_overlay_add(child() ? &KID_CONTROL : &CONTROL, N.control);
    build();
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

static void kid_volume_mark(lv_obj_t *slider, int top) {
    ink_ui_box(slider, width() * top / 100, 0, INK_STROKE, INK_SLIDER, 0, INK_SURFACE_BLACK, false);
}
