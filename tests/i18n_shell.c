/* English shell controls, measured labels and Chinese round trips on the real UI. */
#include <stdlib.h>
#include "ink/sys/i18n.h"
#include "ink/sys/file_picker.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/textfield.h"
#include "ink/ui_shell.h"
#include "../system/ui/shell/pinpad.h"
#define ink_main product_main
#include "configured_boot.c"
#undef ink_main
#include "integration/whole_product.h"

static const char *picture;
static volatile bool captured;
static char labels[8192], context[8192];
static lv_area_t boxes[128];
static unsigned nboxes, crowded;
static char issue[160];
static bool settled(void) { return !ink_ui_busy() && !ink_display_busy(); }
static bool at(const char *name) { on_ui(read_screen); return !strcmp(g_top, name); }
static void walk(lv_obj_t *o) {
    if (!lv_obj_is_visible(o) || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(o, &lv_label_class) && lv_label_get_text(o)[0]) {
        const char *t = lv_label_get_text(o);
        size_t n = strlen(labels); snprintf(labels + n, sizeof labels - n, "%s|", t);
        lv_area_t a; lv_obj_get_content_coords(o, &a);
        const lv_font_t *f = lv_obj_get_style_text_font(o, LV_PART_MAIN);
        int letter = lv_obj_get_style_text_letter_space(o, LV_PART_MAIN), line = lv_obj_get_style_text_line_space(o, LV_PART_MAIN);
        lv_point_t one, wrap;
        lv_text_get_size(&one, t, f, letter, line, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_text_get_size(&wrap, t, f, letter, line, lv_area_get_width(&a), LV_TEXT_FLAG_NONE);
        bool single = lv_label_get_long_mode(o) != LV_LABEL_LONG_MODE_WRAP;
        size_t len = strlen(t);
        bool dots = lv_label_get_long_mode(o) == LV_LABEL_LONG_MODE_DOTS && len >= 3 && !strcmp(t + len - 3, "...");
        if (dots || (single ? one.x > lv_area_get_width(&a) + 1 : wrap.y > lv_area_get_height(&a) + 1)) {
            crowded++; snprintf(issue, sizeof issue, "cut: %.120s", t);
        }
        int w = LV_MIN(single ? one.x : wrap.x, lv_area_get_width(&a));
        int h = single ? one.y : wrap.y;
        lv_text_align_t align = lv_obj_get_style_text_align(o, LV_PART_MAIN);
        a.x1 += align == LV_TEXT_ALIGN_RIGHT ? lv_area_get_width(&a) - w : align == LV_TEXT_ALIGN_CENTER ? (lv_area_get_width(&a) - w) / 2 : 0;
        a.x2 = a.x1 + w - 1; a.y2 = a.y1 + h - 1;
        for (unsigned i = 0; i < nboxes; i++) if (a.x1 <= boxes[i].x2 && boxes[i].x1 <= a.x2 && a.y1 <= boxes[i].y2 && boxes[i].y1 <= a.y2) {
            crowded++; snprintf(issue, sizeof issue, "overlap: %.120s", t);
        }
        if (a.x1 < 0 || a.y1 < 0 || a.x2 >= INK_CANVAS_W || a.y2 >= INK_CANVAS_H) {
            crowded++; snprintf(issue, sizeof issue, "off screen: %.120s", t);
        }
        CHECK(nboxes < sizeof boxes / sizeof *boxes, "label evidence capacity");
        if (nboxes < sizeof boxes / sizeof *boxes) boxes[nboxes++] = a;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) walk(lv_obj_get_child(o, (int32_t)i));
}
static void inspect(void) {
    labels[0] = issue[0] = 0; nboxes = crowded = 0;
    lv_obj_update_layout(lv_screen_active()); lv_obj_update_layout(lv_layer_sys());
    lv_obj_t *surface = lv_screen_active();
    if (strcmp(ink_ui_top_name(), ink_ui_page_name())) {
        for (int32_t i = (int32_t)lv_obj_get_child_count(lv_layer_sys()) - 1; i >= 0; i--) {
            lv_obj_t *o = lv_obj_get_child(lv_layer_sys(), i);
            if (lv_obj_is_visible(o)) { surface = o; break; }
        }
    }
    walk(surface);
    CHECK(ink_ui_snapshot(context, sizeof context, NULL) > 0, "snapshot available for %s", picture);
}
static void save(void *arg) {
    (void)arg;
    char path[768]; int n = snprintf(path, sizeof path, "%s/%s.json", g_sim.out_dir, picture);
    if (n < 0 || (size_t)n >= sizeof path) abort();
    FILE *file = fopen(path, "wb");
    if (!file || fputs(context, file) == EOF || fclose(file)) abort();
}
/* A refresh that starts right after the scene settled (a clock tick, a late status) is waited out, not taken
 * for an unpresented frame; still a failure if the scene does not settle within 1.5 s (as boot_flow.c). */
static int frame_tries;
static void frame(void *arg) {
    if (!settled() && ++frame_tries < 150) { vtk_event_at(vtk_now() + SIM_MS(10), frame, arg); return; }
    CHECK(settled(), "frame presented: %s", picture); sim_display_snapshot(arg); captured = true;
}
static void capture(const char *name, const char *expected) {
    picture = name;
    CHECK(WAIT_FOR(settled(), 10000), "settled: %s", name);
    on_ui(inspect);
    CHECK(!crowded, "%s layout: %u issues, %.120s", name, crowded, issue);
    CHECK(!expected || strstr(labels, expected), "%s has expected text: %s", name, expected ? expected : "");
    vtk_host_call(save, NULL);
    captured = false; frame_tries = 0; vtk_event_at(vtk_now() + SIM_MS(1), frame, (void *)name);
    CHECK(WAIT_FOR(captured, 2000), "captured: %s", name);
    ink_probe_count("test.frames");
}
static void open_page(const char *cap, const char *name) {
    CHECK(BY_HAND("system.home", NULL) && g_rc.status == INK_RC_DONE, "return home");
    CHECK(BY_HAND(cap, NULL) && g_rc.status == INK_RC_DONE && WAIT_FOR(at(name), 10000), "open %s", name);
}
static void settings(const char *cap, const char *name, const char *title) {
    open_page(cap, name);
    for (unsigned page = 1; page <= 16; page++) {
        char shot[64]; snprintf(shot, sizeof shot, "%s-en-%u", name, page);
        capture(shot, title);
        on_ui(read_screen);
        if (!strstr(g_snap, "shell.settings_next")) break;
        CHECK(page < 16, "bounded settings pagination");
        CHECK(BY_HAND("shell.settings_next", NULL) && g_rc.status == INK_RC_DONE, "next settings page");
    }
}
static const char *click_text;
static bool clicked;
static void click_label(lv_obj_t *o) {
    if (!lv_obj_is_visible(o)) return;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), click_text)) {
        lv_obj_t *button = lv_obj_get_parent(o); clicked = true;
        lv_obj_send_event(button, LV_EVENT_PRESSED, NULL);
        lv_obj_send_event(button, LV_EVENT_CLICKED, NULL); return;
    }
    for (uint32_t i = 0; !clicked && i < lv_obj_get_child_count(o); i++) click_label(lv_obj_get_child(o, (int32_t)i));
}
static void activate_label(void) { clicked = false; click_label(lv_layer_sys()); CHECK(clicked, "activate visible control: %s", click_text); }
static void dismiss(const char *text) { click_text = text; on_ui(activate_label); CHECK(WAIT_FOR(!at("Refused") && !at("AirplaneOn") && settled(), 10000), "dismissed card closes and settles"); }
static void erased(void) { ink_factory_done_open(); }
static void members(void) { ink_shell_members_open(true); }
static int control_snapshot(char *out, size_t cap) {
    ink_ui_page_spec_t model = {.id = ink_ui_page_name(), .title = "Synthetic controls"};
    return ink_ui_semantics_snapshot(&model, out, cap);
}
static void pin_create(lv_obj_t *screen, void *arg) { (void)arg; shell_pinpad(screen, NULL); }
static const ink_activity_t PAD = {.name = "test.pad", .create = pin_create, .snapshot = control_snapshot};
static void pin(void) { ink_ui_push(&PAD, NULL); }
static void picker_done(void *ctx, uint32_t id) { (void)ctx; (void)id; }
static ink_fs_type_t picker_type;
static void picker(void) { CHECK(!ink_file_picker_open(picker_type, picker_done, NULL), "open picker"); }
static void refused(void) {
    ink_receipt_t rc = {.status = INK_RC_REJECTED};
    snprintf(rc.detail, sizeof rc.detail, "children cannot run system actions");
    CHECK(ink_shell_refused(&rc), "show local refusal");
}
static void plane(void) {
    ink_receipt_t rc = {.status = INK_RC_REJECTED};
    snprintf(rc.detail, sizeof rc.detail, "airplane mode is on");
    snprintf(rc.say, sizeof rc.say, "Synthetic service message");
    CHECK(ink_shell_airplane_card(&rc), "show airplane card");
}
static void child_limit(void) { ink_shell_kid_limit("reader"); }
static lv_obj_t *editor;
static void edit_create(lv_obj_t *screen, void *arg) {
    (void)arg;
    ink_text_cfg_t cfg = {.type = INK_TEXT_PLAIN, .enter = INK_ENTER_DONE, .max = 256, .initial = "Synthetic text"};
    editor = ink_textfield_create(screen, &cfg, NULL, NULL, false);
    lv_obj_set_pos(editor, ink_ui_px(INK_MARGIN), ink_ui_px(INK_STATUS_BAR + INK_PRIMARY));
    lv_obj_set_size(editor, ink_ui_px(INK_CANVAS_W - 2 * INK_MARGIN), ink_ui_px(INK_PRIMARY));
    static ink_ui_field_spec_t spec = {.id = "text", .role = "textbox", .label = "Synthetic text"};
    static ink_ui_action_spec_t action = {.id = "focus", .label = "Focus", .capability = "search.focus", .enabled = true};
    static const ink_ui_page_spec_t model = {.id = "test.editor", .title = "Synthetic editor", .fields = &spec, .field_count = 1, .actions = &action, .action_count = 1};
    CHECK(!ink_textfield_bind_short(editor, &model, &spec, &action), "bind synthetic editor semantics");
}
static const ink_activity_t EDITOR = {.name = "test.editor", .create = edit_create, .snapshot = control_snapshot};
static void edit(void) { ink_ui_push(&EDITOR, NULL); }
static void focus_editor(void) { CHECK(!ink_textfield_short_focus(editor), "focus native keyboard"); }
static void edit_menu(void) { lv_obj_send_event(editor, LV_EVENT_LONG_PRESSED, NULL); }
static void low_battery(void) {
    hw(HW_BATTERY, 5, 0); ink_power_battery_policy_start();
    CHECK(WAIT_FOR(at("BatteryLow"), 10000), "low battery page");
}
extern void (*sim_power_before_shutdown)(void);
static void shutdown_frame(void) { capture("off-en", "Powered off"); ink_probe_count("test.finished"); }
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open() && settled(), 15000), "initial device settles");
    test_start_home();
    CHECK(BY_HAND("settings.set", "{\"id\":\"power.lock_after_s\",\"value\":600}") && g_rc.status == INK_RC_DONE, "keep the device awake");
    CHECK(BY_HAND("settings.set", "{\"id\":\"system.language\",\"value\":1}") && g_rc.status == INK_RC_DONE, "switch to English");
    double before = sim_metric_value("i18n.missing");
    settings("shell.voice", "SetVoice", "Voice & AI");
    settings("shell.storage", "SetStorage", "Storage & Backup");
    settings("shell.about", "SetAbout", "About & Updates");
    settings("shell.privacy", "SetPrivacy", "Privacy");
    settings("shell.device", "DeviceDetails", "Device Details");
    settings("shell.licenses", "Licenses", "Licenses");
    CHECK(BY_HAND("shell.license", "{\"index\":0}") && g_rc.status == INK_RC_DONE && WAIT_FOR(at("LicenseText"), 5000), "license text page");
    capture("LicenseText-en", "License text");
    open_page("factory.open", "FactoryReset"); capture("FactoryReset-en", "Erase this device");
    on_ui(erased); CHECK(WAIT_FOR(at("FactoryDone"), 5000), "synthetic reset completion"); capture("FactoryDone-en", "This device is erased");
    CHECK(BY_HAND("system.home", NULL), "home after reset preview");
    for (picker_type = INK_FS_ANY; picker_type <= INK_FS_TEXT; picker_type++) {
        char shot[40]; snprintf(shot, sizeof shot, "FilePicker-en-%d", picker_type);
        on_ui(picker); CHECK(WAIT_FOR(at("FilePicker"), 5000), "picker visible");
        CHECK(WAIT_FOR(at("FilePicker") && strstr(g_snap, "\"status\":\"\""), 10000), "picker loaded");
        capture(shot, "Choose"); CHECK(BY_HAND("file_picker.cancel", NULL), "close picker");
    }
    on_ui(members); CHECK(WAIT_FOR(at("member_gate"), 5000), "member gate"); capture("member_gate-en", "Who's using this?"); key(HAL_KEY_HOME);
    on_ui(pin); capture("pinpad-en", "Password");
    CHECK(BY_HAND("system.home", NULL), "close standalone pad");
    on_ui(plane); capture("AirplaneOn-en", "Airplane mode is on"); dismiss("Cancel");
    on_ui(refused); capture("Refused-en", "Action not taken"); dismiss("OK");
    on_ui(edit); CHECK(WAIT_FOR(at("test.editor"), 5000), "editor page committed"); on_ui(focus_editor); CHECK(WAIT_FOR(at("Keyboard"), 5000), "keyboard visible"); capture("keyboard-en", "Space");
    on_ui(edit_menu); capture("textfield-menu-en", "Paste");
    CHECK(BY_HAND("system.home", NULL), "close editor");
    hw(HW_KEY_DOWN, HAL_KEY_POWER, 0); delay_ms(1700); hw(HW_KEY_UP, HAL_KEY_POWER, 0);
    CHECK(WAIT_FOR(at("power_menu"), 5000), "power menu"); capture("power_menu-en", "Power off"); dismiss("Cancel");
    CHECK(WAIT_FOR(!at("power_menu") && settled(), 5000), "power menu closed before switching member");
    CHECK(ink_member_switch("kid") == 0, "synthetic child space");
    CHECK(WAIT_FOR(ink_member_space_open() && ink_power_state() != INK_PWR_LOCKED && settled(), 10000), "child space settles");
    on_ui(child_limit); capture("KidBlocked-en", "Ask a parent"); on_ui(ink_shell_kid_close);
    CHECK(ink_member_switch("owner") == 0, "return to synthetic owner");
    CHECK(WAIT_FOR(settled(), 10000), "owner settles");
    low_battery(); capture("BatteryLow-en", "battery left");
    hw(HW_BATTERY_CHARGING, 62, 0); ink_power_battery_policy_start();
    CHECK(BY_HAND("shell.lock", NULL), "lock device");
    CHECK(WAIT_FOR(at("Charging"), 10000), "charging page"); capture("Charging-en", "Charging");
    hw(HW_BATTERY, 62, 0); ink_power_battery_policy_start(); key(HAL_KEY_POWER);
    CHECK(WAIT_FOR(ink_power_state() == INK_PWR_ACTIVE && settled(), 10000), "wake device");
    CHECK(BY_HAND("settings.set", "{\"id\":\"display.lock_style\",\"value\":1}") && g_rc.status == INK_RC_DONE, "clock lock style");
    CHECK(BY_HAND("shell.lock", NULL) && WAIT_FOR(at("LockPage"), 5000), "lock clock page"); capture("LockPage-en", "Wake to view"); key(HAL_KEY_POWER);
    CHECK(WAIT_FOR(ink_power_state() == INK_PWR_ACTIVE && settled(), 10000), "wake after lock screenshot");
    CHECK(sim_metric_value("i18n.missing") == before, "migrated paths have zero missing keys (%.0f new)", sim_metric_value("i18n.missing") - before);
    CHECK(BY_HAND("settings.set", "{\"id\":\"system.language\",\"value\":0}") && g_rc.status == INK_RC_DONE, "switch back to Chinese");
    open_page("shell.privacy", "SetPrivacy"); capture("SetPrivacy-zh", "隐私");
    open_page("factory.open", "FactoryReset"); capture("FactoryReset-zh", "清空这台设备");
    CHECK(BY_HAND("settings.set", "{\"id\":\"system.language\",\"value\":1}") && g_rc.status == INK_RC_DONE, "English shutdown frame");
    CHECK(BY_HAND("system.home", NULL), "home before shutdown");
    sim_power_before_shutdown = shutdown_frame;
    CHECK(BY_HAND("system.shutdown", NULL), "normal synthetic shutdown");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "i18n-shell-test", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start shell test");
}
