/* Settings presenters share the existing F1/F4 bindings and policy gate. */
/* ui-scale-runtime: dimension h,w,y,head,left,title_w,room,body,cell_h Theme sizes plus measured text, bounded by the page body. */
/* ui-scale-runtime: dimension S.y,heading,row_height,row_body Measured section heights and cumulative positions using theme tokens. */
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/hal_device.h"
#include "ink/hal_afe.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"
#include "settings_registry.h"

enum { VOICE, STORAGE, ABOUT, PRIVACY, DEVICE, LICENSES };
typedef struct { const char *id, *title, *value, *setting, *cap, *icon, *section, *note; } row_t;
#include "ink/licenses.h"
/* The rows of these pages are the registry's (settings_registry.c), in its order; the licences are generated. */
static const struct { const char *name, *title, *cap; } PAGES[] = {
    {"SetVoice", TR_NOOP("语音和 AI"), "shell.voice"}, {"SetStorage", TR_NOOP("存储和备份"), "shell.storage"},
    {"SetAbout", TR_NOOP("关于和更新"), "shell.about"}, {"SetPrivacy", TR_NOOP("隐私"), "shell.privacy"},
    {"DeviceDetails", TR_NOOP("设备详情"), "shell.device"}, {"Licenses", TR_NOOP("开源许可"), "shell.licenses"}};
