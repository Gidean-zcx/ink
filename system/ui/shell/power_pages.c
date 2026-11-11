/* Battery surfaces share the interruption channel and the existing auto-lock setting. */
/* ui-scale-runtime: dimension y Centered text receives theme-derived positions. */
/* ui-scale-runtime: font font Centered text receives only the theme font tokens. */
#include "ink/sys/i18n.h"
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/appreg.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"
#include "shell_card.h"

void ink_shell_interrupt_present(const ink_interrupt_t *top);
static struct {
    shell_card_t card;
    ink_exec_job_t off_job;
    void (*normal_off)(const char *why);
    const char *why;
    int interrupt, mode, percent;
    bool blocked, off, drawn;
    uint32_t commit;
    char title[96], body[256];
} B INK_PSRAM_BSS;

static bool saving(void) {
    int64_t enabled = 0;
    ink_state_get_int(ink_state_find("power.saving"), &enabled);
    return enabled != 0;
}
static int semantic_snapshot(char *buf, size_t cap) {
    bool controls = !B.off && B.mode == 1 && ink_member_space_open() && ink_member_current()->role != INK_ROLE_CHILD;
    char percent[64], control[160];
    snprintf(percent, sizeof percent, "电量只剩 %d%%", B.percent);
    snprintf(control, sizeof control, "省电：1 分钟锁屏=%s；原设锁屏=%lld 秒", saving() ? TR("开") : TR("关"), (long long)ink_setting_get("power.lock_after_s"));
    ink_ui_field_spec_t fields[] = {
        {.id = "status", .role = "status", .label = "状态", .value = B.off ? (!ink_member_space_open() ? "已关机。没电了，充上电再开机" : B.title) : B.mode == 3 ? "正在充电。已锁定，按电源键或 AI 键唤醒。" : B.mode == 2 ? "电量用完了，正在存东西。存好就关机。" : "到 2% 时先保存再关机。"},
        {.id = "body", .role = "text", .label = "说明", .value = B.off ? B.body : percent},
        {.id = "saving", .role = "switch", .label = "省电", .value = control}
    };
    ink_ui_action_spec_t actions[] = {
        {.id = "interrupt.ack", .label = "知道了", .capability = "interrupt.ack", .enabled = !B.off && B.mode == 1},
        {.id = "power.set_saving", .label = "省电：1 分钟锁屏", .capability = "power.set_saving", .args_json = saving() ? "{\"enabled\":false}" : "{\"enabled\":true}", .enabled = controls}
    };
    const char *name = B.off ? "off" : B.mode == 3 ? "Charging" : B.mode == 2 ? "BatterySaving" : "BatteryLow";
    ink_ui_page_spec_t page = {.id = name, .title = B.off ? "PowerOffEmpty" : name, .object = {"battery", ink_ui_generation()},
        .fields = fields, .field_count = controls ? 3 : 2, .actions = actions, .action_count = sizeof actions / sizeof *actions};
    return ink_ui_semantics_snapshot(&page, buf, cap);
}
static const ink_ui_overlay_t LOW = {.name = "BatteryLow", .level = 3, .snapshot = semantic_snapshot};
static const ink_ui_overlay_t SAVING = {.name = "BatterySaving", .level = 3, .snapshot = semantic_snapshot};
static const ink_ui_overlay_t CHARGING = {.name = "Charging", .level = 3, .snapshot = semantic_snapshot};
static const ink_ui_overlay_t OFF = {.name = "off", .level = 5, .snapshot = semantic_snapshot};
static const ink_ui_overlay_t *overlay(void) { return B.off ? &OFF : B.mode == 3 ? &CHARGING : B.mode == 2 ? &SAVING : &LOW; }
static void close_page(void) {
    if (!B.card.root) return;
    ink_ui_overlay_remove(overlay());
    shell_card_close(&B.card);
}
static void center(const char *text, int font, int y, ink_ui_weight_t weight) {
    lv_obj_t *l = ink_ui_label(B.card.panel, text, font, weight, INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
}
static void render(int mode) {
    close_page(); B.mode = mode;
    if (!mode) return;
    shell_card_open(&B.card); shell_card_full(&B.card);
    lv_obj_clean(B.card.panel);
    ink_ui_overlay_add(overlay(), B.card.root);
    lv_obj_remove_flag(ink_shell_statusbar(B.card.panel, true), LV_OBJ_FLAG_CLICKABLE);
    int y = INK_CANVAS_H / 2 - INK_CIRCLE_LARGE;
    if (mode == 1) {
        ink_ui_circle(B.card.panel, "battery", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, y,
                      INK_CIRCLE_LARGE, INK_ICON_STATE, INK_SURFACE_WHITE, NULL, NULL);
        char title[64]; snprintf(title, sizeof title, TR("电量只剩 %d%%"), B.percent);
        center(title, INK_FONT_SUBTITLE, y + INK_CIRCLE_LARGE + INK_MARGIN, INK_WEIGHT_BOLD);
        center(TR("插上电吧。到 2% 时会先存好东西再关机。"), INK_FONT_NOTE,
               y + INK_CIRCLE_LARGE + INK_MARGIN + ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP_M, INK_WEIGHT_REGULAR);
        int bottom = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY;
        if (ink_member_space_open() && ink_member_current()->role != INK_ROLE_CHILD) {
        char preference[64]; snprintf(preference, sizeof preference, TR("原来设为 %lld 秒，关掉省电后照旧"), (long long)ink_setting_get("power.lock_after_s"));
        center(preference, INK_FONT_NOTE, bottom - INK_CONTROL - INK_GAP_L - INK_LINE_NOTE - INK_GAP_M, INK_WEIGHT_REGULAR);
        lv_obj_t *toggle = ink_ui_toggle(B.card.panel, TR("省电：1 分钟锁屏"), saving(),
                            INK_MARGIN, bottom - INK_CONTROL - INK_GAP_L, INK_CANVAS_W - 2 * INK_MARGIN, NULL, NULL);
        ink_shell_bind(toggle, "power.set_saving", saving() ? "{\"enabled\":false}" : "{\"enabled\":true}");
        }
        ink_shell_bind(ink_ui_pill(B.card.panel, TR("知道了"), INK_MARGIN, bottom, INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, NULL, NULL), "interrupt.ack", NULL);
    } else if (mode == 2) {
        for (int i = 0; i < 3; i++)
            ink_ui_box(B.card.panel, INK_CANVAS_W / 2 - INK_GAP_L / 2 + (i - 1) * (INK_GAP_L + INK_GAP_L),
                       INK_CANVAS_H / 2 - INK_GAP_XXL, INK_GAP_L, INK_GAP_L, -1, i < 2 ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, true);
        center(TR("电量用完了，正在存东西"), INK_FONT_SUBTITLE, INK_CANVAS_H / 2 + INK_GAP_XL, INK_WEIGHT_BOLD);
        center(TR("存好就关机。"), INK_FONT_NOTE, INK_CANVAS_H / 2 + INK_GAP_XL + ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP_M, INK_WEIGHT_REGULAR);
    } else {
        int diameter = 2 * (INK_CIRCLE_APP + INK_MARGIN), top = INK_CANVAS_H / 2 - INK_BLOCK - diameter / 2;
        lv_obj_t *arc = lv_arc_create(B.card.panel);
        lv_obj_remove_style_all(arc);
        lv_obj_set_pos(arc, ink_ui_px((INK_CANVAS_W - diameter) / 2), ink_ui_px(top));
        lv_obj_set_size(arc, ink_ui_px(diameter), ink_ui_px(diameter));
        lv_obj_set_style_arc_color(arc, ink_ui_ink(), LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, ink_ui_px(INK_STROKE), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, ink_ui_ink(), LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(arc, ink_ui_px(INK_GAP), LV_PART_INDICATOR);
        lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_arc_set_bg_angles(arc, 0, 360); lv_arc_set_rotation(arc, 270); lv_arc_set_value(arc, B.percent);
        char percent[16]; snprintf(percent, sizeof percent, "%d%%", B.percent);
        center(percent, INK_FONT_NUMBER, top + (diameter - ink_ui_line_height(INK_FONT_NUMBER)) / 2, INK_WEIGHT_REGULAR);
        center(TR("正在充电"), INK_FONT_SUBTITLE, top + diameter + INK_MARGIN, INK_WEIGHT_BOLD);
        center(TR("已锁定，按电源键或 AI 键唤醒"), INK_FONT_NOTE,
               INK_CANVAS_H - INK_MARGIN - ink_ui_line_height(INK_FONT_NOTE), INK_WEIGHT_REGULAR);
    }
}
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (B.off || (!strncmp(id, "member.", 7) && strcmp(id, "member.current") && strcmp(id, "member.authed"))) return;
    int64_t percent = 0, charging = 0;
    ink_state_get_int(ink_state_find("battery.level"), &percent);
    ink_state_get_int(ink_state_find("battery.charging"), &charging);
    int mode = B.blocked ? 0 : B.interrupt ? (percent <= 2 ? 2 : 1) : charging && ink_power_state() == INK_PWR_LOCKED ? 3 : 0;
    if (mode == 3 && B.mode == 3 && percent / 10 == B.percent / 10) return;
    if (mode == B.mode && percent == B.percent && strncmp(id, "member.", 7) && strcmp(id, "power.lock_after_s") && strcmp(id, "power.saving")) return;
    B.percent = (int)percent;
    render(mode);
}
static void present(const ink_interrupt_t *top) {
    B.interrupt = top && top->kind == INK_INT_LOW_BATTERY && top->nactions == 1 && top->actions[0].cap &&
                  !strcmp(top->actions[0].cap, "interrupt.ack") ? top->id : 0;
    B.blocked = top && !B.interrupt;
    ink_shell_interrupt_present(B.interrupt ? NULL : top);
    changed(NULL, 0, "");
}
static void draw_off(void *arg) {
    (void)arg; close_page(); B.off = true;
    snprintf(B.title, sizeof B.title, TR("已关机"));
    snprintf(B.body, sizeof B.body, "%s", !strcmp(B.why, "battery_unsaved") ? TR("没电了，有些内容没保存。充上电再开机") : TR("没电了，东西都存好了。充上电再开机"));
    for (int i = 0; ink_member_space_open() && i < ink_app_count(); i++) {
        const ink_app_def_t *d = ink_app_at(i); ink_card_t card = {0};
        if (!ink_app_visible(d)) continue;
        bool found = false;
        for (int k = 0; d->card && d->card(k, &card) == 1; k++) if (card.bookmark && ink_app_card_visible(&card)) {
            snprintf(B.title, sizeof B.title, "%s", card.text);
            found = true; break;
        }
        if (found) break;
    }
    shell_card_open(&B.card); shell_card_full(&B.card); lv_obj_clean(B.card.panel);
    ink_ui_overlay_add(&OFF, B.card.root);
    lv_obj_t *title = ink_ui_label(B.card.panel, B.title, INK_FONT_HEADING, INK_WEIGHT_BOLD, INK_BLOCK,
                 INK_CANVAS_H / 2 - INK_LINE_HEADING, INK_CANVAS_W - 2 * INK_BLOCK);
    lv_obj_t *body = ink_ui_label(B.card.panel, B.body, INK_FONT_LIST, INK_WEIGHT_REGULAR, INK_BLOCK,
                 INK_CANVAS_H / 2 + INK_GAP_L, INK_CANVAS_W - 2 * INK_BLOCK);
    lv_obj_align_to(body, title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, ink_ui_px(INK_GAP_L));
    ink_ui_notice(B.card.panel, TR("已关机，没电了，充上电再开机"), INK_CANVAS_H - INK_MARGIN - INK_LINE_NOTE);
    lv_refr_now(NULL); B.commit = ink_display_commits();
    __atomic_store_n(&B.drawn, true, __ATOMIC_RELEASE);
}
static void off_picture(const char *why) {
    if (strncmp(why, "battery", 7)) { B.normal_off(why); return; }
    B.why = why;
    uint32_t before = ink_display_commits(), failures = ink_display_failures();
    __atomic_store_n(&B.drawn, false, __ATOMIC_RELEASE);
    if (ink_exec_post(INK_EXEC_UI, &B.off_job, draw_off, NULL)) { ink_power_off_picture_result(false); return; }
    uint64_t end = osal_now_us() + 6000000ull;
    while ((!__atomic_load_n(&B.drawn, __ATOMIC_ACQUIRE) || ink_ui_busy() || ink_display_busy()) && osal_now_us() < end) osal_task_delay_us(50000);
    bool ok = __atomic_load_n(&B.drawn, __ATOMIC_ACQUIRE) && B.commit > before && !ink_ui_busy() && !ink_display_busy() && ink_display_failures() == failures;
    ink_power_off_picture_result(ok);
    ink_probe_count(ok ? "ui.off_picture_on_panel" : "ui.off_picture_late");
}
int ink_power_pages_init(void (*normal_off)(const char *why)) {
    B.normal_off = normal_off;
    static const ink_int_presenter_t presenter = {.show = present, .exec = INK_EXEC_UI};
    ink_interrupt_set_presenter(&presenter); ink_power_set_off_picture(off_picture);
    return ink_state_watch("battery.", changed, NULL, INK_EXEC_UI) || ink_state_watch("power.", changed, NULL, INK_EXEC_UI) ||
           ink_state_watch("member.", changed, NULL, INK_EXEC_UI) ||
           ink_settings_watch("power.lock_after_s", changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
