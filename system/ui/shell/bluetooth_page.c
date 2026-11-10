/* Headset presentation uses the existing Bluetooth capabilities and F4 list. */
/* ui-scale-runtime: dimension B.y,head,h,w Measured theme rows and section spacing. */
#include "ink/sys/i18n.h"
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/ui_list.h"
#include "ink/ui_shell.h"

static const ink_activity_t PAGE;
static struct {
    struct { char addr[18], name[32]; bool paired; } devices[24];
    unsigned epoch; size_t count; int y; bool busy;
    char status[sizeof ((ink_receipt_t *)0)->say], member[16], args[26][160], values[24][64]; /* status shows a receipt's say whole */
    ink_ui_action_spec_t actions[26];
    ink_ui_list_t *list; ink_ui_list_position_t position; lv_obj_t *body, *notice;
} B INK_PSRAM_BSS;
static bool current(void) {
    return B.body && ink_ui_page_current() && !strcmp(ink_ui_page_name(), PAGE.name) &&
        ink_member_space_open() && !strcmp(B.member, ink_member_current()->id);
}
static void draw(void);
static void request(const char *cap, const char *args, unsigned kind);
static void reply(void *ctx, const ink_receipt_t *rc) {
    unsigned token = (unsigned)(uintptr_t)ctx, kind = token % 4;
    if (token / 4 != B.epoch || !current()) return;
    if (kind) B.busy = false;
    if (rc->status != INK_RC_DONE) {
        snprintf(B.status, sizeof B.status, "%s", strstr(rc->detail, "timed out") ? TR("操作超时，请重试") :
            strstr(rc->detail, "pair") ? TR("配对失败，请确认耳机已开机") : TR("没有完成，请重试"));
    } else if (kind < 2 && rc->result) {
        if (!kind) B.count = 0;
        ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
        if (j && !ink_json_parse(j, rc->result, strlen(rc->result))) {
            for (int t = 1; t < j->ntok; t++) if (j->tok[t].type == 1) {
                char addr[18], name[32];
                if (ink_json_str(j, ink_json_find(j, t, "addr"), addr, sizeof addr) <= 0) continue;
                ink_json_str(j, ink_json_find(j, t, "name"), name, sizeof name);
                size_t i = 0;
                while (i < B.count && strcmp(B.devices[i].addr, addr)) i++;
                if (i == B.count && B.count < 24) {
                    snprintf(B.devices[i].addr, sizeof B.devices[i].addr, "%s", addr);
                    snprintf(B.devices[i].name, sizeof B.devices[i].name, "%s", name);
                    B.devices[i].paired = !kind; B.count++;
                }
            }
            if (kind) snprintf(B.status, sizeof B.status, "%s", j->tok[0].size ? TR("搜索完成") : TR("没有搜到耳机，请确认已开机"));
        } else snprintf(B.status, sizeof B.status, TR("耳机列表暂时读不出来"));
        osal_free(j);
    } else if (kind == 2) {
        snprintf(B.status, sizeof B.status, "%s", rc->say[0] ? rc->say : TR("已完成"));
        request("bt.list", NULL, 0);
    }
    if (!ink_setting_get("bt.enabled")) {
        for (size_t i = B.count; i > 0; i--) if (!B.devices[i - 1].paired) B.count = i - 1;
        snprintf(B.status, sizeof B.status, TR("蓝牙已关闭"));
    }
    draw();
}
static void request(const char *cap, const char *args, unsigned kind) {
    ink_call_opts_t o = {.src = kind ? INK_SRC_TOUCH : INK_SRC_SYSTEM, .member = B.member,
        .args_json = args, .reply_exec = INK_EXEC_UI, .done = reply, .ctx = (void *)(uintptr_t)(B.epoch * 4 + kind)};
    if (kind) {
        if (kind == 1) while (B.count && !B.devices[B.count - 1].paired) B.count--;
        B.busy = true;
        snprintf(B.status, sizeof B.status, "%s", kind == 1 ? TR("正在搜索耳机") : !strcmp(cap, "bt.pair") ? TR("正在配对并连接") : TR("正在处理"));
    }
    if (ink_cap_call(cap, NULL, &o)) { B.busy = false; snprintf(B.status, sizeof B.status, TR("没有完成，请重试")); }
}
static void touch(lv_event_t *e) {
    size_t i = (size_t)(uintptr_t)lv_event_get_user_data(e);
    ink_ui_action_spec_t a = B.actions[i];
    if (B.busy && i) return;
    if (lv_event_get_code(e) == LV_EVENT_LONG_PRESSED) {
        if (!i || i > B.count || !B.devices[i - 1].paired) return;
        a.capability = "bt.forget";
    }
    if (!a.enabled) return;
    request(a.capability, a.args_json, !strcmp(a.capability, "bt.scan") ? 1 : 2); draw();
}
static void bind_row(lv_obj_t *slot, size_t i, ink_ui_field_spec_t *f, ink_ui_action_spec_t *a, void *ctx) {
    (void)ctx;
    int w = INK_CANVAS_W - 2 * INK_MARGIN, head = i == 1 ? ink_ui_line_height(INK_FONT_SUBTITLE) + INK_GAP : 0;
    int h = i && i <= B.count ? ink_ui_row_double() : INK_CONTROL;
    if (!(i % 3)) B.y = 0;
    lv_obj_set_pos(slot, ink_ui_px(INK_MARGIN), ink_ui_px(B.y)); B.y += head + h + INK_GAP;
    lv_obj_set_height(slot, ink_ui_px(head + h)); lv_obj_clean(slot); lv_obj_remove_flag(slot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(slot, 0, 0); lv_obj_set_style_bg_opa(slot, LV_OPA_TRANSP, 0);
    *f = (ink_ui_field_spec_t){.id = "bt.enabled", .role = "setting", .label = TR("蓝牙"), .value = ink_setting_get("bt.enabled") ? TR("开") : TR("关")};
    *a = (ink_ui_action_spec_t){.id = "toggle", .label = TR("蓝牙"), .capability = "settings.set", .args_json = B.args[i], .enabled = true};
    lv_obj_t *row;
    if (!i) {
        snprintf(B.args[i], sizeof B.args[i], "{\"id\":\"bt.enabled\",\"value\":%d}", !ink_setting_get("bt.enabled"));
        row = ink_ui_card(slot, 0, 0, w, h, INK_SURFACE_STATIC);
        ink_ui_circle(row, "bluetooth", INK_GAP, (h - INK_CIRCLE_SMALL) / 2, INK_CIRCLE_SMALL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
        row = ink_ui_toggle(row, TR("蓝牙"), ink_setting_get("bt.enabled"), INK_CIRCLE_SMALL + INK_GAP_XL, 0, w - INK_CIRCLE_SMALL - 2 * INK_GAP_XL, NULL, NULL);
    } else if (i <= B.count) {
        size_t d = i - 1; char connected[18] = "";
        ink_state_get_str(ink_state_find("bt.device"), connected, sizeof connected);
        bool on = !strcmp(connected, B.devices[d].addr);
        snprintf(B.values[d], sizeof B.values[d], "%s", on ? TR("已连接") : B.devices[d].paired ? TR("没有连接") : TR("点按配对并连接"));
        ink_jw_t j; ink_jw_init(&j, B.args[i], sizeof B.args[i]);
        ink_jw_raw(&j, "{\"addr\":"); ink_jw_str(&j, B.devices[d].addr);
        ink_jw_raw(&j, ",\"name\":"); ink_jw_str(&j, B.devices[d].name); ink_jw_raw(&j, ",\"connect\":true}");
        *f = (ink_ui_field_spec_t){.id = B.devices[d].addr, .role = "listitem", .label = B.devices[d].name, .value = B.values[d], .object = {B.devices[d].addr, 0}};
        *a = (ink_ui_action_spec_t){.id = B.devices[d].addr, .label = B.devices[d].name, .capability = on ? "bt.disconnect" : B.devices[d].paired ? "bt.connect" : "bt.pair", .args_json = B.args[i], .object = f->object, .enabled = ink_setting_get("bt.enabled") && !B.busy};
        if (head) ink_ui_label(slot, TR("耳机"), INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, 0, 0, w);
        row = ink_ui_card(slot, 0, head, w, h, INK_SURFACE_STATIC);
        ink_ui_circle(row, "headphones", INK_GAP, (h - INK_CIRCLE_SMALL) / 2, INK_CIRCLE_SMALL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL);
        int left = INK_CIRCLE_SMALL + INK_GAP_XL;
        ink_ui_line(row, B.devices[d].name[0] ? B.devices[d].name : TR("蓝牙耳机"), INK_FONT_LIST, INK_WEIGHT_REGULAR, left, INK_GAP, w - left - INK_CONTROL);
        ink_ui_line(row, B.values[d], INK_FONT_NOTE, INK_WEIGHT_REGULAR, left, INK_GAP + ink_ui_line_height(INK_FONT_LIST), w - left - INK_GAP);
        if (on) ink_ui_icon(row, "check", INK_ICON_SMALL, INK_ICON_LINE, w - INK_CONTROL, (h - INK_ICON_SMALL) / 2, false);
        lv_obj_add_event_cb(row, touch, LV_EVENT_LONG_PRESSED, (void *)(uintptr_t)i);
    } else {
        *f = (ink_ui_field_spec_t){.id = "search", .role = "status", .label = TR("搜索耳机"), .value = B.status};
        *a = (ink_ui_action_spec_t){.id = "search", .label = TR("搜索耳机"), .capability = "bt.scan", .enabled = ink_setting_get("bt.enabled") && !B.busy};
        row = ink_ui_pill(slot, TR("搜索耳机"), 0, 0, w, h, false, NULL, NULL);
    }
    B.actions[i] = *a; lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, touch, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)i);
}
static void draw(void) {
    if (!current()) return;
    ink_ui_list_refresh(B.list, B.count + 2);
    if (B.count < 2) lv_obj_add_flag(lv_obj_get_child(B.body, -1), LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(B.notice, B.status); ink_ui_content_changed();
}
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h; (void)id;
    if (!ink_setting_get("bt.enabled")) {
        B.busy = false;
        while (B.count && !B.devices[B.count - 1].paired) B.count--;
        snprintf(B.status, sizeof B.status, TR("蓝牙已关闭"));
    } else if (!strcmp(B.status, TR("蓝牙已关闭"))) B.status[0] = 0;
    draw();
}
static void event(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (current() && !B.busy) request("bt.list", NULL, 0);
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg; B.epoch++; B.count = 0; B.busy = false; memset(&B.position, 0, sizeof B.position);
    snprintf(B.member, sizeof B.member, "%s", ink_member_current()->id); B.status[0] = 0;
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, TR("蓝牙"));
    B.body = ink_ui_box(screen, 0, INK_STATUS_BAR + INK_PRIMARY + INK_GAP_XS, INK_CANVAS_W,
        INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_GAP_XS, 0, INK_SURFACE_WHITE, false);
    const ink_ui_action_spec_t actions[] = {{.id = "prev", .label = TR("上一页"), .capability = "shell.bluetooth_prev"},
        {.id = "next", .label = TR("下一页"), .capability = "shell.bluetooth_next"}};
    ink_ui_page_spec_t page = {.id = "SetBluetooth", .title = TR("蓝牙"), .object = {"SetBluetooth", 0}, .actions = actions, .action_count = 2};
    B.list = ink_ui_list_create(B.body, &page, &B.position, 3, 1, ink_ui_row_double() + INK_GAP_L, TR("还没有耳机"), bind_row, NULL);
    B.notice = ink_ui_notice(B.body, "", INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - 2 * INK_CONTROL);
    ink_ui_notice(B.body, TR("长按已配对的耳机可忘记"), INK_CANVAS_H - INK_STATUS_BAR - INK_PRIMARY - INK_CONTROL);
    draw(); request("bt.list", NULL, 0);
}
static void destroy(void) { B.epoch++; B.body = NULL; B.list = NULL; }
static int snapshot(char *out, size_t cap) {
    int n = current() ? ink_ui_list_snapshot(B.list, out, cap) : -1;
    if (n < 0) return -1;
    ink_jw_t w = {.buf = out, .cap = cap, .len = (size_t)n - 1};
    ink_jw_raw(&w, ",\"status\":"); ink_jw_str(&w, B.status);
    ink_jw_raw(&w, ",\"note\":"); ink_jw_str(&w, TR("长按已配对耳机可忘记；AI 使用 bt.forget(addr)")); ink_jw_raw(&w, "}");
    return w.overflow ? -1 : (int)w.len;
}
static bool key(ink_key_t k, bool lng) { return current() && ink_ui_list_key(B.list, k, lng); }
static const ink_activity_t PAGE = {.name = "SetBluetooth", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key};
void ink_bluetooth_page_open(void) { ink_shell_close(); ink_ui_push(&PAGE, NULL); }
static void navigate(ink_call_t *c) {
    if (!strcmp(c->cap->id, "shell.bluetooth")) ink_bluetooth_page_open();
    else if (current()) { ink_ui_list_turn(B.list, !strcmp(c->cap->id, "shell.bluetooth_next") ? 1 : -1); }
    else c->rc.status = INK_RC_STALE;
}
int ink_bluetooth_page_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "shell.bluetooth", .title = "打开蓝牙耳机", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
        {.id = "shell.bluetooth_prev", .title = "耳机上一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate},
        {.id = "shell.bluetooth_next", .title = "耳机下一页", .effect = INK_FX_NAVIGATE, .child_ok = true, .run = navigate}};
    if (ink_cap_register(caps, 3) || ink_state_watch("bt.", changed, NULL, INK_EXEC_UI) || ink_settings_watch("bt.", changed, NULL, INK_EXEC_UI)) return -1;
    return ink_cap_subscribe("bt.changed", event, NULL, INK_EXEC_UI);
}
