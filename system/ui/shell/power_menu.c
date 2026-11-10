/* The power menu (placeholder look, D-64): the power key held down. What its buttons do are the capabilities the AI has too. */
#include "ink/sys/i18n.h"
#include <stdio.h>
#include <string.h>

#include "ink/sys/cap.h"
#include "ink/sys/exec.h"
#include "ink/sys/input.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/state.h"
#include "ink/sys/ui_semantics.h"
#include "shell_card.h"
#include "ink/ui_shell.h"

static struct {
    shell_card_t card;
    ink_exec_job_t open_job, close_job;
    uint32_t version;
    lv_obj_t *result;
    char say[160];
} M;

static const ink_ui_action_spec_t ACTIONS[] = {
    {.id = "system.shutdown", .label = TR_NOOP("关机"), .capability = "system.shutdown", .enabled = true},
    {.id = "system.reboot", .label = TR_NOOP("重启"), .capability = "system.reboot", .enabled = true},
    {.id = "shell.lock", .label = TR_NOOP("锁屏"), .capability = "shell.lock", .enabled = true},
    {.id = "system.report_problem", .label = TR_NOOP("记录问题"), .capability = "system.report_problem", .enabled = true},
    {.id = "system.export_logs", .label = TR_NOOP("导出日志"), .capability = "system.export_logs", .enabled = true}
};
static const ink_ui_field_spec_t FIELDS[] = {
    {.id = "cancel", .role = "button", .label = TR_NOOP("取消"), .value = TR_NOOP("取消")}
};
static const ink_ui_page_spec_t MODEL = {.id = "power_menu", .title = TR_NOOP("电源菜单"), .object = {"power_menu", 0},
    .fields = FIELDS, .field_count = sizeof FIELDS / sizeof *FIELDS, .actions = ACTIONS, .action_count = sizeof ACTIONS / sizeof *ACTIONS};
static ink_ui_page_spec_t model(void) {
    ink_ui_page_spec_t page = MODEL; page.title = TR(MODEL.title);
    page.object.revision = ink_ui_generation(); return page;
}
static int semantic_snapshot(char *buf, size_t cap) {
    ink_ui_page_spec_t page = model();
    ink_ui_field_spec_t fields[sizeof FIELDS / sizeof *FIELDS + 1];
    memcpy(fields, FIELDS, sizeof FIELDS);
    fields[0].label = fields[0].value = TR(FIELDS[0].label);
    fields[page.field_count++] = (ink_ui_field_spec_t){.id = "result", .role = "status", .label = TR("记录结果"), .value = M.say};
    ink_ui_action_spec_t actions[sizeof ACTIONS / sizeof *ACTIONS];
    memcpy(actions, ACTIONS, sizeof actions);
    for (size_t i = 0; i < sizeof ACTIONS / sizeof *ACTIONS; i++) actions[i].label = TR(ACTIONS[i].label);
    page.actions = actions; page.fields = fields;
    return ink_ui_semantics_snapshot(&page, buf, cap);
}

static bool on_key(ink_key_t key, bool long_press);
static void pressed(lv_event_t *e) { (void)e; M.version = ink_ui_content_version(); }

static const ink_ui_overlay_t OVERLAY = {.name = "power_menu", .level = 2, .on_key = on_key, .snapshot = semantic_snapshot};

static void close_menu(void) { /* UI task */
    if (!M.card.root) return;
    shell_card_close(&M.card);
    ink_ui_overlay_remove(&OVERLAY);
}

static bool on_key(ink_key_t key, bool long_press) {
    if (!long_press && (key == HAL_KEY_HOME || key == HAL_KEY_BACK)) {
        close_menu();
        return true;
    }
    return false;
}

static void logged(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    if (!M.card.root) return;
    snprintf(M.say, sizeof M.say, "%s", rc->say);
    lv_label_set_text(M.result, M.say);
    ink_ui_content_changed(); M.version = ink_ui_content_version();
}

static void on_button(lv_event_t *e) {
    lv_event_stop_processing(e);
    const char *cap = lv_event_get_user_data(e);
    if (M.version != ink_ui_content_version()) return;
    if (cap && (!strcmp(cap, "system.report_problem") || !strcmp(cap, "system.export_logs"))) {
        ink_call_opts_t opts = {.src = INK_SRC_TOUCH, .page_gen = M.version, .reply_exec = INK_EXEC_UI, .done = logged};
        if (ink_cap_call(cap, NULL, &opts)) { snprintf(M.say, sizeof M.say, TR("暂时无法记录，请稍后重试")); lv_label_set_text(M.result, M.say); }
        return;
    }
    if (cap) {
        ink_ui_page_spec_t page = model();
        ink_ui_action_target_t target = {M.version, page.object};
        ink_receipt_t rc;
        ink_ui_semantics_action(&page, cap, &target, INK_SRC_TOUCH, &rc);
    }
    close_menu();
}