/* Every page's rows fit the per-row buffers below: a ninth About row once wrote past them. */
#define ROWS_MAX 16
_Static_assert(sizeof LICENSE_ROWS / sizeof *LICENSE_ROWS <= ROWS_MAX, "Licenses exceeds ROWS_MAX");
static struct {
    int page;
    unsigned capacity;
    int y;
    int64_t total;
    uint32_t generation;
    char member[16], values[ROWS_MAX][384], args[ROWS_MAX][128], backup[96];
    lv_obj_t *body;
    ink_ui_list_t *list;
    ink_ui_list_position_t position;
    row_t rows[ROWS_MAX];
    const ink_sr_item_t *items[ROWS_MAX];
    ink_sr_state_t state[ROWS_MAX];
    size_t count;
} S INK_PSRAM_BSS;
static void rows_of(int page) { /* a registry row past ROWS_MAX is left out, and the registry test fails */
    S.count = 0;
    for (int i = 0; page == LICENSES ? i < (int)(sizeof LICENSE_ROWS / sizeof *LICENSE_ROWS) : ink_sr_at(i) != NULL; i++) {
        const ink_sr_item_t *it = page == LICENSES ? NULL : ink_sr_at(i);
        if (S.count == ROWS_MAX || (it && (!(it->where & INK_SR_LIST) || strcmp(it->page, PAGES[page].name)))) continue;
        S.items[S.count] = it;
        S.rows[S.count++] = it ? (row_t){it->id, it->title, it->text, it->setting, it->cap, it->icon, it->section, it->note} : LICENSE_ROWS[i];
    }
}
static void storage_value(size_t i, char *out, size_t cap);
static void storage_size(int64_t n, char *out, size_t cap);
static void storage_action(size_t i, ink_ui_action_spec_t *action);
static void storage_request(void *ctx, const char *id, const ink_args_t *args);
static bool current(void) {
    return S.list && ink_ui_page_current() && !strcmp(ink_ui_page_name(), PAGES[S.page].name) &&
        ink_member_space_open() && !strcmp(S.member, ink_member_current()->id);
}
/* What a row says is the registry's (ink_sr_value); this page adds only what it fetched itself and a tap's next value. */
static void value(size_t i) {
    const row_t *r = &S.rows[i];
    char *out = S.values[i]; size_t cap = sizeof S.values[i];
    S.state[i] = INK_SR_AVAILABLE;
    if (!S.items[i]) { snprintf(out, cap, "%s", r->value); return; } /* a licence's version and terms */
    S.state[i] = ink_sr_value(S.items[i], out, cap);
    int64_t v = ink_setting_get(r->setting ? r->setting : r->id);
    if (!strcmp(r->id, "voice.name")) {
        int n = 0;
        for (const char *const *names = hal_afe_names(); names[n];) n++;
        snprintf(S.args[i], sizeof S.args[i], "{\"id\":\"voice.name\",\"value\":%lld}", (long long)(n ? (v + 1) % n : 0));
    } else if (r->setting)
        snprintf(S.args[i], sizeof S.args[i], "{\"id\":\"%s\",\"value\":%lld}", r->setting,
            (long long)(!strcmp(r->setting, "voice.handsfree") ? (v + 1) % 3 : !v));
    else if (!strcmp(r->id, "system.language")) /* a tap moves to the next language */
        snprintf(S.args[i], sizeof S.args[i], "{\"id\":\"%s\",\"value\":%d}", r->id, ink_lang_name((int)v + 1) ? (int)v + 1 : 0);
    else if (!strcmp(r->id, "backup")) {
        char automatic[96]; snprintf(automatic, sizeof automatic, "%s", out);
        snprintf(out, cap, "%s · %s", S.backup, automatic);
    } else if (S.page == STORAGE && i > 1) storage_value(i, out, cap);
    else if (!strcmp(r->id, "storage.total")) {
        if (S.total >= 0) storage_size(S.total, out, cap); else snprintf(out, cap, "%s", TR("暂时读不出来"));
    }
}
static bool paragraph(size_t i) { return S.page == PRIVACY && i < 4; }
static bool footer(size_t i) { return !strcmp(S.rows[i].id, "reset") || !strcmp(S.rows[i].id, "privacy.records"); }
static int heading(size_t i) {
    const char *text = S.rows[i].section;
    return !text ? 0 : !text[0] ? INK_GAP_L : INK_GAP_L + ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP_M;
}
static int row_body(size_t i) {
    const row_t *r = &S.rows[i];
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    if (S.page == LICENSES) return ink_ui_text_height(S.rows[i].title, INK_FONT_LIST, INK_WEIGHT_MEDIUM, w - 2 * INK_GAP_XL) + INK_GAP_XS + ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_L;
    if (S.page == DEVICE) return ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS +
        ink_ui_text_height(S.values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, w) + INK_GAP_L;
    if (paragraph(i)) return ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP_XS +
        ink_ui_text_height(S.values[i], INK_FONT_BODY, INK_WEIGHT_REGULAR, w) + INK_GAP_L;
    if (S.page == VOICE && !i) return ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS + INK_SEGMENT;
    if (S.page == ABOUT && !i) return INK_CIRCLE_APP + INK_GAP + ink_ui_line_height(INK_FONT_SUBTITLE);
    if (S.page == ABOUT && i == 1) return ink_ui_line_height(INK_FONT_NOTE) + INK_GAP_L;
    if (footer(i)) return INK_CONTROL + INK_GAP_XS + ink_ui_text_height(S.values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, w);
    if (r->setting) return r->icon ? LV_MAX(INK_CONTROL, ink_ui_text_height(TR(r->title), INK_FONT_LIST, INK_WEIGHT_MEDIUM,
        w - INK_CIRCLE_SMALL - INK_GAP_XL - INK_PRIMARY - INK_GAP) + INK_GAP_S) : INK_CONTROL;
    return !r->icon || ink_ui_text_width(TR(r->title), INK_FONT_LIST, INK_WEIGHT_MEDIUM) +
        ink_ui_text_width(S.values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR) + INK_GAP > w - INK_CIRCLE_SMALL - INK_GAP_XL - INK_GAP
        ? LV_MAX(ink_ui_row_double(), ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS + 2 * INK_GAP +
            ink_ui_text_height(S.values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, w - (r->icon ? INK_CIRCLE_SMALL + INK_GAP_XL + INK_GAP : INK_GAP_XL))) : INK_CONTROL;
}
static int row_height(size_t i) {
    const char *note = S.state[i] == INK_SR_UNSUPPORTED ? NULL : S.rows[i].note;
    return heading(i) + row_body(i) + (note ? INK_GAP_XS +
        ink_ui_text_height(TR(note), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_CANVAS_W - 2 * INK_MARGIN) : 0);
}
static void bind_row(lv_obj_t *slot, size_t i, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx;
    const row_t *r = &S.rows[i];
    value(i);
    const char *caption = S.page == LICENSES ? r->title : TR(r->title);
    *field = (ink_ui_field_spec_t){.id = r->id, .role = r->setting ? "setting" : "text", .label = caption,
        .value = S.values[i], .object = {r->id, 0}};
    *action = (ink_ui_action_spec_t){.id = r->id, .label = caption, .capability = r->setting ? "settings.set" : r->cap,
        .args_json = r->setting || !strcmp(r->id, "system.language") || !strcmp(r->id, "voice.name") ? S.args[i] : NULL, .object = field->object, .enabled = r->setting || r->cap};
    bool unavailable = S.state[i] == INK_SR_UNSUPPORTED || S.state[i] == INK_SR_DEPENDENT; /* grayed, the reason in its place */
    if (unavailable) { action->enabled = false; field->value = S.values[i]; }
    if (S.items[i] && S.items[i]->args) action->args_json = S.items[i]->args;
    if (S.page == LICENSES) { snprintf(S.args[i], sizeof S.args[i], "{\"index\":%zu}", i); action->args_json = S.args[i]; }
    if (S.page == STORAGE) storage_action(i, action);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, head = heading(i), body = row_body(i), h = row_height(i);
    bool join_next = r->icon && (i + 1) % S.capacity && i + 1 < S.count &&
        S.rows[i + 1].icon && !S.rows[i + 1].section && !r->note;
    if (!(i % S.capacity)) S.y = 0;
    if (footer(i)) S.y = INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_CONTROL - 2 * INK_GAP - h;
    lv_obj_set_pos(slot, ink_ui_px(INK_MARGIN), ink_ui_px(S.y)); S.y += h + (join_next ? 0 : INK_STROKE);
    lv_obj_set_height(slot, ink_ui_px(h));
    lv_obj_set_style_bg_opa(slot, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(slot, 0, 0);
    lv_obj_set_style_radius(slot, 0, 0);
    lv_obj_set_style_clip_corner(slot, false, 0);
    lv_obj_remove_state(slot, LV_STATE_DISABLED);
    if (!lv_obj_get_child_count(slot)) {
        ink_ui_box(slot, 0, 0, w, body, INK_RADIUS_CARD, INK_SURFACE_WHITE, false);
        ink_ui_label(slot, "", INK_FONT_LIST, INK_WEIGHT_MEDIUM, 0, 0, w);
        ink_ui_label(slot, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, 0, w);
    }
    lv_obj_t *bg = lv_obj_get_child(slot, 0), *title = lv_obj_get_child(slot, 1), *text = lv_obj_get_child(slot, 2);
    lv_obj_remove_flag(title, LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(text, LV_OBJ_FLAG_HIDDEN);
    while (lv_obj_get_child_count(slot) > 3) lv_obj_delete(lv_obj_get_child(slot, 3));
    lv_obj_clean(bg); lv_obj_set_pos(bg, 0, ink_ui_px(head)); lv_obj_set_height(bg, ink_ui_px(body));
    lv_obj_set_style_clip_corner(bg, false, 0);
    ink_ui_surface_style(bg, r->icon ? INK_SURFACE_STATIC : INK_SURFACE_WHITE,
        !r->icon && (S.page == STORAGE || (footer(i) && S.page == PRIVACY)));
    lv_obj_set_style_radius(bg, footer(i) ? LV_RADIUS_CIRCLE : ink_ui_px(INK_RADIUS_CARD), 0);
    bool join_previous = r->icon && i % S.capacity && S.rows[i - 1].icon && !r->section && !S.rows[i - 1].note;
    if (r->icon) {
        lv_obj_set_style_border_post(bg, false, 0);
        if (join_previous)
            ink_ui_group_divider(bg, 0, w);
        ink_ui_circle(slot, r->icon, INK_GAP, head + (body - INK_CIRCLE_SMALL) / 2,
            INK_CIRCLE_SMALL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
    }
    int left = r->icon ? INK_CIRCLE_SMALL + INK_GAP_XL : paragraph(i) || S.page == VOICE || S.page == ABOUT ? 0 : INK_GAP_XL;
    int y = head + (body - ink_ui_line_height(INK_FONT_LIST) - INK_GAP_XS -
        ink_ui_text_height(S.values[i], INK_FONT_NOTE, INK_WEIGHT_REGULAR, w - left - (r->icon ? INK_GAP : 0))) / 2;
    if (S.page == DEVICE || S.page == LICENSES) y = head;
    lv_label_set_text(title, caption); lv_label_set_text(text, S.values[i]);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_LEFT, 0); lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(title, ink_ui_font_px(paragraph(i) ? INK_FONT_SUBTITLE : INK_FONT_LIST, INK_WEIGHT_MEDIUM), 0);
    lv_obj_set_style_text_font(text, ink_ui_font_px(paragraph(i) ? INK_FONT_BODY : INK_FONT_NOTE, INK_WEIGHT_REGULAR), 0);
    lv_obj_set_pos(title, ink_ui_px(left), ink_ui_px(paragraph(i) ? head : y)); lv_obj_set_width(title, ink_ui_px(w - left - (r->icon ? INK_GAP : 0)));
    lv_obj_set_pos(text, ink_ui_px(left), ink_ui_px(paragraph(i) ? head + ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP_XS : y + ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS));
    lv_obj_set_width(text, ink_ui_px(w - left - (r->icon ? INK_GAP : 0)));
    if (S.page == LICENSES) lv_obj_set_y(text, ink_ui_px(y + ink_ui_text_height(caption, INK_FONT_LIST, INK_WEIGHT_MEDIUM, w - 2 * INK_GAP_XL) + INK_GAP_XS));
    if (r->icon && body == INK_CONTROL) {
        int title_w = ink_ui_text_width(caption, INK_FONT_LIST, INK_WEIGHT_MEDIUM);
        lv_obj_set_width(title, ink_ui_px(title_w));
        lv_obj_set_y(title, ink_ui_px(head + (body - ink_ui_line_height(INK_FONT_LIST)) / 2));
        lv_obj_set_pos(text, ink_ui_px(left + title_w + INK_GAP), ink_ui_px(head + (body - ink_ui_line_height(INK_FONT_NOTE)) / 2));
        lv_obj_set_width(text, ink_ui_px(w - left - title_w - 2 * INK_GAP));
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_RIGHT, 0);
    }
    if (r->section && r->section[0]) ink_ui_label(slot, TR(r->section), INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, 0, INK_GAP_L, w);
    if (r->note && S.state[i] != INK_SR_UNSUPPORTED) ink_ui_label(slot, TR(r->note), INK_FONT_NOTE, INK_WEIGHT_REGULAR, 0, head + body + INK_GAP_XS, w);
    if (unavailable) {
        lv_label_set_text(text, S.values[i]);
        lv_obj_set_y(title, ink_ui_px(head));
        lv_obj_set_y(text, ink_ui_px(head + ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS));
        lv_obj_add_state(slot, LV_STATE_DISABLED);
    } else if (r->setting && strcmp(r->setting, "voice.handsfree")) {
        if (!r->icon) lv_obj_add_flag(title, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(text, LV_OBJ_FLAG_HIDDEN);
        ink_ui_toggle(slot, r->icon ? "" : caption, ink_setting_get(r->setting) != 0, left, head + (body - INK_CONTROL) / 2, w - left, NULL, NULL);
        if (r->icon) {
            lv_obj_set_width(title, ink_ui_px(w - left - INK_PRIMARY - INK_GAP));
            y = head + (body - ink_ui_text_height(lv_label_get_text(title), INK_FONT_LIST, INK_WEIGHT_MEDIUM, w - left - INK_PRIMARY - INK_GAP)) / 2;
            lv_obj_set_y(title, ink_ui_px(y));
        }
    } else if (r->setting) {
        const char *const choices[] = {TR("使用时"), TR("一直"), TR("关")};
        lv_obj_set_y(title, 0); lv_obj_add_flag(text, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *segments = ink_ui_segments(slot, choices, 3, (int)ink_setting_get(r->setting), 0,
            ink_ui_line_height(INK_FONT_LIST) + INK_GAP_XS, w, NULL, NULL);
        for (int k = 0; k < 3; k++) {
            char args[96]; snprintf(args, sizeof args, "{\"id\":\"voice.handsfree\",\"value\":%d}", k);
            ink_shell_bind(lv_obj_get_child(segments, k), "settings.set", args);
        }
    } else if (S.page == ABOUT && i < 2) {
        lv_obj_add_flag(title, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(text, 0, ink_ui_px(i ? 0 : INK_CIRCLE_APP + INK_GAP)); lv_obj_set_width(text, ink_ui_px(w));
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
        if (!i) {
            lv_obj_set_style_text_font(text, ink_ui_font_px(INK_FONT_SUBTITLE, INK_WEIGHT_BOLD), 0);
            ink_ui_circle(slot, "ai", (w - INK_CIRCLE_APP) / 2, 0, INK_CIRCLE_APP, INK_ICON_APP, INK_SURFACE_BLACK, NULL, NULL);
        }
    } else if (S.page == STORAGE && i == 1) {
        lv_obj_add_flag(title, LV_OBJ_FLAG_HIDDEN); lv_obj_set_y(text, ink_ui_px(head + (body - ink_ui_line_height(INK_FONT_LIST)) / 2));
        lv_obj_set_style_text_font(text, ink_ui_font_px(INK_FONT_LIST, INK_WEIGHT_REGULAR), 0);
    } else if (footer(i)) {
        lv_obj_set_pos(title, 0, ink_ui_px((INK_CONTROL - ink_ui_line_height(INK_FONT_LIST)) / 2)); lv_obj_set_width(title, ink_ui_px(w));
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(text, 0, ink_ui_px(INK_CONTROL + INK_GAP_XS)); lv_obj_set_width(text, ink_ui_px(w));
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0); lv_obj_set_height(bg, ink_ui_px(INK_CONTROL));
    }
    if (!action->enabled) lv_obj_remove_flag(slot, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_add_flag(slot, LV_OBJ_FLAG_CLICKABLE);
}
static void refresh(void) { if (current()) ink_ui_list_refresh(S.list, S.count); }
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h;
    ink_settings_home_sync(); /* the home writes a changed value into its row */
    if (!strcmp(id, "battery.level") && S.page != DEVICE) return;
    refresh();
}
static void backup_reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != S.generation || !current() || S.page != STORAGE) return;
    const char *text = TR("备份记录暂时读不出来");
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    char phase[32] = ""; bool durable = false;
    if (j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        ink_json_str(j, ink_json_find(j, 0, "phase"), phase, sizeof phase);
        durable = ink_json_eq(j, ink_json_find(j, 0, "durable"), "true");
        text = !strcmp(phase, "none") ? TR("还没有备份") : !strcmp(phase, "saved") ?
            (durable ? TR("已备份") : TR("等待服务器保存回执")) : !strcmp(phase, "done") ? TR("已恢复") :
            !strcmp(phase, "failed") ? TR("备份或恢复失败") : TR("备份或恢复正在进行");
    }
    osal_free(j); snprintf(S.backup, sizeof S.backup, "%s", text); refresh();
}
static void backup_changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (current() && S.page == STORAGE) {
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = S.member, .reply_exec = INK_EXEC_UI,
            .done = backup_reply, .ctx = (void *)(uintptr_t)S.generation};
        if (ink_cap_call("backup.status", NULL, &o)) {
            snprintf(S.backup, sizeof S.backup, TR("备份记录暂时读不出来")); refresh();
        }
    }
}
static void device_reply(void *ctx, const ink_receipt_t *rc) {
    if ((uint32_t)(uintptr_t)ctx != S.generation || !current() || S.page != DEVICE) return;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result)))
        ink_json_int(j, ink_json_find(j, 0, "total"), &S.total);
    osal_free(j); refresh();
}
static void create(lv_obj_t *screen, void *arg) {
    S.page = (int)(intptr_t)arg; S.generation++;
    rows_of(S.page);
    snprintf(S.member, sizeof S.member, "%s", ink_member_current()->id);
    memset(&S.position, 0, sizeof S.position);
    snprintf(S.backup, sizeof S.backup, TR("正在读取备份记录"));
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR(PAGES[S.page].title));
    S.body = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS, INK_CANVAS_W,
        INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS, 0, INK_SURFACE_WHITE, false);
    const ink_ui_action_spec_t actions[] = {
        {.id = "prev", .label = TR("上一页"), .capability = "shell.settings_prev"},
        {.id = "next", .label = TR("下一页"), .capability = "shell.settings_next"}};
    ink_ui_page_spec_t page = {.id = PAGES[S.page].name, .title = TR(PAGES[S.page].title),
        .object = {PAGES[S.page].name, 0}, .actions = actions, .action_count = 2};
    int room = INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS - INK_CONTROL - 2 * INK_GAP;
    for (size_t i = 0; i < S.count; i++) value(i);
    /* Keep the voice controls together; every F4 page must fit the measured text. */
    for (S.capacity = S.page == VOICE || S.page == STORAGE ? 3 : (unsigned)S.count; S.capacity > 1; S.capacity--) {
        bool fits = true; int y = 0;
        for (size_t i = 0; i < S.count; i++) {
            if (!(i % S.capacity)) y = 0;
            y += row_height(i) + INK_STROKE;
            if (y > room) fits = false;
        }
        if (fits) break;
    }
    for (size_t i = 0; ink_sr_target && i < S.count; i++) /* settings.open: the page with its row, framed */
        if (S.items[i] == ink_sr_target) {
            S.position.page = i / S.capacity;
            snprintf(S.position.selected, sizeof S.position.selected, "%s", S.rows[i].id);
        }
    int cell_h = room / (int)S.capacity - INK_GAP_M;
    S.list = ink_ui_list_create(S.body, &page, &S.position, S.capacity, 1, cell_h, TR("这里还没有内容"), bind_row, NULL);
    S.total = -1;
    refresh(); backup_changed(NULL, NULL, NULL); storage_request(NULL, NULL, NULL);
    if (S.page == DEVICE) {
        ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = S.member, .reply_exec = INK_EXEC_UI, .done = device_reply, .ctx = (void *)(uintptr_t)S.generation};
        ink_cap_call("file.free", NULL, &o);
    }
}
static void destroy(void) { S.generation++; S.list = NULL; S.body = NULL; }
static int snapshot(char *out, size_t cap) {
    if (!current()) return -1;
    int n = ink_ui_list_snapshot(S.list, out, cap);
    if (n < 0) return -1;
    ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)n - 1};
    ink_jw_raw(&w, ",\"notes\":["); bool comma = false;
    size_t end = (S.position.page + 1) * S.capacity;
    for (size_t i = S.position.page * S.capacity; i < end && i < S.count; i++) {
        const row_t *r = &S.rows[i];
        const char *texts[] = {r->section, S.state[i] == INK_SR_UNSUPPORTED ? NULL : r->note};
        for (size_t k = 0; k < 2; k++) if (texts[k] && texts[k][0]) {
            if (comma) ink_jw_raw(&w, ",");
            ink_jw_str(&w, TR(texts[k])); comma = true;
        }
    }
    ink_jw_raw(&w, "]}");
    if (w.overflow) { out[0] = 0; return -1; }
    return (int)w.len;
}
static bool key(ink_key_t k, bool lng) { return current() && ink_ui_list_key(S.list, k, lng); }
static const ink_activity_t ACTIVITIES[] = {
    {.name = "SetVoice", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key},
    {.name = "SetStorage", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key},
    {.name = "SetAbout", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key},
    {.name = "SetPrivacy", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key},
    {.name = "DeviceDetails", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key},
    {.name = "Licenses", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key}};
