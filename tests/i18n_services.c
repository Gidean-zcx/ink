/* Real service messages on shell surfaces; synthetic modem and notification events only. */
#include <stdlib.h>
#include "ink/sys/i18n.h"
#include "ink/sys/notify.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/task.h"
#include "ink/ui_shell.h"
#define ink_main product_main
#include "../system/boot/main.c"
#undef ink_main
#include "integration/whole_product.h"
#ifdef INK_I18N_REVERSE
#undef TR
#define TR(s) (s)
#endif
#include "../system/policy/confirm.c"
#undef TR
#define TR(s) ink_tr(s)

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
static void read_labels(void) {
    labels[0] = 0; nboxes = crowded = 0; crowded_what[0] = 0;
    lv_obj_update_layout(lv_screen_active()); walk(ink_ui_overlay_up() ? lv_layer_sys() : lv_screen_active());
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

static bool has_label(const char *s) { on_ui(read_labels); return strstr(labels, s) != NULL; }
static void lang(int value) {
    char args[64]; snprintf(args, sizeof args, "{\"id\":\"system.language\",\"value\":%d}", value);
    CHECK(BY_HAND("settings.set", args) && g_rc.status == INK_RC_DONE, "set language %d", value);
    CHECK(WAIT_FOR(settled(), 10000), "language redraw settles");
}
static void emit(const char *id, const char *json) {
    ink_args_t *a = osal_calloc(1, sizeof *a, OSAL_MEM_EXTERNAL);
    char why[80];
    CHECK(a && !ink_args_from_json(a, json, strlen(json), NULL, why, sizeof why), "synthetic event arguments parse");
    if (a) { ink_cap_emit(id, a); osal_free(a); }
}
static uint32_t notice(const char *title) {
    ink_db_cursor_t cursor = {0}; ink_notify_view_t n;
    while (ink_notify_next(&cursor, &n) == 1) if (!strcmp(n.title, title)) return n.id;
    return 0;
}
static bool contains_notice(const char *title, const char *body) {
    ink_db_cursor_t cursor = {0}; ink_notify_view_t n;
    while (ink_notify_next(&cursor, &n) == 1) if (!strcmp(n.title, title) && !strcmp(n.body, body)) return true;
    return false;
}
static void ui_capture(const char *name) {
    CHECK(WAIT_FOR(settled(), 10000), "service frame settles: %s", name);
    on_ui(read_labels);
    CHECK(!crowded, "%s has no clipped, overlapping or off-screen labels: %d %.120s", name, crowded, crowded_what);
    captured = false; vtk_event_at(vtk_now() + SIM_MS(1), snapshot_event, (void *)name);
    CHECK(WAIT_FOR(captured, 2000), "capture %s", name);
    char metric[96]; snprintf(metric, sizeof metric, "test.snapshot.%s", name); ink_probe_count(metric);
}
static void confirmations(void) {
    CHECK(AI("call.dial", "{\"number\":\"00000000000\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "AI call requires confirmation");
    ink_confirm_view_t card;
    CHECK(ink_confirm_list(&card, 1) == 1 && !strcmp(card.title, "Call") && !strcmp(card.detail, "Call 00000000000"),
          "translated confirmation title and template: %.80s / %.100s", card.title, card.detail);
    CHECK(WAIT_FOR(ink_ui_overlay_up(), 3000), "call card is shown"); ui_capture("ConfirmCall-en");
    CHECK(ink_confirm_decide(card.token, false, INK_SRC_TOUCH) == 0, "cancel synthetic call without dialing");
    CHECK(WAIT_FOR(!ink_ui_overlay_up(), 3000), "call card closes");
    CHECK(AI("settings.set", "{\"id\":\"audio.child_max\",\"value\":50}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "high-risk setting requires confirmation");
    CHECK(ink_confirm_list(&card, 1) == 1 && !strcmp(card.detail, "Set audio.child_max to 50"), "setting ask uses English template");
    CHECK(WAIT_FOR(ink_ui_overlay_up(), 3000), "setting card is shown"); ui_capture("ConfirmSetting-en");
    CHECK(ink_confirm_decide(card.token, false, INK_SRC_TOUCH) == 0, "cancel high-risk setting");
    CHECK(WAIT_FOR(!ink_ui_overlay_up(), 3000), "setting card closes");
}
static void notifications(void) {
    CHECK(ink_notify_clear() == 0, "clean synthetic inbox");
    emit("battery.low", "{\"level\":1,\"percent\":19}");
    CHECK(WAIT_FOR(contains_notice("Low battery", "19%. Charge now"), 5000), "background notification is generated in English");
    CHECK(BY_HAND("shell.notifications", NULL) && ready("Notifications"), "open real inbox");
    capture("Notifications", "Notifications-en");
    CHECK(shows("Low battery") && shows("19%. Charge now"), "English notification title and body displayed");
    uint32_t saved = notice("Low battery");
    lang(0);
    CHECK(contains_notice("Low battery", "19%. Charge now") && notice("Low battery") == saved, "stored notification retains its original language");
    emit("reminder.fired", "{\"id\":42,\"title\":\"SYNTH reminder\"}");
    CHECK(WAIT_FOR(contains_notice("提醒", "SYNTH reminder"), 5000), "new notification follows Chinese after switch");
    CHECK(BY_HAND("shell.notifications", NULL) && ready("Notifications"), "refresh real inbox after switch");
    capture("Notifications", "Notifications-zh");
    CHECK(ink_notify_clear() == 0, "clear both language fixtures");
    lang(1);
    CHECK(BY_HAND("shell.notify_settings", NULL) && ready("SetNotifications"), "open real channel settings");
    capture("SetNotifications", "Channels-en");
    CHECK(shows("System") && shows("Messages"), "built-in channel labels translated");
    CHECK(BY_HAND("shell.page_next", NULL) && WAIT_FOR(settled(), 5000), "channel second page");
    capture("SetNotifications", "Channels2-en");
    CHECK(BY_HAND("system.home", NULL) && ready("home"), "return home");
}
static void interruptions(void) {
    emit("battery.low", "{\"level\":2,\"percent\":8}");
    CHECK(WAIT_FOR(ink_interrupt_active(), 5000) && BY_HAND("interrupt.list", NULL) &&
          strstr(g_result, "Very low battery") && strstr(g_result, "8%. Charge now"), "English low battery service interruption");
    ui_capture("Battery-en");
    emit("battery.low", "{\"level\":0,\"percent\":90}");
    CHECK(WAIT_FOR(!ink_interrupt_active(), 5000), "charging ends low battery interruption");
    snprintf(g_hw.num, sizeof g_hw.num, "00000000000"); hw(HW_CALL, 0, 0);
    CHECK(WAIT_FOR(ink_interrupt_active() && has_label("Answer"), 6000), "incoming call uses English action");
    ui_capture("IncomingCall-en");
    CHECK(BY_HAND("call.answer", NULL) && g_rc.status == INK_RC_DONE, "answer synthetic modem call");
    CHECK(WAIT_FOR(has_label("In call"), 5000), "answered call status translated");
    ui_capture("ActiveCall-en");
    hw(HW_HANGUP, 0, 0); CHECK(WAIT_FOR(!ink_interrupt_active(), 5000), "synthetic call ends");
    emit("alarm.fired", "{\"id\":42,\"title\":\"SYNTH alarm\"}");
    CHECK(WAIT_FOR(ink_interrupt_active() && has_label("Snooze 10 min"), 5000), "alarm has translated actions");
    ui_capture("Alarm-en"); emit("alarm.ended", "{\"id\":42}");
    CHECK(WAIT_FOR(!ink_interrupt_active(), 5000), "synthetic alarm ends");
}
static void receipts(void) {
    CHECK(BY_HAND("system.full_refresh", NULL) && !strcmp(g_rc.say, "Screen refreshed"), "service receipt translated");
    CHECK(BY_HAND("settings.get", "{\"id\":\"audio.call_ring\"}") && !strcmp(g_rc.say, "Ringtone: Clear"), "setting name and enum value translated");
    CHECK(BY_HAND("settings.get", "{\"id\":\"audio.key_tone\"}") && !strcmp(g_rc.say, "Key sound: Off"), "boolean setting translated");
    lang(0);
    CHECK(BY_HAND("system.full_refresh", NULL) && !strcmp(g_rc.say, "屏幕刷新一下"), "Chinese receipt unchanged after switch");
    CHECK(BY_HAND("settings.get", "{\"id\":\"audio.call_ring\"}") && !strcmp(g_rc.say, "来电铃声：清音"), "Chinese enum receipt unchanged");
    CHECK(AI("call.dial", "{\"number\":\"00000000000\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "Chinese call card");
    ink_confirm_view_t card;
    CHECK(ink_confirm_list(&card, 1) == 1 && !strcmp(card.title, "打电话") && !strcmp(card.detail, "打电话给 00000000000"), "Chinese confirmation unchanged");
    CHECK(ink_confirm_decide(card.token, false, INK_SRC_TOUCH) == 0, "cancel Chinese call card");
    CHECK(WAIT_FOR(!ink_ui_overlay_up(), 3000), "Chinese card closes");
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(settled(), 15000), "boot settles"); test_start_home();
    lang(1);
    double missing = sim_metric_value("i18n.missing");
    ink_lunar_t lunar = {2026, 4, 5, false}; char lunar_name[32], ganzhi[12];
    CHECK(ink_lunar_name(&lunar, lunar_name, sizeof lunar_name) > 0 && !strcmp(lunar_name, "Lunar 4/5"), "English lunar date");
    CHECK(!strcmp(ink_lunar_zodiac(2026), "Horse") && ink_lunar_ganzhi(2026, ganzhi, sizeof ganzhi) > 0 && !strcmp(ganzhi, "BingWu"), "English zodiac and sexagenary name");
    confirmations(); notifications(); interruptions(); receipts();
    CHECK(sim_metric_value("i18n.missing") == missing, "all exercised service keys have translations (missing delta %.0f)", sim_metric_value("i18n.missing") - missing);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "i18n-services", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start service language driver");
}
