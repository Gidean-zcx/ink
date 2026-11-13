/* control-1: the control centre after AOSP quick settings and the status bar after MuditaOS, on the real product.
 * INK_CONTROL_CASE picks the walk: touch, ai (the same walk through capabilities), kid, bar, shots. INK_CONTROL_TEXT
 * sets the text size for the pictures. Refresh areas are checked by tests/control_center.py from the trace. */
#include <stdlib.h>
#include "ink/rotation.h"
#include "ink/sys/notify.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static char kase[16], text_size[4], view[3072], value[64], shot_name[48];
static const char *wanted;
static lv_area_t area;
static bool seen;
static volatile bool captured;
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static void quiet(void) { CHECK(WAIT_FOR(settled(), 10000), "the panel settles"); }
static void inspect(void) { CHECK(ink_ui_snapshot(view, sizeof view, NULL) > 0, "the page has an AI snapshot"); }
static void look(void) { quiet(); on_ui(inspect); }
static bool has(const char *text) { return strstr(view, text) != NULL; }

/* A tile or slider: its name label, then its value label, in one box on the panel. */
static void find_in(lv_obj_t *o) {
    uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n && !seen; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_label_class) && !strcmp(lv_label_get_text(c), wanted) && i + 1 < n &&
            lv_obj_check_type(lv_obj_get_child(o, (int32_t)i + 1), &lv_label_class)) {
            lv_obj_update_layout(o);
            lv_obj_get_coords(o, &area);
            snprintf(value, sizeof value, "%s", lv_label_get_text(lv_obj_get_child(o, (int32_t)i + 1)));
            seen = true;
        } else find_in(c);
    }
}
static void find_ui(void) { seen = false; value[0] = 0; find_in(lv_layer_sys()); }
static bool tile(const char *name) { wanted = name; on_ui(find_ui); return seen; }
static const char *tile_value(const char *name) { return tile(name) ? value : ""; }
static void find_label_in(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o) && !seen; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_is_visible(c) && lv_obj_check_type(c, &lv_label_class) && !strcmp(lv_label_get_text(c), wanted)) {
            lv_obj_get_coords(c, &area);
            seen = true;
        } else find_label_in(c);
    }
}
static void find_label_ui(void) { seen = false; lv_obj_update_layout(lv_layer_sys()); find_label_in(lv_layer_sys()); find_label_in(lv_screen_active()); }
static bool label(const char *text) { wanted = text; on_ui(find_label_ui); return seen; }
static void find_back_in(lv_obj_t *o) {
    const char *cap = ink_shell_bound_cap(o);
    if (cap && !strcmp(cap, "shell.back")) { lv_obj_get_coords(o, &area); seen = true; }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o) && !seen; i++) find_back_in(lv_obj_get_child(o, (int32_t)i));
}
static void find_back(void) { seen = false; lv_obj_update_layout(lv_screen_active()); find_back_in(lv_screen_active()); }
static void tap_area(void) { tap((area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2); quiet(); }
static void touch(const char *name) { CHECK(tile(name) || label(name), "visible: %s", name); if (seen) tap_area(); }
static void hold(const char *name) {
    CHECK(tile(name), "tile to hold: %s", name);
    if (seen) { hw(HW_PRESS, (area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2); delay_ms(750); hw(HW_RELEASE, (area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2); }
    quiet();
}
static void swipe(int x0, int y0, int x1, int y1) {
    hw(HW_PRESS, x0, y0); delay_ms(100); hw(HW_PRESS, x1, y1); delay_ms(100); hw(HW_RELEASE, x1, y1); delay_ms(400); quiet();
}
static void capture(void *arg) { sim_display_snapshot(arg); captured = true; }
static void shot(const char *name) {
    quiet();
    snprintf(shot_name, sizeof shot_name, "%s-%s", name, text_size[0] ? text_size : "1");
    captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), capture, shot_name);
    CHECK(WAIT_FOR(captured, 2000), "picture %s", shot_name);
}
static bool top(const char *name) { return !strcmp(ink_ui_top_name(), name); }
static void ap_add(void *arg) { (void)arg; sim_wifi_ap_add("FAKE_AP", -50, "FAKE_PASSWORD_1"); }
static void ap_del(void *arg) { (void)arg; sim_wifi_ap_del("FAKE_AP"); }
static void world(void (*fn)(void *)) { vtk_event_at(vtk_now() + SIM_MS(1), fn, NULL); delay_ms(20); }
static void post(const char *title) {
    ink_notify_t n = {.channel = "message", .title = title, .body = "合成正文"};
    CHECK(ink_notify_post(&n, NULL) == 0, "post %s", title);
}
static void setting(const char *id, int64_t v) { ink_receipt_t rc; CHECK(ink_setting_set(id, v, INK_SRC_SYSTEM, &rc) == INK_RC_DONE, "set %s", id); }

/* A home network the device knows, switched off, two notifications: where every walk starts. */
static void start(bool notes) {
    world(ap_add);
    CHECK(BY_HAND("wifi.connect", "{\"ssid\":\"FAKE_AP\",\"password\":\"FAKE_PASSWORD_1\"}") && g_rc.status == INK_RC_DONE, "join the home network once");
    CHECK(BY_HAND("wifi.set_enabled", "{\"enabled\":false}") && g_rc.status == INK_RC_DONE, "Wi-Fi off");
    setting("display.light", 40); setting("notify.dnd", 0);
    if (notes) { post("合成通知一"); post("合成通知二"); }
    CHECK(WAIT_FOR(ink_setting_get("wifi.enabled") == 0, 3000), "Wi-Fi is off before the walk");
    quiet();
}
static bool first_stage(void) {
    return has("更多=0") && tile("Wi-Fi") && tile("蓝牙") && tile("移动网络") && tile("勿扰") && !tile("飞行模式") && !tile("亮度");
}
static bool second_stage(void) {
    return has("更多=1") && tile("Wi-Fi") && tile("飞行模式") && tile("省电") && tile("叫名字") && tile("刷新屏幕") && tile("亮度") && tile("暖光");
}

/* Where the trace's refreshes must stay: published for the runner, in the panel's native coordinates. */
static void publish(const char *what) {
    int w, h, x0, y0, x1, y1;
    hal_panel_info_t info;
    hal_display_get_info(&info);
    ink_display_native_size(&w, &h);
    ink_logical_to_native(w, h, info.rotation_deg, area.x1, area.y1, &x0, &y0);
    ink_logical_to_native(w, h, info.rotation_deg, area.x2, area.y2, &x1, &y1);
    char key[48];
    snprintf(key, sizeof key, "test.%s.x1", what); ink_probe_value(key, x0 < x1 ? x0 : x1);
    snprintf(key, sizeof key, "test.%s.y1", what); ink_probe_value(key, y0 < y1 ? y0 : y1);
    snprintf(key, sizeof key, "test.%s.x2", what); ink_probe_value(key, x0 < x1 ? x1 : x0);
    snprintf(key, sizeof key, "test.%s.y2", what); ink_probe_value(key, y0 < y1 ? y1 : y0);
}

static void touch_case(void) {
    start(true);
    swipe(342, 30, 342, 600); look(); /* a pull from the status bar */
    CHECK(top("Control") && first_stage(), "the first pull: four tiles: %.150s", view);
    CHECK(label("合成通知一") && label("合成通知二"), "the first stage has the notifications");
    shot("stage1");
    swipe(342, 420, 342, 1000); look(); /* a second pull */
    CHECK(second_stage(), "the second pull: every tile and the sliders: %.150s", view);
    shot("stage2");
    /* a tile redraws alone (D-127): the trace between the marks stays inside it */
    CHECK(tile("勿扰"), "the do-not-disturb tile"); publish("tile");
    ink_probe_mark("control.tile.before"); touch("勿扰"); quiet(); delay_ms(500); ink_probe_mark("control.tile.after");
    CHECK(ink_setting_get("notify.dnd") == 1 && !strcmp(tile_value("勿扰"), "开"), "the tap turns it on and the tile says so");
    touch("勿扰"); CHECK(ink_setting_get("notify.dnd") == 0, "and off again");
    /* Wi-Fi: connecting first, then the network's name; a failed join says so */
    touch("Wi-Fi");
    CHECK(WAIT_FOR(!strcmp(tile_value("Wi-Fi"), "连接中"), 5000), "the tile says connecting first (%s)", value);
    shot("walk-wifi-connecting");
    CHECK(WAIT_FOR(!strcmp(tile_value("Wi-Fi"), "FAKE_AP"), 10000), "then the network's name (%s)", value);
    shot("walk-wifi-joined");
    world(ap_del);
    CHECK(BY_HAND("wifi.connect", "{\"ssid\":\"FAKE_AP\",\"password\":\"FAKE_PASSWORD_1\"}") && g_rc.status == INK_RC_FAILED, "a join that fails");
    CHECK(WAIT_FOR(!strcmp(tile_value("Wi-Fi"), "没连上"), 5000), "the tile says it did not connect (%s)", value);
    shot("walk-wifi-failed");
    hold("Wi-Fi");
    CHECK(!strcmp(ink_ui_page_name(), "SetNetwork") && !top("Control"), "a long press opens the Wi-Fi page (%s)", ink_ui_top_name());
    shot("walk-wifi-page");
    on_ui(find_back); CHECK(seen, "the page's back button"); if (seen) tap_area(); /* the device has no back key */
    CHECK(strcmp(ink_ui_page_name(), "SetNetwork"), "back leaves it (%s over %s)", ink_ui_top_name(), ink_ui_page_name());
    tap(342, INK_STATUS_BAR / 2); look();
    CHECK(top("Control") && first_stage(), "a tap on the status bar opens the first stage");
    touch("展开"); look();
    CHECK(second_stage(), "展开 opens the second stage");
    CHECK(tile("亮度"), "the brightness slider");
    for (int i = 0; i < 3; i++) { tap(area.x2 - ink_ui_px(INK_CONTROL / 2), (area.y1 + area.y2) / 2); quiet(); CHECK(tile("亮度"), "the slider is back"); }
    CHECK(ink_setting_get("display.light") == 70, "three taps on + are three steps (%lld)", (long long)ink_setting_get("display.light"));
    shot("walk-brightness");
    touch("飞行模式");
    CHECK(WAIT_FOR(ink_setting_get("net.airplane") == 1, 4000) && top("Control"), "airplane mode on, the panel stays");
    CHECK(WAIT_FOR(!strcmp(tile_value("飞行模式"), "开"), 3000), "the tile says so");
    shot("walk-airplane-on");
    touch("飞行模式");
    CHECK(WAIT_FOR(ink_setting_get("net.airplane") == 0, 4000), "airplane mode off");
    CHECK(WAIT_FOR(!top("Control"), 3000), "the panel closes by itself");
    shot("walk-closed");
    /* a drag up goes back a stage, then away; a tap under the panel only puts it away */
    tap(342, INK_STATUS_BAR / 2); touch("展开"); look();
    CHECK(second_stage(), "the second stage again");
    swipe(342, 700, 342, 200); look();
    CHECK(top("Control") && first_stage(), "a drag up: back to the first stage");
    swipe(342, 400, 342, 100); quiet();
    CHECK(!top("Control"), "another: the panel goes");
    tap(342, INK_STATUS_BAR / 2); look();
    const char *page = ink_ui_page_name();
    tap(342, ink_ui_px(INK_CANVAS_H - INK_BLOCK_L)); quiet();
    CHECK(!top("Control") && !strcmp(ink_ui_page_name(), page), "a tap under the panel closes it and nothing under it reacts (%s)", ink_ui_page_name());
    /* the mobile network tile: another page, so the panel goes first */
    tap(342, INK_STATUS_BAR / 2); touch("移动网络"); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "SetMobile") && !top("Control"), "the mobile network page, the panel put away (%s)", ink_ui_top_name());
}

