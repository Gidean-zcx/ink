/* English shell controls, measured labels and Chinese round trips on the real UI. */
#include <stdlib.h>
#include "../system/ui/shell/member_pages.h"
#include "../system/ui/shell/ai_pages.h"
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
static void frame(void *arg) { CHECK(settled(), "frame presented: %s", picture); sim_display_snapshot(arg); captured = true; }
static void capture(const char *name, const char *expected) {
    picture = name;
    CHECK(WAIT_FOR(settled(), 10000), "settled: %s", name);
    on_ui(inspect);
    CHECK(!crowded, "%s layout: %u issues, %.120s", name, crowded, issue);
    CHECK(!expected || strstr(labels, expected), "%s has expected text: %s", name, expected ? expected : "");
    vtk_host_call(save, NULL);
    captured = false; vtk_event_at(vtk_now() + SIM_MS(1), frame, (void *)name);
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
static lv_obj_t *preview, *status_label, *question_label, *answer_label;
static const ink_ui_overlay_t *error_overlay;
static ink_ai_error_t preview_error;
static const char *preview_status;
static void ai_open(void) { ink_shell_ai_open(&preview, &status_label, &question_label, &answer_label); }
static void ai_status(void) { ink_shell_ai_status(preview_status, INK_AI_ERROR_NONE); }
static void ai_answer(void) { ink_shell_ai_words("Synthetic answer, with no member data."); ink_shell_ai_status("回答完成", INK_AI_ERROR_NONE); }
static void ai_close(void) { ink_shell_ai_closed(); lv_obj_delete(preview); preview = NULL; }
static void error_open(void) {
    preview = ink_ui_panel(lv_layer_sys(), INK_PRIMARY, true);
    error_overlay = ink_ai_pages_status(preview_error, preview);
    CHECK(error_overlay != NULL, "native error presenter selects a page");
    ink_ui_overlay_add(error_overlay, preview);
}
static void error_close(void) { ink_ui_overlay_remove(error_overlay); lv_obj_delete(preview); preview = NULL; }
static void switch_prompt(void) { ink_shell_member_switch_open(); }
static void parent_prompt(void) { ink_shell_member_parent_open(0, "Synthetic parent request"); }
static const ink_setting_def_t synthetic_setting = {.id = "test.i18n.confirm", .title = "Synthetic setting", .type = INK_SET_BOOL, .ask_by_hand = true};
static const char *update_state;
static void update_preview(void) {
    ink_state_set_int(ink_state_find("update.progress"), 62);
    ink_state_set_str(ink_state_find("update.state"), update_state);
}
static bool update_ok;
static void update_result(void) {
    ink_args_t args; ink_args_init(&args); ink_args_bool(&args, "ok", update_ok);
    ink_cap_emit("update.result", &args);
}
static void run(void *arg) {
    (void)arg;
    CHECK(WAIT_FOR(ink_member_space_open() && settled(), 15000), "initial device settles");
    test_start_home();
    CHECK(BY_HAND("settings.set", "{\"id\":\"power.lock_after_s\",\"value\":600}") && g_rc.status == INK_RC_DONE, "keep previews awake");
    CHECK(BY_HAND("settings.set", "{\"id\":\"system.language\",\"value\":1}") && g_rc.status == INK_RC_DONE, "English previews");
    open_page("shell.tasks", "Tasks"); capture("Tasks-en", "Activity");
    open_page("shell.ai_home", "AiHome"); capture("AiHome-en", "Conversations");
    CHECK(BY_HAND("shell.ai_ask", NULL) && WAIT_FOR(at("Keyboard"), 5000), "typed question has native keyboard"); capture("AiAsk-en", "Send");
    CHECK(strstr(context, "Ask AI") && strstr(context, "Question"), "question snapshot uses English labels");
    CHECK(BY_HAND("system.home", NULL), "close question and keyboard");
    on_ui(ai_open); preview_status = "正在想…"; on_ui(ai_status); capture("AiThink-en", "Reading this page");
    preview_status = "正在听…松开 AI 键结束"; on_ui(ai_status); capture("AiListen-en", "Listening");
    preview_status = "正在录音，先停了再问"; on_ui(ai_status); capture("AiRecording-en", "Stop recording");
    on_ui(ai_answer); capture("AiAnswer-en", "From this page"); on_ui(ai_close);
    const ink_ai_error_t errors[] = {INK_AI_ERROR_OFFLINE, INK_AI_ERROR_TIMEOUT, INK_AI_ERROR_BUDGET, INK_AI_ERROR_LOST};
    const char *const names[] = {"AiOffline-en", "AiTimeout-en", "AiBudget-en", "AiLost-en"};
    for (unsigned i = 0; i < sizeof errors / sizeof *errors; i++) {
        preview_error = errors[i]; on_ui(error_open); capture(names[i], "Keep reading"); on_ui(error_close);
    }
    open_page("shell.ai_log", "AiLog"); capture("AiLog-en", "AI Activity");
    open_page("shell.ai_receipts", "AiReceipts"); capture("AiReceipts-en", "AI Results");
    open_page("shell.bluetooth", "SetBluetooth"); capture("SetBluetooth-en", "Bluetooth");
    open_page("backup.open", "Backup"); capture("Backup-en", "Back Up Now");
    open_page("transfer.open", "Transfer"); capture("Transfer-en", "Receive Files");
    open_page("transfer.usb", "TransferUsbBusy"); capture("TransferUsbBusy-en", "USB File Transfer");
    CHECK(BY_HAND("system.home", NULL), "home before member prompts");
    on_ui(switch_prompt); capture("MemberSwitch-en", "Before switching"); CHECK(BY_HAND("shell.pin_cancel", NULL), "close switch preview");
    CHECK(BY_HAND("shell.pin_set", NULL) && WAIT_FOR(at("PinSet"), 5000), "open password setup"); capture("PinSet-en", "4 to 8 digits");
    CHECK(BY_HAND("shell.pin_cancel", NULL), "close password setup");
    on_ui(parent_prompt); capture("PinForgot-en", "No password set"); CHECK(BY_HAND("shell.pin_cancel", NULL), "close parent preview");
    CHECK(!ink_settings_define(&synthetic_setting, 1), "register one synthetic high-risk setting");
    CHECK(AI("settings.set", "{\"id\":\"test.i18n.confirm\",\"value\":1}") && g_rc.status == INK_RC_PENDING_CONFIRMATION && WAIT_FOR(at("confirm"), 5000), "real confirmation presenter");
    capture("ConfirmSetting-en", "Change this setting?");
    unsigned token = 0; CHECK(sscanf(g_rc.object, "confirm:%u", &token) == 1 && !ink_confirm_decide(token, false, INK_SRC_TOUCH), "reject synthetic setting");
    CHECK(WAIT_FOR(!at("confirm") && settled(), 5000), "confirmation closes");
    CHECK(BY_HAND("search.show", "{\"query\":\"ZZZ42\"}") && WAIT_FOR(at("SearchEmpty"), 10000), "empty search"); capture("SearchEmpty-en", "No results found");
    CHECK(BY_HAND("search.show", "{\"query\":\"Wallpaper\"}") && WAIT_FOR(at("Search") && strstr(g_snap, "Wallpaper image ID"), 10000), "translated wallpaper setting is searchable"); capture("WallpaperSearch-en", "Wallpaper image ID");
    open_page("shell.update", "Update"); capture("Update-en", "Software Update");
    const char *const states[] = {"failed", "ready", "installing", "idle"};
    const char *const pages[] = {"UpdateFail", "UpdateReady", "UpdateInstall", "Update"};
    for (unsigned i = 0; i < sizeof states / sizeof *states; i++) {
        update_state = states[i]; on_ui(update_preview); CHECK(WAIT_FOR(at(pages[i]), 5000), "synthetic update state: %s", pages[i]);
        char name[48]; snprintf(name, sizeof name, "%s-state-en", pages[i]); capture(name, NULL);
    }
    update_state = "ready"; on_ui(update_preview); hw(HW_BATTERY, 23, 0); ink_power_battery_policy_start(); CHECK(WAIT_FOR(at("UpdateBlocked"), 5000), "battery-blocked update preview"); capture("UpdateBlocked-en", "Cannot install now"); hw(HW_BATTERY, 80, 0); ink_power_battery_policy_start();
    update_state = "idle"; on_ui(update_preview);
    update_ok = true; on_ui(update_result); CHECK(WAIT_FOR(at("UpdateDone"), 5000), "completed update preview"); capture("UpdateDone-en", "Update complete");
    update_ok = false; on_ui(update_result); CHECK(WAIT_FOR(at("BootRecovery"), 5000), "rollback preview"); capture("BootRecovery-en", "Your data is safe");
    CHECK(BY_HAND("shell.update_close", NULL), "close update previews");
    CHECK(sim_metric_value("i18n.missing") == 0, "remaining shell keys all translated: %.0f", sim_metric_value("i18n.missing"));
    CHECK(BY_HAND("settings.set", "{\"id\":\"system.language\",\"value\":0}"), "Chinese round trip");
    open_page("shell.ai_home", "AiHome"); capture("AiHome-zh", "对话记录");
    open_page("shell.tasks", "Tasks"); capture("Tasks-zh", "正在做的事");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "i18n-remaining", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "start remaining shell test");
}