void ink_settings_page_open(const char *name) {
    if (ink_member_current()->role == INK_ROLE_CHILD || !ink_member_space_open()) return;
    for (size_t i = 0; i < sizeof PAGES / sizeof *PAGES; i++) if (!strcmp(name, PAGES[i].name)) {
        ink_shell_close(); ink_ui_push(&ACTIVITIES[i], (void *)(intptr_t)i); return;
    }
}
static struct { int index, height; ink_ui_list_t *list; ink_ui_list_position_t position; char text[1024]; } L INK_PSRAM_BSS;
static size_t license_chunk(size_t start) {
    const char *text = LICENSE_TEXTS[L.index] + start;
    size_t n = strlen(text); if (n >= sizeof L.text) n = sizeof L.text - 1;
    while (n && text[n] && text[n] != ' ' && text[n] != '\n') n--;
    memcpy(L.text, text, n); L.text[n] = 0;
    while (n && ink_ui_text_height(L.text, INK_FONT_BODY, INK_WEIGHT_REGULAR, INK_CANVAS_W - 2 * INK_MARGIN) > L.height) {
        do { n--; } while (n && L.text[n] != ' ' && L.text[n] != '\n');
        L.text[n] = 0;
    }
    return n;
}
static void license_bind(lv_obj_t *slot, size_t i, ink_ui_field_spec_t *field, ink_ui_action_spec_t *action, void *ctx) {
    (void)ctx; *action = (ink_ui_action_spec_t){.id = "license.text"};
    size_t start = 0; for (size_t k = 0; k < i; k++) start += license_chunk(start);
    license_chunk(start); lv_obj_clean(slot); lv_obj_remove_flag(slot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(slot, 0, 0); lv_obj_set_style_radius(slot, 0, 0);
    ink_ui_label(slot, L.text, INK_FONT_BODY, INK_WEIGHT_REGULAR, 0, 0, INK_CANVAS_W - 2 * INK_MARGIN);
    *field = (ink_ui_field_spec_t){.id = "license.text", .role = "text", .label = TR("许可全文"), .value = L.text};
}
static void license_create(lv_obj_t *screen, void *arg) {
    L.index = (int)(intptr_t)arg; memset(&L.position, 0, sizeof L.position);
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR("许可全文"));
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS;
    L.height = INK_CANVAS_H - y - INK_CONTROL - 3 * INK_GAP;
    lv_obj_t *body = ink_ui_box(screen, 0, y, INK_CANVAS_W, INK_CANVAS_H - y, 0, INK_SURFACE_WHITE, false);
    const ink_ui_action_spec_t actions[] = {
        {.id = "prev", .label = TR("上一页"), .capability = "shell.settings_prev"},
        {.id = "next", .label = TR("下一页"), .capability = "shell.settings_next"}};
    ink_ui_page_spec_t page = {.id = "LicenseText", .title = LICENSE_ROWS[L.index].title, .object = {LICENSE_ROWS[L.index].id, 0}, .actions = actions, .action_count = 2};
    L.list = ink_ui_list_create(body, &page, &L.position, 1, 1, L.height, "", license_bind, NULL);
    size_t count = 0, start = 0, n;
    while ((n = license_chunk(start))) { start += n; count++; }
    if (L.list) ink_ui_list_refresh(L.list, count);
}
static void license_destroy(void) { L.list = NULL; }
static int license_snapshot(char *out, size_t cap) { return L.list ? ink_ui_list_snapshot(L.list, out, cap) : -1; }
static bool license_key(ink_key_t k, bool lng) { return L.list && ink_ui_list_key(L.list, k, lng); }
static const ink_activity_t LICENSE_TEXT = {.name = "LicenseText", .create = license_create, .destroy = license_destroy, .snapshot = license_snapshot, .on_key = license_key};
static void license_open(ink_call_t *c) {
    int64_t i = -1; ink_args_get_int(&c->args, "index", &i);
    if (i < 0 || (uint64_t)i >= sizeof LICENSE_ROWS / sizeof *LICENSE_ROWS) { c->rc.status = INK_RC_INVALID; return; }
    ink_shell_close(); ink_ui_push(&LICENSE_TEXT, (void *)(intptr_t)i);
}
static void navigate(ink_call_t *c) {
    if (!strcmp(c->cap->id, "shell.license")) { license_open(c); return; }
    if (!strcmp(c->cap->id, "shell.settings_prev") || !strcmp(c->cap->id, "shell.settings_next")) {
        if (L.list && !strcmp(ink_ui_page_name(), "LicenseText")) { ink_ui_list_turn(L.list, !strcmp(c->cap->id, "shell.settings_next") ? 1 : -1); return; }
        if (!current()) c->rc.status = INK_RC_STALE;
        else ink_ui_list_turn(S.list, !strcmp(c->cap->id, "shell.settings_next") ? 1 : -1);
    } else for (size_t i = 0; i < sizeof PAGES / sizeof *PAGES; i++)
        if (!strcmp(c->cap->id, PAGES[i].cap)) ink_settings_page_open(PAGES[i].name);
}
static const ink_cap_t CAPS[] = {
    {.id = "shell.voice", .title = "打开语音和 AI 设置", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.storage", .title = "打开存储和备份设置", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.about", .title = "打开关于和更新", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.privacy", .title = "打开隐私", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.device", .title = "查看设备详情", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.licenses", .title = "查看开源许可", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.license", .title = "阅读一项开源许可", .input = "{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":14}},\"required\":[\"index\"]}", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.settings_prev", .title = "设置上一页", .effect = INK_FX_NAVIGATE, .run = navigate},
    {.id = "shell.settings_next", .title = "设置下一页", .effect = INK_FX_NAVIGATE, .run = navigate}};