static void ai_case(void) {
    start(true);
    CHECK(AI("shell.control", NULL) && g_rc.status == INK_RC_DONE, "the AI opens the panel"); look();
    CHECK(first_stage() && has("Wi-Fi=关（关；点按=wifi.set_enabled {\"enabled\":true}；长按=shell.network）"), "its snapshot names the tiles: %.150s", view);
    CHECK(AI("shell.more", NULL) && g_rc.status == INK_RC_DONE, "the AI opens the second stage"); look();
    CHECK(second_stage(), "the same second stage");
    CHECK(AI("settings.set", "{\"id\":\"notify.dnd\",\"value\":1}") && g_rc.status == INK_RC_DONE, "do not disturb by the AI"); look();
    CHECK(has("勿扰=开（开") && !strcmp(tile_value("勿扰"), "开"), "the tile and the snapshot agree");
    CHECK(AI("settings.set", "{\"id\":\"notify.dnd\",\"value\":0}") && g_rc.status == INK_RC_DONE, "and off");
    CHECK(AI("wifi.set_enabled", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "Wi-Fi on by the AI");
    CHECK(WAIT_FOR((on_ui(inspect), has("Wi-Fi=连接中（连接中")), 5000), "connecting first: %.150s", view);
    CHECK(WAIT_FOR((on_ui(inspect), has("Wi-Fi=FAKE_AP（开")), 10000), "then the network's name: %.150s", view);
    world(ap_del);
    CHECK(AI("wifi.connect", "{\"ssid\":\"FAKE_AP\"}") && g_rc.status == INK_RC_FAILED, "a join that fails");
    CHECK(WAIT_FOR((on_ui(inspect), has("Wi-Fi=没连上（没连上")), 5000), "it did not connect: %.150s", view);
    CHECK(AI("shell.network", NULL) && g_rc.status == INK_RC_DONE, "the long press's page"); quiet();
    CHECK(!strcmp(ink_ui_page_name(), "SetNetwork") && !top("Control"), "the Wi-Fi page, the panel put away");
    CHECK(AI("shell.back", NULL) && g_rc.status == INK_RC_DONE, "back"); quiet();
    CHECK(AI("shell.control", NULL) && AI("shell.more", NULL), "the panel again, second stage"); look();
    for (int i = 0; i < 3; i++) {
        char args[64];
        snprintf(args, sizeof args, "{\"id\":\"display.light\",\"value\":%lld}", (long long)ink_setting_get("display.light") / 10 * 10 + 10);
        CHECK(AI("settings.set", args) && g_rc.status == INK_RC_DONE, "one step up, as +");
    }
    look();
    CHECK(ink_setting_get("display.light") == 70 && has("亮度=70") && !strcmp(tile_value("亮度"), "7"), "three steps, the slider shows 7");
    CHECK(AI("net.set_airplane", "{\"enabled\":true}") && g_rc.status == INK_RC_DONE, "airplane mode by the AI");
    CHECK(WAIT_FOR(ink_setting_get("net.airplane") == 1, 5000) && top("Control"), "on, the panel stays");
    CHECK(AI("net.set_airplane", "{\"enabled\":false}") && g_rc.status == INK_RC_DONE, "and off");
    CHECK(WAIT_FOR(ink_setting_get("net.airplane") == 0, 5000), "airplane mode off");
    CHECK(WAIT_FOR(!top("Control"), 3000), "the panel closes by itself, as after a tap");
}

static void kid_case(void) {
    CHECK(ink_member_switch("kid") == 0, "the child takes the device"); quiet();
    int64_t wifi = ink_setting_get("wifi.enabled");
    CHECK(BY_HAND("shell.control", NULL) && g_rc.status == INK_RC_DONE, "the child's panel"); look();
    CHECK(top("KidControl") && tile("Wi-Fi") && tile("蓝牙") && tile("刷新屏幕") && tile("亮度") && tile("暖光") && tile("音量"), "the child's tiles and sliders");
    CHECK(!tile("移动网络") && !tile("勿扰") && !tile("飞行模式") && !tile("省电") && !tile("叫名字"), "nothing a child cannot use");
    CHECK(!strcmp(tile_value("Wi-Fi"), "受家人限制") && has("Wi-Fi=受家人限制（受家人限制）"), "what the family keeps says so");
    touch("Wi-Fi");
    CHECK(ink_setting_get("wifi.enabled") == wifi && top("KidControl"), "a tap on it does nothing");
    CHECK(AI("wifi.set_enabled", wifi ? "{\"enabled\":false}" : "{\"enabled\":true}") && g_rc.status == INK_RC_REJECTED &&
          ink_setting_get("wifi.enabled") == wifi, "nor the AI");
    shot("kid");
}

/* The image an icon of this name gets: the same descriptor every time. */
static const void *icon_src;
static const char *icon_name;
static void icon_ref(void) {
    lv_obj_t *o = ink_ui_icon(lv_layer_top(), icon_name, INK_ICON_SMALL, INK_ICON_LINE, 0, 0, false);
    icon_src = lv_image_get_src(o);
    lv_obj_delete(o);
}
static lv_obj_t *bar;
static void find_bar(lv_obj_t *o) {
    const char *cap = ink_shell_bound_cap(o);
    if (cap && !strcmp(cap, "shell.control")) bar = o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o) && !bar; i++) find_bar(lv_obj_get_child(o, (int32_t)i));
}
static bool icon_found, bar_text_found;
static const char *bar_text;
static void bar_scan(lv_obj_t *o) { /* the icons and words the bar draws, at any depth */
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_image_class) && lv_image_get_src(c) == icon_src) icon_found = true;
        if (bar_text && lv_obj_check_type(c, &lv_label_class) && !strcmp(lv_label_get_text(c), bar_text)) bar_text_found = true;
        bar_scan(c);
    }
}
static void bar_look(void) {
    bar = NULL; icon_found = bar_text_found = false;
    find_bar(lv_screen_active());
    if (bar) bar_scan(bar);
}
static bool bar_has(const char *name) { icon_name = name; bar_text = NULL; on_ui(icon_ref); on_ui(bar_look); return icon_found; }
static bool bar_says(const char *text) { bar_text = text; icon_src = NULL; on_ui(bar_look); return bar_text_found; }
/* The pull-down entry: as tall as the bar, and no other tap target reaches into it. */
static int overlaps, bar_h;
static void entry_in(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    lv_area_t a;
    if (o != bar && lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE) && lv_obj_get_parent(o) && o != lv_screen_active()) {
        lv_obj_get_click_area(o, &a);
        if (a.y1 < ink_ui_px(INK_STATUS_BAR) && a.y2 >= 0) overlaps++;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) if (lv_obj_get_child(o, (int32_t)i) != bar) entry_in(lv_obj_get_child(o, (int32_t)i));
}
static void entry(void) {
    bar = NULL; overlaps = 0;
    lv_obj_update_layout(lv_screen_active());
    find_bar(lv_screen_active());
    lv_area_t a;
    if (bar) { lv_obj_get_click_area(bar, &a); bar_h = a.y2 - a.y1 + 1; }
    entry_in(lv_screen_active());
}
static void entry_check(const char *page) {
    on_ui(entry);
    CHECK(bar && bar_h >= ink_ui_px(INK_CONTROL) && !overlaps, "%s: the pull-down entry is %d high and %d targets reach into it", page, bar_h, overlaps);
}

