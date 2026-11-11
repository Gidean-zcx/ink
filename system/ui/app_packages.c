/* Bounded data-only apps. Card IO on BG, definitions and LVGL on UI. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/appreg.h"
#include "ink/sys/file.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/log.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/ui_shell.h"

enum { PACKAGES = 8, PAGES = 4, NODES = 6, PERMISSIONS = 12, OPTIONS = 3, JSON_BYTES = 4096 };
typedef struct {
    char id[24], kind[12], label[96], value[128], capability[48], args[256], from[48];
    char options[OPTIONS][32];
    int count;
} node_t;
typedef struct { char id[24], title[64]; node_t nodes[NODES]; int count; } page_t;
typedef struct {
    ink_app_def_t def;
    ink_activity_t activity;
    char id[32], title[64], icon[16], version[24], permission[PERMISSIONS][48];
    const char *permissions[PERMISSIONS + 1];
    page_t pages[PAGES];
    int count;
} package_t;
static struct {
    package_t *installed[PACKAGES], *staged[PACKAGES];
    ink_exec_job_t scan, apply;
    bool scanning, again;
} PK INK_PSRAM_BSS;
static struct {
    package_t *package;
    page_t page;
    int index, offset;
    lv_obj_t *body, *widgets[NODES], *notice;
    ink_ui_field_spec_t fields[NODES + 1];
    char descriptions[NODES][256];
    ink_ui_action_spec_t actions[NODES + 4];
    char action_args[NODES][64], message[160], uninstall[96];
    ink_ui_page_spec_t spec;
} RP INK_PSRAM_BSS;

static int after(const ink_json_t *j, int t) {
    int i = t + 1;
    while (i < j->ntok && j->tok[i].start < j->tok[t].end) i++;
    return i;
}
/* ink_json_str allows a short output; package strings must fit in full. */
static bool string(const ink_json_t *j, int t, char *out, size_t cap) {
    if (t < 0 || j->tok[t].type != 4 || (size_t)(j->tok[t].end - j->tok[t].start) >= cap) return false;
    for (int i = j->tok[t].start; i + 5 < j->tok[t].end; i++)
        if (!memcmp(j->text + i, "\\u0000", 6)) return false;
    return ink_json_str(j, t, out, cap) >= 0;
}
static bool property(const ink_json_t *j, int t, const char *key, char *out, size_t cap, bool required) {
    int v = ink_json_find(j, t, key);
    return v < 0 ? !required : string(j, v, out, cap) && (!required || out[0]);
}
static bool raw(const ink_json_t *j, int t, char *out, size_t cap) {
    if (t < 0) return false;
    size_t n = (size_t)(j->tok[t].end - j->tok[t].start);
    if (n >= cap) return false;
    memcpy(out, j->text + j->tok[t].start, n); out[n] = 0;
    return true;
}
static bool read_json(const char *path, char *buf, ink_json_t *j) {
    int64_t size = hal_fs_size(path);
    if (size <= 0 || size > JSON_BYTES) return false;
    hal_file_t *f = hal_fs_open(path, HAL_O_READ);
    if (!f) return false;
    size_t got = 0;
    while (got < (size_t)size) {
        ssize_t n = hal_fs_read(f, buf + got, (size_t)size - got);
        if (n <= 0) break;
        got += (size_t)n;
    }
    int closed = hal_fs_close(f);
    buf[got] = 0;
    return !closed && got == (size_t)size && ink_json_parse(j, buf, got) == 0 && j->tok[0].type == 1 && after(j, 0) == j->ntok;
}
static bool parse_node(const ink_json_t *j, int t, node_t *n) {
    if (j->tok[t].type != 1 || !property(j, t, "id", n->id, sizeof n->id, true) || !ink_layout_name_ok(n->id) ||
        !property(j, t, "type", n->kind, sizeof n->kind, true) || !property(j, t, "label", n->label, sizeof n->label, true) ||
        !property(j, t, "capability", n->capability, sizeof n->capability, false) ||
        !property(j, t, "from", n->from, sizeof n->from, false)) return false;
    static const char *const KINDS[] = {"text", "list", "input", "toggle", "segments", "slider", "button"};
    bool known = false;
    for (unsigned i = 0; i < sizeof KINDS / sizeof *KINDS; i++) known |= !strcmp(n->kind, KINDS[i]);
    if (!known) return false;
    int v = ink_json_find(j, t, "value");
    if (v >= 0 && !(j->tok[v].type == 4 ? string(j, v, n->value, sizeof n->value) : raw(j, v, n->value, sizeof n->value))) return false;
    snprintf(n->args, sizeof n->args, "{}");
    v = ink_json_find(j, t, "args");
    if (v >= 0 && (j->tok[v].type != 1 || !raw(j, v, n->args, sizeof n->args))) return false;
    v = ink_json_find(j, t, "options");
    if (!strcmp(n->kind, "segments")) {
        if (v < 0 || j->tok[v].type != 2 || j->tok[v].size < 1 || j->tok[v].size > OPTIONS) return false;
        for (int k = v + 1; k < after(j, v); k = after(j, k))
            if (!string(j, k, n->options[n->count++], sizeof n->options[0])) return false;
    }
    if (!strncmp(n->capability, "app.page_", 9) || !strncmp(n->capability, "confirm.", 8)) return false;
    v = ink_json_find(j, t, "value");
    if (!strcmp(n->kind, "toggle")) {
        if (v >= 0 && !ink_json_eq(j, v, "true") && !ink_json_eq(j, v, "false")) return false;
        if (v < 0) snprintf(n->value, sizeof n->value, "false");
    } else if (!strcmp(n->kind, "slider") || !strcmp(n->kind, "segments")) {
        int64_t number = 0;
        if (v >= 0 && !ink_json_int(j, v, &number)) return false;
        if (number < 0 || number > (!strcmp(n->kind, "slider") ? 10 : n->count - 1)) return false;
        if (v < 0) snprintf(n->value, sizeof n->value, "0");
    } else if (v >= 0 && j->tok[v].type != 4) return false;
    return true;
}
static bool load_package(package_t *bundle, const char *id, char *buf, ink_json_t *j) {
    char path[128]; int64_t format = 0;
    snprintf(path, sizeof path, "/content/apps/%s/manifest.json", id);
    if (!read_json(path, buf, j) || !ink_json_int(j, ink_json_find(j, 0, "format"), &format) || format != 1 ||
        !property(j, 0, "id", bundle->id, sizeof bundle->id, true) || strcmp(id, bundle->id) ||
        !property(j, 0, "name", bundle->title, sizeof bundle->title, true) || !property(j, 0, "icon", bundle->icon, sizeof bundle->icon, true) ||
        !property(j, 0, "version", bundle->version, sizeof bundle->version, true)) return false;
    int v = ink_json_find(j, 0, "permissions");
    if (v < 0 || j->tok[v].type != 2 || j->tok[v].size > PERMISSIONS) return false;
    int n = 0;
    for (int k = v + 1; k < after(j, v); k = after(j, k)) {
        if (!string(j, k, bundle->permission[n], sizeof bundle->permission[0])) return false;
        bundle->permissions[n] = bundle->permission[n]; n++;
    }
    v = ink_json_find(j, 0, "pages");
    if (v < 0 || j->tok[v].type != 2 || j->tok[v].size < 1 || j->tok[v].size > PAGES) return false;
    for (int k = v + 1; k < after(j, v); k = after(j, k)) {
        page_t *page = &bundle->pages[bundle->count++];
        if (!string(j, k, page->id, sizeof page->id) || !ink_layout_name_ok(page->id)) return false;
        for (int i = 0; i < bundle->count - 1; i++) if (!strcmp(bundle->pages[i].id, page->id)) return false;
    }
    for (int i = 0; i < bundle->count; i++) {
        page_t *page = &bundle->pages[i];
        snprintf(path, sizeof path, "/content/apps/%s/%s.json", id, page->id);
        if (!read_json(path, buf, j) || !property(j, 0, "title", page->title, sizeof page->title, true)) return false;
        v = ink_json_find(j, 0, "nodes");
        if (v < 0 || j->tok[v].type != 2 || j->tok[v].size > NODES) return false;
        for (int k = v + 1; k < after(j, v); k = after(j, k)) {
            node_t *node = &page->nodes[page->count++];
            if (!parse_node(j, k, node)) return false;
            for (int q = 0; q < page->count - 1; q++) if (!strcmp(page->nodes[q].id, node->id)) return false;
        }
    }
    return true;
}