int ink_settings_pages_init(void) {
    if (ink_settings_home_init() || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) || ink_settings_watch("", changed, NULL, INK_EXEC_UI) ||
        ink_state_watch("storage.", changed, NULL, INK_EXEC_UI) || ink_state_watch("update.", changed, NULL, INK_EXEC_UI) || ink_state_watch("battery.level", changed, NULL, INK_EXEC_UI)) return -1;
    if (ink_cap_subscribe("file.changed", storage_request, NULL, INK_EXEC_UI)) return -1;
    return ink_cap_subscribe("backup.changed", backup_changed, NULL, INK_EXEC_UI);
}

static struct { uint32_t generation; int state[2]; int64_t bytes[2][4], clean[3], revision; } U INK_PSRAM_BSS;
static void storage_size(int64_t n, char *out, size_t cap) {
    int64_t unit = n >= 1073741824 ? 1073741824 : n >= 1048576 ? 1048576 : n >= 1024 ? 1024 : 1;
    if (unit == 1) snprintf(out, cap, "%lld B", (long long)n);
    else snprintf(out, cap, "%lld.%lld %s", (long long)(n / unit), (long long)(n % unit * 10 / unit),
        unit == 1024 ? "KB" : unit == 1048576 ? "MB" : "GB");
}
static void storage_value(size_t i, char *out, size_t cap) {
    int state = U.generation == S.generation ? U.state[i == 5] : 0;
    if (state != 1) {
        snprintf(out, cap, "%s%s%s", state ? TR("暂时读不出来") : TR("正在整理"), state ? " · " : "", state ? TR("再试一次") : ""); return;
    }
    char a[32], b[32];
    if (i == 5) {
        storage_size(U.clean[0] + U.clean[1] + U.clean[2], a, sizeof a);
        snprintf(out, cap, TR("能腾出 %s"), a);
    } else {
        int k = (int)i - 2;
        storage_size(U.bytes[0][k] + (k == 2 ? U.bytes[0][3] : 0), a, sizeof a);
        storage_size(U.bytes[1][k] + (k == 2 ? U.bytes[1][3] : 0), b, sizeof b);
        snprintf(out, cap, TR("我 %s / 全机 %s"), a, b);
    }
}
static void storage_action(size_t i, ink_ui_action_spec_t *action) {
    if (i > 1 && U.generation == S.generation && U.state[i == 5] == 2) {
        action->capability = "shell.storage"; action->args_json = NULL; action->enabled = true; return;
    }
    if (i != 5) return;
    action->capability = U.clean[0] || U.clean[2] ? "file.cleanup" : "file.cache_clear";
    snprintf(S.args[i], sizeof S.args[i], "{\"revision\":%lld}", (long long)U.revision);
    action->args_json = S.args[i];
    action->enabled = U.generation == S.generation && U.state[1] == 1 && U.clean[0] + U.clean[1] + U.clean[2] > 0;
}
static void storage_reply(void *ctx, const ink_receipt_t *rc, bool clean) {
    if ((uint32_t)(uintptr_t)ctx != S.generation || !current() || S.page != STORAGE) return;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    U.state[clean] = 2;
    if (j && rc->status == INK_RC_DONE && rc->result && !ink_json_parse(j, rc->result, strlen(rc->result))) {
        if (!clean && strstr(rc->result, "\"organizing\"")) { U.state[0] = 0; osal_free(j); refresh(); return; }
        U.state[clean] = 1;
        static const char *const keys[] = {"books", "audio", "photos", "other", "temporary", "cache", "partial"};
        for (int s = 0; s < (clean ? 1 : 2); s++) {
            int root = clean ? 0 : ink_json_find(j, 0, s ? "device" : "member");
            for (int k = 0; k < (clean ? 3 : 4); k++)
                ink_json_int(j, ink_json_find(j, root, keys[clean ? k + 4 : k]), clean ? &U.clean[k] : &U.bytes[s][k]);
        }
        if (clean) ink_json_int(j, ink_json_find(j, 0, "revision"), &U.revision);
    }
    osal_free(j); refresh();
}
static void storage_usage_reply(void *ctx, const ink_receipt_t *rc) { storage_reply(ctx, rc, false); }
static void storage_clean_reply(void *ctx, const ink_receipt_t *rc) { storage_reply(ctx, rc, true); }
static void storage_request(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (!current() || S.page != STORAGE) return;
    memset(&U, 0, sizeof U); U.generation = S.generation;
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .member = S.member, .reply_exec = INK_EXEC_UI,
        .done = storage_usage_reply, .ctx = (void *)(uintptr_t)S.generation};
    if (ink_cap_call("file.usage", NULL, &o)) U.state[0] = 2;
    o.done = storage_clean_reply;
    if (ink_cap_call("file.cleanup_preview", NULL, &o)) U.state[1] = 2;
    refresh();
}
