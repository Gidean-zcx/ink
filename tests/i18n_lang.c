/* i18n-1: the language setting, the first-boot pages and the settings home in English and back,
 * kept across a restart, a key without a translation, and the AI request's language. */
#include <stdlib.h>
#include "ink/sys/ai.h"
#include "ink/sys/i18n.h"
#include "ink/ui_shell.h"
#include "ink/sys/textfield.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"

static const char SECRET[] = "test-secret-123";
static const char *label;
static int label_x, label_y;
static volatile bool captured;
static char labels[4096];
static int crowded;
static char crowded_what[160];

static void locate(lv_obj_t *root) {
    if (!lv_obj_is_visible(root)) return;
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), label)) {
        lv_area_t a; lv_obj_get_coords(lv_obj_get_parent(root), &a);
        label_x = (a.x1 + a.x2) / 2; label_y = (a.y1 + a.y2) / 2;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) locate(lv_obj_get_child(root, (int32_t)i));
}
static void locate_label(void) {
    label_x = label_y = -1; lv_obj_update_layout(lv_screen_active()); lv_obj_update_layout(lv_layer_sys());
    locate(lv_screen_active()); locate(lv_layer_sys());
}
static bool visible(const char *name) { label = name; on_ui(locate_label); return label_x >= 0; }
static void press(const char *name) {
    CHECK(visible(name), "visible control: %s", name);
    if (label_x >= 0) tap(label_x, label_y);
}
static void read_page(void) { read_screen(); snprintf(g_top, sizeof g_top, "%s", ink_ui_page_name()); }
static bool at(const char *name) { on_ui(read_page); return !strcmp(g_top, name); }
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static bool ready(const char *name) {
    bool ok = WAIT_FOR(at(name), 18000) && WAIT_FOR(settled(), 10000);
    CHECK(ok, "arrive at %s (actual %s)", name, g_top);
    return ok;
}

/* Every visible label's text: whole (a one-line label is not cut short, a wrapped one has room for all
 * its lines) and apart from every other label's text. */
