/* One requested frame, decoded on MEDIA; only the redacted result reaches UI/AI. */
#include <stdio.h>
#include <string.h>
#include "quirc.h"
#include "ink/osal.h"
#include "ink/sys/appreg.h"
#include "ink/sys/camera.h"
#include "ink/sys/i18n.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"

typedef struct {
    ink_exec_job_t job; ink_call_t *call;
    uint32_t epoch, generation;
    ink_args_t args;
    char type[12], content[301], copy_json[640];
    const char *target, *label;
    bool ok;
} scan_t;
static scan_t *result;
static lv_obj_t *scan_root;
static int scanning;
static const ink_activity_t SCAN;
static void discard(scan_t *p) { if (p) { memset(p, 0, sizeof *p); osal_free(p); } }
/* Escaped delimiters in MECARD/WIFI and newline-separated vCard fields. */
static bool field(const char *s, const char *key, char delim, char *out, size_t cap) {
    size_t k = strlen(key);
    while (*s) {
        bool match = !strncmp(s, key, k); size_t n = 0;
        if (match) s += k;
        while (*s && *s != delim && *s != '\r') {
            char c = *s++;
            if (c == '\\' && *s) { c = *s++; if (c == 'n') c = '\n'; }
            if (match) { if (n + 1 >= cap) return false; out[n++] = c; }
        }
        if (match) { out[n] = 0; return true; }
        if (*s) s++;
    }
    return false;
}
static bool parse(scan_t *p, const char *text) {
    char name[97] = "", number[25] = "", pass[65] = "", security[16] = "";
    ink_args_init(&p->args);
    if (!strncmp(text, "WIFI:", 5)) {
        strcpy(p->type, "wifi");
        bool valid = field(text + 5, "S:", ';', name, 33) && name[0] && field(text + 5, "T:", ';', security, sizeof security);
        bool open = !strcmp(security, "nopass");
        valid = valid && (open || !strcmp(security, "WPA") || !strcmp(security, "WEP"));
        if (!open) valid = valid && field(text + 5, "P:", ';', pass, sizeof pass);
        if (valid) { ink_args_str(&p->args, "ssid", name); ink_args_str(&p->args, "password", pass); }
        memset(pass, 0, sizeof pass);
        if (!valid) return false;
        snprintf(p->content, sizeof p->content, "%s", name);
        p->target = "wifi.connect"; p->label = "连接 Wi-Fi";
    } else if (!strncmp(text, "MECARD:", 7) || !strncmp(text, "BEGIN:VCARD", 11)) {
        strcpy(p->type, "contact"); bool me = text[0] == 'M';
        if (!field(me ? text + 7 : text, me ? "N:" : "FN:", me ? ';' : '\n', name, sizeof name) || !name[0] ||
            !field(me ? text + 7 : text, "TEL:", me ? ';' : '\n', number, sizeof number) || !number[0]) return false;
        ink_args_str(&p->args, "name", name); ink_args_str(&p->args, "number", number);
        snprintf(p->content, sizeof p->content, "%s\n%s", name, number);
        p->target = "contacts.put"; p->label = "保存联系人";
    } else {
        size_t n = strlen(text);
        if (n >= sizeof p->content) return false;
        memcpy(p->content, text, n + 1);
        if (!strncmp(text, "tel:", 4)) {
            if (!text[4] || strlen(text + 4) > 20) return false;
            for (const char *s = text + 4; *s; s++) if (!strchr("+0123456789*#", *s)) return false;
            strcpy(p->type, "tel"); ink_args_str(&p->args, "number", text + 4);
            p->target = "call.dial"; p->label = "拨打电话";
        } else {
            strcpy(p->type, !strncmp(text, "https://", 8) || !strncmp(text, "http://", 7) ? "url" : "text");
            ink_jw_t w; ink_jw_init(&w, p->copy_json, sizeof p->copy_json);
            ink_jw_raw(&w, "{\"text\":"); ink_jw_str(&w, text); ink_jw_raw(&w, "}");
            if (w.overflow) return false;
        }
    }
    return true;
}
static void create(lv_obj_t *root, void *arg) {
    (void)arg; scan_root = root; ink_shell_statusbar(root, true); ink_shell_topbar(root, TR("扫码"));
    const char *message = result ? (result->ok ? result->content : TR("没有识别到可用的二维码，请对准后重试")) : TR("正在识别二维码");
    int top = INK_STATUS_BAR + 2 * INK_PRIMARY, width = INK_CANVAS_W - 2 * INK_MARGIN;
    lv_obj_t *body = ink_ui_box(root, INK_MARGIN, top, width, INK_CANVAS_H - top - 4 * INK_PRIMARY - INK_GAP, 0, INK_SURFACE_WHITE, false);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE); lv_obj_set_scroll_dir(body, LV_DIR_VER);
    ink_ui_label(body, message, INK_FONT_BODY, INK_WEIGHT_REGULAR, 0, 0, width);
    if (result && result->ok) {
        const char *hint = result->target ? TR("是否执行下面的操作？") : !strcmp(result->type, "url") ? TR("设备没有浏览器，可以复制网址") : TR("可以复制文字");
        ink_ui_label(root, hint, INK_FONT_BODY, INK_WEIGHT_REGULAR, INK_MARGIN, INK_CANVAS_H - 4 * INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN);
        lv_obj_t *b = ink_ui_pill(root, TR(result->target ? result->label : "复制"), INK_MARGIN, INK_CANVAS_H - 3 * INK_PRIMARY, INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, true, NULL, NULL);
        ink_shell_bind(b, result->target ? "camera.scan_apply" : "clipboard.write", result->target ? NULL : result->copy_json);
    }
    lv_obj_t *b = ink_ui_pill(root, TR("再扫一次"), INK_MARGIN, INK_CANVAS_H - INK_PRIMARY - INK_MARGIN, INK_CANVAS_W - 2 * INK_MARGIN, INK_PRIMARY, false, NULL, NULL);
    ink_shell_bind(b, "camera.scan", NULL);
}
static int snapshot(char *out, size_t cap) {
    ink_ui_field_spec_t fields[] = {{.id = "type", .role = "text", .label = TR("类型"), .value = result && result->ok ? result->type : "none"},
        {.id = "content", .role = "text", .label = TR("内容"), .value = result && result->ok ? result->content : TR("没有识别到可用的二维码，请对准后重试")}};
    ink_ui_action_spec_t actions[] = {{.id = "scan", .label = TR("再扫一次"), .capability = "camera.scan", .enabled = true},
        {.id = "apply", .label = TR(result && result->target ? result->label : "复制"), .capability = result && result->target ? "camera.scan_apply" : "clipboard.write",
         .args_json = result && !result->target ? result->copy_json : NULL, .enabled = result && result->ok}};
    ink_ui_page_spec_t page = {.id = "Scan", .title = TR("扫码"), .object = {"camera:scan", 1}, .fields = fields, .field_count = 2, .actions = actions, .action_count = 2};
    return ink_ui_semantics_snapshot(&page, out, cap);
}
static void destroy(void) { discard(result); result = NULL; scan_root = NULL; }
static const ink_activity_t SCAN = {.name = "Scan", .app = "camera", .create = create, .destroy = destroy, .snapshot = snapshot};
static void finish(void *arg) {
    scan_t *p = arg; ink_call_t *c = p->call; p->call = NULL;
    __atomic_store_n(&scanning, 0, __ATOMIC_RELEASE);
    if (p->epoch != ink_member_epoch() || p->generation != ink_ui_generation() || strcmp(ink_ui_page_name(), "Scan")) {
        c->rc.status = INK_RC_CANCELLED; discard(p);
    } else {
        result = p;
        char json[1024]; ink_jw_t w; ink_jw_init(&w, json, sizeof json);
        ink_jw_raw(&w, "{\"type\":"); ink_jw_str(&w, p->ok ? p->type : "none");
        ink_jw_raw(&w, ",\"content\":"); ink_jw_str(&w, p->ok ? p->content : ""); ink_jw_raw(&w, "}");
        ink_cap_result(c, json); c->rc.status = p->ok ? INK_RC_DONE : INK_RC_FAILED;
        snprintf(c->rc.say, sizeof c->rc.say, "%s", TR(p->ok ? "已识别，请查看扫码结果" : "没有识别到可用的二维码，请对准后重试"));
        lv_obj_clean(scan_root); create(scan_root, NULL); ink_ui_content_changed(); ink_ui_hint_obj(scan_root, INK_REFRESH_UI);
    }
    ink_cap_done(c);
}
static void decode(void *arg) {
    scan_t *p = arg;
    int w = 0, h = 0; uint8_t *rgb = ink_camera_read_rgb(&w, &h);
    struct quirc *q = rgb ? quirc_new() : NULL;
    struct { struct quirc_code code; struct quirc_data data; } *decoded = osal_calloc(1, sizeof *decoded, OSAL_MEM_EXTERNAL);
    if (q && decoded && !quirc_resize(q, w, h)) {
        uint8_t *gray = quirc_begin(q, NULL, NULL);
        for (size_t i = 0; i < (size_t)w * h; i++) gray[i] = (77u * rgb[3*i] + 150u * rgb[3*i+1] + 29u * rgb[3*i+2] + 128u) >> 8;
        osal_free(rgb); rgb = NULL; quirc_end(q);
        for (int i = 0; i < quirc_count(q) && !p->ok; i++) {
            quirc_extract(q, i, &decoded->code);
            if (quirc_decode(&decoded->code, &decoded->data)) continue;
            int n = decoded->data.payload_len;
            if (n > 0 && n <= 300 && !memchr(decoded->data.payload, 0, (size_t)n)) p->ok = parse(p, (const char *)decoded->data.payload);
        }
    }
    osal_free(rgb);
    if (q) quirc_destroy(q);
    if (decoded) { memset(decoded, 0, sizeof *decoded); osal_free(decoded); }
    if (ink_exec_post(INK_EXEC_UI, &p->job, finish, p)) {
        ink_call_t *c = p->call; c->rc.status = INK_RC_FAILED; discard(p); __atomic_store_n(&scanning, 0, __ATOMIC_RELEASE); ink_cap_done(c);
    }
}
static void scan(ink_call_t *c) {
    if (__atomic_load_n(&scanning, __ATOMIC_ACQUIRE)) { c->rc.status = INK_RC_REJECTED; return; }
    scan_t *p = osal_calloc(1, sizeof *p, OSAL_MEM_EXTERNAL);
    if (!p) { c->rc.status = INK_RC_FAILED; return; }
    if (!strcmp(ink_ui_page_name(), "Scan")) { destroy(); ink_ui_redraw(); }
    else ink_ui_push(&SCAN, NULL);
    p->epoch = ink_member_epoch(); p->generation = ink_ui_generation(); p->call = c;
    __atomic_store_n(&scanning, 1, __ATOMIC_RELEASE); ink_cap_defer(c);
    if (ink_exec_post(INK_EXEC_MEDIA, &p->job, decode, p)) { __atomic_store_n(&scanning, 0, __ATOMIC_RELEASE); c->rc.status = INK_RC_FAILED; discard(p); ink_cap_done(c); }
}
static ink_rc_t prepare(ink_args_t *args, ink_receipt_t *rc) {
    (void)rc;
    if (!result || !result->ok || !result->target) return INK_RC_INVALID;
    ink_args_init(args); ink_args_str(args, "content", result->content); return INK_RC_DONE;
}
static void applied(void *ctx, const ink_receipt_t *rc) {
    ink_call_t *c = ctx; c->rc = *rc;
    if (rc->result) ink_cap_result(c, rc->result);
    ink_cap_done(c);
}
static void apply(ink_call_t *c) {
    if (!result || !result->ok || !result->target || result->epoch != ink_member_epoch()) { c->rc.status = INK_RC_STALE; return; }
    ink_cap_defer(c);
    ink_call_opts_t o = {.src = c->src, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_UI, .done = applied, .ctx = c};
    if (ink_cap_call(result->target, &result->args, &o)) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
}
int ink_camera_scan_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "camera.scan", .app = "camera", .title = "打开扫码，返回类型和内容（不含 Wi-Fi 密码）", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .permission = "camera", .timeout_ms = 15000, .run = scan},
        {.id = "camera.scan_apply", .app = "camera", .title = "执行扫码结果中的操作", .ask = "执行扫码操作：{content}", .effect = INK_FX_EXTERNAL, .child_ok = true, .activity = "Scan", .ask_by_hand = true, .prepare_confirm = prepare, .run = apply}
    };
    return ink_cap_find("camera.capture") ? ink_cap_register(caps, 2) : 0;
}