static void draw(void);
static void request_scan(void);
static int node_index(const char *id) {
    for (int i = 0; i < RP.page.count; i++) if (!strcmp(RP.page.nodes[i].id, id)) return i;
    return -1;
}
static bool declared(const char *cap) { return ink_app_foreground_may(cap); }
static void capture_inputs(void) {
    for (int i = 0; i < RP.page.count; i++) if (RP.widgets[i] && !strcmp(RP.page.nodes[i].kind, "input"))
        snprintf(RP.page.nodes[i].value, sizeof RP.page.nodes[i].value, "%s", ink_textfield_text(RP.widgets[i]));
}
typedef struct { ink_call_t *call; uint32_t generation, epoch; int page; } reply_t;
static void receipt(void *ctx, const ink_receipt_t *rc) {
    reply_t *reply = ctx;
    if (RP.body && ink_ui_page_current() && reply->generation == ink_ui_generation() && reply->epoch == ink_member_epoch() && reply->page == RP.index) {
        capture_inputs();
        ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
        if (j && rc->status == INK_RC_DONE && rc->result && ink_json_parse(j, rc->result, strlen(rc->result)) == 0)
            for (int i = 0; i < RP.page.count; i++) {
                node_t *n = &RP.page.nodes[i];
                int t = n->from[0] ? ink_json_find(j, 0, n->from) : -1;
                if (t >= 0) {
                    if (j->tok[t].type == 4) string(j, t, n->value, sizeof n->value);
                    else raw(j, t, n->value, sizeof n->value);
                }
            }
        osal_free(j);
        snprintf(RP.message, sizeof RP.message, "%s", rc->say[0] ? rc->say : rc->status == INK_RC_DONE ? "已完成" : rc->detail);
        draw();
    }
    reply->call->rc = *rc; reply->call->rc.result = NULL;
    if (rc->result) ink_cap_result(reply->call, rc->result);
    ink_cap_done(reply->call); osal_free(reply);
}
static void invalid(ink_call_t *c, const char *why) {
    c->rc.status = INK_RC_INVALID;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
}
static void action(ink_call_t *c) {
    char id[24]; ink_args_get_str(&c->args, "id", id, sizeof id);
    int at = node_index(id);
    if (!RP.body || at < 0 || !ink_app_visible(&RP.package->def)) { invalid(c, "没有这个控件或应用不可用"); return; }
    node_t *n = &RP.page.nodes[at]; capture_inputs();
    if (!strcmp(n->kind, "input") && !ink_args_find(&c->args, "value")) {
        if (!RP.widgets[at] || ink_textfield_short_focus(RP.widgets[at])) invalid(c, "输入框不在当前页");
        return;
    }
    const ink_cap_t *cap = ink_cap_find(n->capability);
    if (!cap || !declared(n->capability) || (cap->permission && !declared(cap->permission)) ||
        (cap->expose && !(cap->expose & INK_EXPOSE_APP))) {
        c->rc.status = INK_RC_REJECTED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "应用没有声明这个能力或权限");
        return;
    }
    const ink_arg_t *value = ink_args_find(&c->args, "value");
    if (value) {
        if (strcmp(n->kind, "toggle") && strcmp(n->kind, "slider") && strcmp(n->kind, "segments")) {
            invalid(c, "普通输入请使用系统输入会话"); return;
        }
        if ((!strcmp(n->kind, "toggle") && value->type != INK_ARG_BOOL) ||
            ((!strcmp(n->kind, "slider") || !strcmp(n->kind, "segments")) &&
             (value->type != INK_ARG_INT || value->i < 0 || value->i > (!strcmp(n->kind, "slider") ? 10 : n->count - 1)))) {
            invalid(c, "控件的值不在范围内"); return;
        }
        snprintf(n->value, sizeof n->value, "%lld", (long long)value->i);
        if (value->type == INK_ARG_BOOL) snprintf(n->value, sizeof n->value, "%s", value->i ? "true" : "false");
    }
    ink_args_t *args = osal_malloc(sizeof *args * 2, OSAL_MEM_EXTERNAL);
    char why[96];
    if (!args || ink_args_from_json(args, n->args, strlen(n->args), NULL, why, sizeof why)) {
        osal_free(args); invalid(c, "动作参数无效"); return;
    }
    ink_args_t *bound = args + 1; ink_args_init(bound); int r = 0;
    for (int i = 0; i < args->n && !r; i++) {
        const ink_arg_t *v = &args->a[i]; const char *argument_key = ink_args_key(args, v), *argument_value = args->text + v->off;
        int ref = v->type == INK_ARG_STR && argument_value[0] == '$' ? node_index(argument_value + 1) : -1;
        if (v->type == INK_ARG_STR && argument_value[0] == '$' && ref < 0) { r = -1; break; }
        const node_t *field = ref < 0 ? NULL : &RP.page.nodes[ref];
        if (field && !strcmp(field->kind, "toggle")) r = ink_args_bool(bound, argument_key, !strcmp(field->value, "true"));
        else if (field && (!strcmp(field->kind, "slider") || !strcmp(field->kind, "segments"))) r = ink_args_int(bound, argument_key, atoi(field->value));
        else if (field || v->type == INK_ARG_STR) r = ink_args_str(bound, argument_key, field ? field->value : argument_value);
        else if (v->type == INK_ARG_BOOL) r = ink_args_bool(bound, argument_key, v->i != 0);
        else if (v->type == INK_ARG_INT) r = ink_args_int(bound, argument_key, v->i);
        else r = ink_args_json(bound, argument_key, argument_value, v->len);
    }
    reply_t *reply = !r ? osal_malloc(sizeof *reply, OSAL_MEM_EXTERNAL) : NULL;
    if (!reply) { osal_free(args); invalid(c, "动作参数过长或内存不足"); return; }
    *reply = (reply_t){c, ink_ui_generation(), ink_member_epoch(), RP.index};
    ink_call_opts_t opts = {.src = c->src, .member = ink_cap_call_member(c), .done = receipt, .ctx = reply, .reply_exec = INK_EXEC_UI};
    ink_cap_defer(c);
    if (ink_cap_call(n->capability, bound, &opts)) {
        invalid(c, "系统繁忙"); ink_cap_done(c); osal_free(reply);
    }
    osal_free(args);
}
static void entered(void *ctx, const char *value) {
    node_t *n = &RP.page.nodes[(int)(intptr_t)ctx];
    snprintf(n->value, sizeof n->value, "%s", value);
    ink_ui_content_changed();
}
static void control(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e); node_t *n = &RP.page.nodes[i];
    ink_args_t args; ink_args_init(&args); ink_args_str(&args, "id", n->id);
    if (!strcmp(n->kind, "toggle")) ink_args_bool(&args, "value", strcmp(n->value, "true") != 0);
    else if (!strcmp(n->kind, "segments")) {
        lv_obj_t *button = lv_event_get_current_target(e);
        ink_args_int(&args, "value", lv_obj_get_index(button));
    } else if (!strcmp(n->kind, "slider")) {
        lv_obj_t *obj = lv_event_get_current_target(e); lv_area_t a; lv_obj_get_coords(obj, &a);
        lv_point_t bundle; lv_indev_t *indev = lv_indev_active(); if (!indev) return;
        lv_indev_get_point(indev, &bundle);
        int v = (bundle.x - a.x1) * 10 / lv_area_get_width(&a);
        ink_args_int(&args, "value", v < 0 ? 0 : v > 10 ? 10 : v);
    }
    ink_call_opts_t opts = {.src = INK_SRC_TOUCH};
    ink_cap_call("app.page_action", &args, &opts);
}
static void draw(void) {
    memset(RP.widgets, 0, sizeof RP.widgets); lv_obj_clean(RP.body);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, y = 0;
    for (int i = 0; i < RP.page.count; i++) {
        node_t *n = &RP.page.nodes[i];
        RP.fields[i] = (ink_ui_field_spec_t){.id = n->id, .role = n->kind, .label = n->label, .value = n->value};
        if (!strcmp(n->kind, "segments")) {
            ink_jw_t choices; ink_jw_init(&choices, RP.descriptions[i], sizeof RP.descriptions[i]);
            ink_jw_raw(&choices, "{\"options\":[");
            for (int k = 0; k < n->count; k++) {
                if (k) ink_jw_raw(&choices, ",");
                ink_jw_str(&choices, n->options[k]);
            }
            ink_jw_raw(&choices, "],\"selected\":"); ink_jw_str(&choices, n->value); ink_jw_raw(&choices, "}");
            RP.fields[i].value = RP.descriptions[i];
        } else if (!strcmp(n->kind, "slider")) {
            snprintf(RP.descriptions[i], sizeof RP.descriptions[i], "%s [0..10]", n->value);
            RP.fields[i].value = RP.descriptions[i];
        }
        snprintf(RP.action_args[i], sizeof RP.action_args[i], "{\"id\":\"%s\"}", n->id);
        RP.actions[i] = (ink_ui_action_spec_t){.id = n->id, .label = n->label, .capability = "app.page_action", .args_json = RP.action_args[i],
            .enabled = !strcmp(n->kind, "input") || (n->capability[0] && declared(n->capability))};
        if (i < RP.offset || i >= RP.offset + 4) continue;
        int h = !strcmp(n->kind, "segments") ? INK_SEGMENT : !strcmp(n->kind, "slider") ? INK_SLIDER : INK_CONTROL;
        if (y + h > INK_CANVAS_H - INK_STATUS_BAR - 3 * INK_PRIMARY - 3 * INK_GAP) continue;
        lv_obj_t *obj = NULL;
        if (!strcmp(n->kind, "input")) {
            ink_text_cfg_t cfg = {.initial = n->value, .hint = n->label, .max = sizeof n->value - 1};
            obj = ink_textfield_create(RP.body, &cfg, entered, (void *)(intptr_t)i, false);
            lv_obj_set_pos(obj, 0, ink_ui_px(y)); lv_obj_set_size(obj, ink_ui_px(w), ink_ui_px(h));
            ink_textfield_bind_short(obj, &RP.spec, &RP.fields[i], &RP.actions[i]);
        } else if (!strcmp(n->kind, "toggle")) obj = ink_ui_toggle(RP.body, n->label, !strcmp(n->value, "true"), 0, y, w, control, (void *)(intptr_t)i);
        else if (!strcmp(n->kind, "slider")) obj = ink_ui_slider(RP.body, n->label, atoi(n->value), 0, y, w, control, (void *)(intptr_t)i);
        else if (!strcmp(n->kind, "segments")) {
            const char *labels[OPTIONS]; for (int k = 0; k < n->count; k++) labels[k] = n->options[k];
            obj = ink_ui_segments(RP.body, labels, n->count, atoi(n->value), 0, y, w, control, (void *)(intptr_t)i);
        } else if (!strcmp(n->kind, "button")) obj = ink_ui_pill(RP.body, n->label, 0, y, w, h, false, control, (void *)(intptr_t)i);
        else {
            obj = ink_ui_list_row(RP.body, "grid", n->label, n->value, 0, y, w, false, n->capability[0] ? control : NULL, (void *)(intptr_t)i);
        }
        if (n->capability[0] && !declared(n->capability)) lv_obj_add_state(obj, LV_STATE_DISABLED);
        RP.widgets[i] = obj; y += h + INK_GAP;
    }
    int a = RP.page.count;
    RP.actions[a++] = (ink_ui_action_spec_t){.id = "next", .label = "下一页", .capability = "app.page_turn", .args_json = "{\"direction\":1}",
        .enabled = RP.offset + 4 < RP.page.count || RP.index + 1 < RP.package->count};
    RP.actions[a++] = (ink_ui_action_spec_t){.id = "prev", .label = "上一页", .capability = "app.page_turn", .args_json = "{\"direction\":-1}",
        .enabled = RP.offset || RP.index};
    RP.actions[a++] = (ink_ui_action_spec_t){.id = "back", .label = "返回", .capability = "shell.back", .enabled = true};
    RP.actions[a++] = (ink_ui_action_spec_t){.id = "uninstall", .label = "卸载", .capability = "file.delete", .args_json = RP.uninstall,
        .enabled = ink_member_current()->role == INK_ROLE_ADULT};
    RP.fields[RP.page.count] = (ink_ui_field_spec_t){.id = "receipt", .label = "执行结果", .role = "status", .value = RP.message};
    RP.spec = (ink_ui_page_spec_t){.id = "AppPage", .title = RP.page.title, .object = {RP.package->id, 1},
        .fields = RP.fields, .field_count = (size_t)RP.page.count + 1, .actions = RP.actions, .action_count = (size_t)a};
    _Static_assert(NODES + 4 <= sizeof RP.actions / sizeof *RP.actions, "page action capacity");
    lv_label_set_text(RP.notice, RP.message); ink_ui_content_changed();
}
static int snapshot(char *out, size_t cap) {
    if (!RP.body || !ink_app_visible(&RP.package->def)) return -1;
    capture_inputs();
    return ink_ui_semantics_snapshot(&RP.spec, out, cap);
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg; char id[32]; ink_state_get_str(ink_state_find("app.foreground"), id, sizeof id);
    RP.package = NULL;
    for (int i = 0; i < PACKAGES; i++) if (PK.installed[i] && !strcmp(PK.installed[i]->id, id)) RP.package = PK.installed[i];
    if (!RP.package) return;
    RP.index = RP.offset = 0; RP.page = RP.package->pages[0]; RP.message[0] = 0;
    ink_shell_statusbar(screen, true); ink_shell_topbar(screen, RP.package->title);
    int y = INK_STATUS_BAR + INK_PRIMARY + INK_GAP, w = INK_CANVAS_W - 2 * INK_MARGIN;
    RP.body = ink_ui_box(screen, INK_MARGIN, y, w, INK_CANVAS_H - y - 2 * INK_PRIMARY, 0, INK_SURFACE_WHITE, false);
    RP.notice = ink_ui_line(screen, "", INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, INK_CANVAS_H - 2 * INK_PRIMARY, w);
    if (ink_member_current()->role == INK_ROLE_ADULT) {
        snprintf(RP.uninstall, sizeof RP.uninstall, "{\"path\":\"/apps/%s\"}", RP.package->id);
        ink_shell_bind(ink_ui_pill(screen, "卸载", INK_MARGIN, INK_CANVAS_H - INK_PRIMARY, w, INK_CONTROL, false, NULL, NULL), "file.delete", RP.uninstall);
    }
    ink_shell_bind(ink_ui_circle(screen, "back", INK_MARGIN, INK_CANVAS_H - 3 * INK_PRIMARY, INK_CONTROL, INK_ICON_MEDIUM,
        INK_SURFACE_WHITE, NULL, NULL), "app.page_turn", "{\"direction\":-1}");
    ink_shell_bind(ink_ui_circle(screen, "next", INK_CANVAS_W - INK_MARGIN - INK_CONTROL, INK_CANVAS_H - 3 * INK_PRIMARY,
        INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL), "app.page_turn", "{\"direction\":1}");
    draw();
}
static void destroy(void) { RP.body = RP.notice = NULL; memset(RP.widgets, 0, sizeof RP.widgets); RP.package = NULL; }
static void turn(ink_call_t *c) {
    int64_t direction = 1; ink_args_get_int(&c->args, "direction", &direction);
    capture_inputs();
    if (direction > 0 && RP.offset + 4 < RP.page.count) RP.offset += 4;
    else if (direction < 0 && RP.offset) RP.offset = 0;
    else {
        int next = RP.index + (direction > 0 ? 1 : -1);
        if (next < 0 || next >= RP.package->count) { invalid(c, "没有更多页面"); return; }
        RP.index = next; RP.offset = 0; RP.page = RP.package->pages[next];
    }
    draw();
}
static bool page_key(ink_key_t k, bool long_press) {
    (void)long_press;
    if (k != INK_KEY_NEXT && k != INK_KEY_PREV) return false;
    ink_args_t args; ink_args_init(&args); ink_args_int(&args, "direction", k == INK_KEY_NEXT ? 1 : -1);
    ink_call_opts_t opts = {.src = INK_SRC_KEY}; ink_cap_call("app.page_turn", &args, &opts); return true;
}
static void apply(void *arg) {
    (void)arg;
    bool changed = false;
    bool keep[PACKAGES] = {0};
    for (int i = 0; i < PACKAGES; i++) if (PK.staged[i]) for (int k = 0; k < PACKAGES; k++) {
        const package_t *a = PK.installed[k], *b = PK.staged[i];
        if (!a || strcmp(a->id, b->id) || strcmp(a->title, b->title) || strcmp(a->icon, b->icon) || strcmp(a->version, b->version) ||
            a->count != b->count || memcmp(a->permission, b->permission, sizeof a->permission) || memcmp(a->pages, b->pages, sizeof a->pages)) continue;
        keep[k] = true; osal_free(PK.staged[i]); PK.staged[i] = NULL; break;
    }
    for (int i = 0; i < PACKAGES; i++) if (PK.installed[i] && !keep[i]) {
        changed = true;
        ink_app_unregister(PK.installed[i]->id); osal_free(PK.installed[i]); PK.installed[i] = NULL;
    }
    for (int i = 0; i < PACKAGES; i++) if (PK.staged[i]) {
        package_t *bundle = PK.staged[i]; PK.staged[i] = NULL;
        bundle->activity = (ink_activity_t){.name = "AppPage", .app = bundle->id, .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = page_key};
        bundle->def = (ink_app_def_t){.id = bundle->id, .title = bundle->title, .icon = bundle->icon, .entry = &bundle->activity, .permissions = bundle->permissions, .child_ok = true};
        int slot = 0; while (slot < PACKAGES && PK.installed[slot]) slot++;
        if (slot == PACKAGES || !hal_fs_card_present() || ink_usb_disk_active() || ink_app_register(&bundle->def)) osal_free(bundle);
        else { PK.installed[slot] = bundle; changed = true; }
    }
    PK.scanning = false;
    if (changed) ink_app_card_changed(NULL);
    ink_probe_count("app.packages_scanned");
    if (changed && !strcmp(ink_ui_page_name(), "home")) ink_ui_redraw();
    if (PK.again) { PK.again = false; request_scan(); }
}
static void scan(void *arg) {
    (void)arg;
    if (!ink_fs_card_hold()) {
        char *buf = osal_malloc(JSON_BYTES + 1, OSAL_MEM_EXTERNAL);
        ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
        hal_dirent_t *entry = osal_malloc(sizeof *entry, OSAL_MEM_EXTERNAL);
        hal_dir_t *d = buf && j && entry ? hal_fs_opendir("/content/apps") : NULL;
        int n = 0;
        while (d && n < PACKAGES && hal_fs_readdir(d, entry) == 1) {
            if (!entry->is_dir || strlen(entry->name) >= 32 || !ink_layout_name_ok(entry->name)) continue;
            package_t *bundle = osal_calloc(1, sizeof *bundle, OSAL_MEM_EXTERNAL);
            if (bundle && load_package(bundle, entry->name, buf, j)) PK.staged[n++] = bundle;
            else { osal_free(bundle); ink_log(INK_LOG_WARN, "apps", "invalid package skipped"); ink_probe_count("app.package_invalid"); }
        }
        if (d) hal_fs_closedir(d);
        osal_free(buf); osal_free(j); osal_free(entry); ink_fs_card_release();
    }
    ink_exec_post(INK_EXEC_UI, &PK.apply, apply, NULL);
}
static void request_scan(void) {
    if (PK.scanning) { PK.again = true; return; }
    PK.scanning = true;
    if (ink_exec_post(INK_EXEC_BG, &PK.scan, scan, NULL)) PK.scanning = false;
}
static void storage(void *ctx, int h, const char *id) { (void)ctx; (void)h; (void)id; request_scan(); }
static void start_scan(void *arg) { (void)arg; request_scan(); }
static void changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; char path[INK_FS_PATH_MAX]; ink_args_get_str(args, "path", path, sizeof path);
    char kind[16]; ink_args_get_str(args, "kind", kind, sizeof kind);
    if (!strcmp(kind, "indexed")) return;
    if ((!strcmp(path, "/") && !strcmp(kind, "rescan")) || !strncmp(path, "/apps", 5) || !strncmp(path, "/family/apps", 12)) request_scan();
}
static const ink_cap_t CAPS[] = {
    {.id = "app.page_action", .title = "操作声明页的控件（输入框无值时聚焦）", .activity = "AppPage", .child_ok = true, .effect = INK_FX_NAVIGATE,
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":23},\"value\":{}},\"required\":[\"id\"],\"additionalProperties\":false}", .run = action},
    {.id = "app.page_turn", .title = "翻声明应用的页面", .activity = "AppPage", .child_ok = true, .effect = INK_FX_NAVIGATE,
     .input = "{\"type\":\"object\",\"properties\":{\"direction\":{\"type\":\"integer\",\"enum\":[-1,1]}},\"required\":[\"direction\"]}", .run = turn},
};
int ink_app_packages_init(void) {
    if (ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS) ||
        ink_state_watch("storage.card", storage, NULL, INK_EXEC_UI) || ink_state_watch("storage.mode", storage, NULL, INK_EXEC_UI) ||
        ink_cap_subscribe("file.changed", changed, NULL, INK_EXEC_UI)) return -1;
    return ink_exec_post(INK_EXEC_UI, &PK.apply, start_scan, NULL);
}