static lv_area_t boxes[96];
static int nboxes;
static void text_area(lv_obj_t *l, lv_area_t *out) {
    const char *t = lv_label_get_text(l);
    const lv_font_t *f = lv_obj_get_style_text_font(l, LV_PART_MAIN);
    int32_t letter = lv_obj_get_style_text_letter_space(l, LV_PART_MAIN), line = lv_obj_get_style_text_line_space(l, LV_PART_MAIN);
    lv_area_t a; lv_obj_get_content_coords(l, &a);
    int32_t w = lv_area_get_width(&a);
    lv_point_t one, wrapped;
    lv_text_get_size(&one, t, f, letter, line, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_text_get_size(&wrapped, t, f, letter, line, w, LV_TEXT_FLAG_NONE);
    bool single = lv_label_get_long_mode(l) != LV_LABEL_LONG_MODE_WRAP;
    size_t len = strlen(t);
    bool dots = lv_label_get_long_mode(l) == LV_LABEL_LONG_MODE_DOTS && len >= 3 && !strcmp(t + len - 3, "...");
    if (dots || (single ? one.x > w + 1 : wrapped.y > lv_area_get_height(&a) + 1)) {
        crowded++; snprintf(crowded_what, sizeof crowded_what, "cut: %.120s", t);
    }
    int32_t tw = single ? LV_MIN(one.x, w) : LV_MIN(wrapped.x, w), th = single ? one.y : wrapped.y;
    lv_text_align_t align = lv_obj_get_style_text_align(l, LV_PART_MAIN);
    out->x1 = align == LV_TEXT_ALIGN_RIGHT ? a.x2 - tw : align == LV_TEXT_ALIGN_CENTER ? a.x1 + (w - tw) / 2 : a.x1;
    out->x2 = out->x1 + tw - 1; out->y1 = a.y1; out->y2 = a.y1 + th - 1;
}
static void walk(lv_obj_t *o) {
    if (!lv_obj_is_visible(o) || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && lv_label_get_text(o)[0]) {
        size_t n = strlen(labels);
        snprintf(labels + n, sizeof labels - n, "%s|", lv_label_get_text(o));
        lv_area_t a; text_area(o, &a);
        for (int i = 0; i < nboxes; i++) {
            if (a.x1 <= boxes[i].x2 && boxes[i].x1 <= a.x2 && a.y1 <= boxes[i].y2 && boxes[i].y1 <= a.y2) {
                crowded++; snprintf(crowded_what, sizeof crowded_what, "overlap: %.120s", lv_label_get_text(o));
            }
        }
        if (a.y2 >= lv_display_get_vertical_resolution(NULL) || a.x2 >= lv_display_get_horizontal_resolution(NULL)) {
            crowded++; snprintf(crowded_what, sizeof crowded_what, "off screen: %.120s", lv_label_get_text(o));
        }
        if (nboxes < (int)(sizeof boxes / sizeof *boxes)) boxes[nboxes++] = a;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) walk(lv_obj_get_child(o, (int32_t)i));
}
/* A visible label with exactly this text on the system layer, where the AI panel is. */
static const char *seek_text;
static bool seek_seen;
static void seek_in(lv_obj_t *o) {
    if (seek_seen || !lv_obj_is_visible(o) || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), seek_text)) { seek_seen = true; return; }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) seek_in(lv_obj_get_child(o, (int32_t)i));
}
static void seek_sys(void) { seek_seen = false; lv_obj_update_layout(lv_layer_sys()); seek_in(lv_layer_sys()); }
static bool on_panel(const char *text) { seek_text = text; on_ui(seek_sys); return seek_seen; }
static void read_labels(void) {
    labels[0] = 0; nboxes = crowded = 0; crowded_what[0] = 0;
    lv_obj_update_layout(lv_screen_active()); walk(lv_screen_active());
}
static bool shows(const char *text) {
    char want[160]; snprintf(want, sizeof want, "|%s|", text);
    char all[4100]; snprintf(all, sizeof all, "|%s", labels);
    return strstr(all, want) != NULL;
}
static void snapshot_event(void *arg) {
    if (!settled()) { CHECK(false, "all scene snapshots must be fully presented"); return; }
    sim_display_snapshot((const char *)arg); captured = true;
}
/* A settled frame named `name`: its labels are whole and apart, and it is saved as snap-<name>.png. */
static void capture(const char *page, const char *name) {
    if (!ready(page)) return;
    on_ui(read_labels);
    CHECK(!crowded, "%s: every label is whole and apart (%d, last %s)", name, crowded, crowded_what);
    captured = false;
    vtk_event_at(vtk_now() + SIM_MS(1), snapshot_event, (void *)name);
    CHECK(WAIT_FOR(captured, 2000), "settled scene captured: %s", name);
    char metric[80]; snprintf(metric, sizeof metric, "test.snapshot.%s", name); ink_probe_count(metric);
}
static void environment(void *arg) { (void)arg; sim_wifi_ap_add("test-wifi", -42, SECRET); }
static void native_type(const char *value) {
    for (const char *p = value; *p; p++) {
        char args[40]; snprintf(args, sizeof args, "{\"code\":%u}", (unsigned char)*p);
        CHECK(BY_HAND("keyboard.key", args) && g_rc.status == INK_RC_DONE, "keyboard accepts a synthetic password character");
    }
}
static bool language(const char *code) {
    return WAIT_FOR(!strcmp(ink_lang_code(), code) && settled(), 3000);
}

static const char *const ZH_ROWS[] = {"设置", "网络和 SIM 卡", "蓝牙", "显示和亮度", "声音", "语音和 AI", "锁屏和壁纸", "存储和备份", "关于和更新"};
static const char *const EN_ROWS[] = {"Settings", "Network & SIM", "Bluetooth", "Display", "Sound", "Voice & AI",
    "Lock Screen", "Storage & Backup", "About & Updates"};