static void power_tile(const char *label, const char *icon, int col, const char *action, bool primary) {
    int cell = (INK_CANVAS_W - 2 * INK_MARGIN) / 3;
    lv_obj_t *b = ink_ui_pill(M.card.panel, "", INK_MARGIN + col * cell, INK_MARGIN, cell,
                              INK_CIRCLE_APP + INK_GAP_M + ink_ui_text_height(label, INK_FONT_LIST, INK_WEIGHT_MEDIUM, cell - 2 * INK_GAP_XS), false, on_button, (void *)action);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_add_event_cb(b, pressed, LV_EVENT_PRESSED, NULL);
    ink_ui_circle(b, icon, (cell - INK_CIRCLE_APP) / 2 - INK_GAP_XS, 0, INK_CIRCLE_APP, INK_ICON_APP,
                  primary ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, NULL, NULL);
    lv_obj_t *l = ink_ui_label(b, label, INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, INK_CIRCLE_APP + INK_GAP_M, cell - 2 * INK_GAP_XS);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
}

static void open_job(void *arg) { /* UI task */
    (void)arg;
    if (M.card.root || ink_interrupt_active()) return;
    shell_card_open(&M.card);
    ink_ui_overlay_add(&OVERLAY, M.card.root);
    int cell = (INK_CANVAS_W - 2 * INK_MARGIN) / 3;
    int text_h = LV_MAX(ink_ui_line_height(INK_FONT_LIST), ink_ui_text_height(TR("锁屏"), INK_FONT_LIST, INK_WEIGHT_MEDIUM, cell));
    int h = 2 * INK_MARGIN + INK_CIRCLE_APP + INK_GAP_M + text_h + INK_MARGIN + 4 * INK_PRIMARY;
    lv_obj_clean(M.card.panel);
    lv_obj_set_y(M.card.panel, ink_ui_px(INK_CANVAS_H - h));
    lv_obj_set_height(M.card.panel, ink_ui_px(h + INK_RADIUS_PANEL));
    power_tile(TR("关机"), "power", 0, "system.shutdown", true);
    power_tile(TR("重启"), "update", 1, "system.reboot", false);
    lv_obj_t *lock = ink_ui_circle(M.card.panel, "lock", INK_MARGIN + 2 * cell + (cell - INK_CIRCLE_APP) / 2, INK_MARGIN,
                                   INK_CIRCLE_APP, INK_ICON_APP, INK_SURFACE_WHITE, on_button, "shell.lock");
    lv_obj_add_event_cb(lock, pressed, LV_EVENT_PRESSED, NULL);
    ink_shell_bind(lock, "shell.lock", NULL);
    lv_obj_t *word = ink_ui_label(M.card.panel, TR("锁屏"), INK_FONT_LIST, INK_WEIGHT_MEDIUM, INK_MARGIN + 2 * cell,
                                  INK_MARGIN + INK_CIRCLE_APP + INK_GAP_M, cell);
    lv_obj_set_style_text_align(word, LV_TEXT_ALIGN_CENTER, 0);
    const char *labels[] = {TR("记录问题"), TR("导出日志")};
    const char *caps[] = {"system.report_problem", "system.export_logs"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = ink_ui_pill(M.card.panel, labels[i], INK_MARGIN, h - (4 - i) * INK_PRIMARY - INK_MARGIN,
                                INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, false, on_button, (void *)caps[i]);
        lv_obj_add_event_cb(b, pressed, LV_EVENT_PRESSED, NULL);
    }
    M.say[0] = 0;
    M.result = ink_ui_label(M.card.panel, "", INK_FONT_BODY, INK_WEIGHT_REGULAR, INK_MARGIN, h - 2 * INK_PRIMARY - INK_MARGIN, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_t *cancel = ink_ui_pill(M.card.panel, TR("取消"), INK_MARGIN, h - INK_PRIMARY - INK_MARGIN,
                 INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, false, on_button, NULL);
    lv_obj_add_event_cb(cancel, pressed, LV_EVENT_PRESSED, NULL);
    M.version = ink_ui_content_version();
}

static void on_long(void) { /* the timer task. A call or an alarm does not answer to it */
    ink_exec_post(INK_EXEC_UI, &M.open_job, open_job, NULL);
}

static void close_job(void *arg) { /* UI task */
    (void)arg;
    close_menu();
}

static void on_power(void *ctx, int h, const char *id) { /* locked: nobody is looking at it */
    (void)ctx;
    (void)id;
    char s[16] = "";
    ink_state_get_str(h, s, sizeof s);
    if (!strcmp(s, "locked")) ink_exec_post(INK_EXEC_UI, &M.close_job, close_job, NULL);
}

int ink_shell_power_menu_init(void) {
    ink_input_set_power_long_hook(on_long);
    return ink_state_watch("power.state", on_power, NULL, INK_EXEC_DIRECT);
}