static void bar_case(void) {
    quiet();
    ink_state_set_bool(ink_state_find("bt.connected"), false); quiet();
    CHECK(!bar_has("bluetooth"), "no Bluetooth icon while nothing is connected");
    ink_state_set_bool(ink_state_find("bt.connected"), true); quiet();
    CHECK(bar_has("bluetooth"), "the icon once headphones connect");
    ink_state_set_bool(ink_state_find("bt.connected"), false); quiet();
    CHECK(!bar_has("bluetooth"), "and gone when they go");
    CHECK(WAIT_FOR(bar_says("4G"), 10000) && !bar_has("plane"), "the network as it is, once the card is registered");
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":true}") && WAIT_FOR(ink_setting_get("net.airplane") == 1, 4000), "airplane mode on"); quiet();
    CHECK(WAIT_FOR(bar_has("plane"), 3000) && !bar_says("4G") && !bar_says("无服务") && !bar_has("wifi"), "the plane in place of the network");
    CHECK(BY_HAND("net.set_airplane", "{\"enabled\":false}") && WAIT_FOR(ink_setting_get("net.airplane") == 0, 4000), "airplane mode off"); quiet();
    CHECK(WAIT_FOR(!bar_has("plane"), 3000) && WAIT_FOR(bar_says("4G"), 10000), "the network back in its place"); quiet();
    entry_check("home");
    /* a new minute refreshes the time alone */
    wanted = NULL; quiet();
    on_ui(entry);
    lv_obj_t *time = NULL;
    for (uint32_t i = 0; bar && i < lv_obj_get_child_count(bar) && !time; i++)
        if (lv_obj_check_type(lv_obj_get_child(bar, (int32_t)i), &lv_label_class) && strchr(lv_label_get_text(lv_obj_get_child(bar, (int32_t)i)), ':'))
            time = lv_obj_get_child(bar, (int32_t)i);
    CHECK(time != NULL, "the time label");
    if (time) { /* the time's column of the bar; the next minute may be a digit wider */
        lv_obj_get_coords(time, &area);
        area.x1 = 0, area.y1 = 0, area.x2 += ink_ui_px(INK_FONT_STATUS), area.y2 = ink_ui_px(INK_STATUS_BAR) - 1;
        publish("clock");
    }
    int64_t now = ink_time_now_s();
    ink_probe_mark("control.minute.before");
    delay_ms((int)(60 - now % 60) * 1000 + 800); quiet();
    ink_probe_mark("control.minute.after");
    CHECK(ink_time_now_s() / 60 != now / 60, "a minute went by");
    CHECK(BY_HAND("shell.network", NULL) && g_rc.status == INK_RC_DONE, "a page with a back button"); quiet();
    entry_check("SetNetwork");
}