static void settings_home(const char *const rows[], const char *const absent[], const char *name) {
    CHECK(BY_HAND("shell.settings", NULL) && g_rc.status == INK_RC_DONE, "open the settings home");
    capture("Settings", name);
    for (int i = 0; i < 9; i++) {
        CHECK(shows(rows[i]), "%s shows %s", name, rows[i]);
        CHECK(!shows(absent[i]), "%s no longer shows %s", name, absent[i]);
    }
}

static const char *missing_key = "这句测试用的话没有译文";
static const char *missing_result;
static void look_up_missing(void) { missing_result = ink_tr(missing_key); }
static void ask(void) { ink_ai_ask_text("这一页讲了什么？"); }

static void finish(void) {
    for (int i = 0; i < 300 && ink_storage_pending(); i++) delay_ms(10);
    delay_ms(1500); /* the settings file is written one second after the last change */
    CHECK(WAIT_FOR(ink_storage_pending() == 0 && settled(), 15000), "the language setting is on flash");
    ink_probe_count("test.finished");
}

static void first_run(void) {
    CHECK(ready("FirstBoot") && strstr(g_snap, "你好"), "a new device starts in Chinese");
    CHECK(ink_setting_get("system.language") == 0 && !strcmp(ink_lang_code(), "zh"), "Chinese is the default language");
    capture("FirstBoot", "FirstBoot-zh");

    /* The AI and the buttons use the same capability; the open page is drawn again at once. */
    CHECK(AI("settings.set", "{\"id\":\"system.language\",\"value\":\"English\"}") && g_rc.status == INK_RC_DONE,
          "the AI switches the language through settings.set");
    CHECK(language("en") && WAIT_FOR(at("FirstBoot") && strstr(g_snap, "\"title\":\"Hello\""), 3000),
          "FirstBoot is drawn again in English without a restart (%.150s)", g_snap);
    capture("FirstBoot", "FirstBoot-en");
    CHECK(shows("Hello") && shows("Set up this device with your phone") && shows("Set up on this device") && !shows("你好"),
          "FirstBoot shows English text");

    press("Set up on this device");
    CHECK(WAIT_FOR(at("BootWifi") && strstr(g_snap, "test-wifi"), 8000), "the English Wi-Fi page finds the network");
    capture("BootWifi", "BootWifi-en");
    CHECK(shows("Choose Wi-Fi") && shows("Other network") && shows("Search again") && strstr(g_snap, "Nearby network"),
          "the Wi-Fi page and its snapshot are English");
    press("test-wifi");
    CHECK(ready("BootWifiPass"), "the password page opens");
    native_type(SECRET);
    CHECK(BY_HAND("keyboard.key", "{\"code\":10}") && g_rc.status == INK_RC_DONE, "submit the synthetic password");
    capture("BootNoHub", "BootNoHub-en");
    CHECK(shows("Can't find the family server") && shows("Start now") && shows("Try again"),
          "the longest first-boot message fits in English");
    press("Start now");
    capture("BootHello", "BootHello-en");
    CHECK(shows("Just ask") && shows("Skip") && shows("Read a sample"), "the welcome page is English");

    /* A key without a translation shows in Chinese, never blank, and is counted. */
    double before = sim_metric_value("i18n.missing");
    on_ui(look_up_missing);
    CHECK(missing_result == missing_key && sim_metric_value("i18n.missing") == before + 1,
          "an untranslated key falls back to its Chinese text and counts i18n.missing");

    press("Skip");
    CHECK(ready("home"), "setup finished");

    /* The AI request carries the language; the stand-in hub answers in it. */
    double answers = sim_metric_value("ai.answers");
    on_ui(ask);
    CHECK(WAIT_FOR(sim_metric_value("ai.answers") > answers, 20000), "the hub answers");
    const char *asked = sim_hub_last_ask();
    CHECK(asked && strstr(asked, "\"lang\":\"en\""), "the AI request says the interface language (%.150s)", asked ? asked : "");
    CHECK(WAIT_FOR(at("home") && strstr(g_snap, "This page is about Xiaoman"), 5000), "the answer comes in English (%.150s)", g_snap);
    capture("home", "AiAnswer-en");
    /* The panel reads the service's status by its Chinese key and translates it to show it. Given the shown English text
     * it never knew the answer was done (nor a tap-to-talk turn, a recording, failed audio): the finished answer kept
     * the hint in its status line, which the Chinese panel clears (batch audit, 2026-11-11). */
    CHECK(on_panel("From this page") && !on_panel("Press AI to ask more, HOME to close"),
          "the finished answer looks as in Chinese: its source, and nothing left in the status line");
    CHECK(BY_HAND("ai.panel.hide", NULL) && g_rc.status == INK_RC_DONE, "close the answer");

    settings_home(EN_ROWS, ZH_ROWS, "Settings-en");
    CHECK(strstr(g_snap, "语言=en"), "the settings snapshot tells the AI the language");
    for (int wake = 2; wake >= 0; wake--) { /* every value of the voice row fits beside its name */
        char args[64]; snprintf(args, sizeof args, "{\"id\":\"voice.handsfree\",\"value\":%d}", wake);
        CHECK(BY_HAND("settings.set", args) && g_rc.status == INK_RC_DONE && WAIT_FOR(settled(), 3000), "wake setting %d", wake);
        on_ui(read_labels);
        CHECK(!crowded && shows("Voice & AI"), "voice row %d is whole (%d, last %s)", wake, crowded, crowded_what);
    }

    /* The row in About & Updates: the same capability by hand. */
    CHECK(BY_HAND("shell.about", NULL) && ready("SetAbout"), "open about");
    for (int i = 0; i < 4 && !strstr(g_snap, "system.language"); i++) {
        CHECK(BY_HAND("shell.settings_next", NULL) && g_rc.status == INK_RC_DONE, "page to the language row");
        ready("SetAbout");
    }
    CHECK(strstr(g_snap, "\"id\":\"system.language\"") && strstr(g_snap, "\"label\":\"Language\"") &&
          strstr(g_snap, "\"value\":\"English\""), "about lists the language row with its current value (%.150s)", g_snap);
    capture("SetAbout", "SetAbout-en");
    press("Language");
    CHECK(language("zh"), "the row switches the language");
    CHECK(WAIT_FOR(at("SetAbout") && strstr(g_snap, "\"label\":\"语言\"") && strstr(g_snap, "\"value\":\"中文\""), 3000),
          "about is drawn again in Chinese (%.150s)", g_snap);

    settings_home(ZH_ROWS, EN_ROWS, "Settings-zh");
    CHECK(strstr(g_snap, "语言=zh"), "back to Chinese in the snapshot");
    CHECK(AI("settings.set", "{\"id\":\"system.language\",\"value\":1}") && g_rc.status == INK_RC_DONE && language("en"),
          "English again before the restart");
    CHECK(WAIT_FOR(at("Settings") && strstr(g_snap, "语言=en"), 3000), "the settings home follows at once");
    on_ui(read_labels);
    CHECK(shows("Settings") && shows("About & Updates"), "the open settings home is redrawn in English");
    finish();
}

static void after_restart(void) {
    CHECK(ready("home"), "the finished device starts at home");
    CHECK(ink_setting_get("system.language") == 1 && !strcmp(ink_lang_code(), "en"), "English is kept across the restart");
    settings_home(EN_ROWS, ZH_ROWS, "Settings-en-restart");
    CHECK(BY_HAND("settings.set", "{\"id\":\"system.language\",\"value\":0}") && g_rc.status == INK_RC_DONE && language("zh"),
          "switch back to Chinese by hand");
    CHECK(WAIT_FOR(at("Settings") && strstr(g_snap, "语言=zh"), 3000), "the settings home follows at once");
    on_ui(read_labels);
    for (int i = 0; i < 9; i++) CHECK(shows(ZH_ROWS[i]), "Chinese again: %s", ZH_ROWS[i]);
    finish();
}

static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(settled(), 15000), "initial page settles");
    vtk_event_at(vtk_now() + SIM_MS(1), environment, NULL); delay_ms(10);
    const char *phase = getenv("INK_I18N_PHASE");
    if (phase && !strcmp(phase, "restart")) after_restart();
    else first_run();
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "i18n-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start the i18n driver");
}