static void shots_case(void) {
    start(false);
    CHECK(BY_HAND("shell.control", NULL), "the panel without notifications"); look();
    CHECK(first_stage() && !label("合成通知一") && !label("还没有通知"), "no notification section, no empty text");
    shot("empty");
    CHECK(BY_HAND("shell.close", NULL), "close"); quiet();
    shot("statusbar");
    post("合成通知一"); post("合成通知二"); quiet();
    CHECK(BY_HAND("shell.control", NULL), "the panel"); look();
    CHECK(label("合成通知一"), "with its notifications");
    shot("stage1");
    CHECK(BY_HAND("shell.more", NULL), "the second stage"); look();
    shot("stage2");
    CHECK(BY_HAND("shell.close", NULL), "close"); quiet();
    kid_case();
}

static void run(void *arg) {
    (void)arg;
    quiet();
    if (text_size[0]) { setting("display.font_size", atoi(text_size)); quiet(); }
    if (!strcmp(kase, "touch")) touch_case();
    else if (!strcmp(kase, "ai")) ai_case();
    else if (!strcmp(kase, "kid")) kid_case();
    else if (!strcmp(kase, "bar")) bar_case();
    else if (!strcmp(kase, "shots")) shots_case();
    else CHECK(false, "a known case: %s", kase);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    const char *c = getenv("INK_CONTROL_CASE"), *t = getenv("INK_CONTROL_TEXT");
    snprintf(kase, sizeof kase, "%s", c ? c : "touch");
    snprintf(text_size, sizeof text_size, "%s", t ? t : "");
    product_main(); test_start_home();
    osal_task_cfg_t cfg = {.name = "control-center", .fn = run, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the test driver");
}
