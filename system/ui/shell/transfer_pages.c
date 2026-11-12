/* Transfer presenters use the same capabilities as AI. */
/* ui-scale-runtime: dimension x,y,w,font Theme-derived layout and typography. */
/* ui-scale-runtime: font font Only theme text sizes are passed below. */
#include "ink/sys/i18n.h"
#include <stdio.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/ui_semantics.h"
#include "ink/ui_shell.h"

enum { WIRELESS, LIVE, USB_BUSY, USB_ON, USB_FAIL };
static const char *const NAMES[] = {"Transfer", "TransferLive", "TransferUsbBusy", "TransferUsbOn", "TransferUsbFail"};
static struct {
    lv_obj_t *screen;
    lv_timer_t *timer;
    lv_obj_t *file_rows[2], *progress_label;
    unsigned files;
    char names[2][64], file_text[136], connection[24];
    uint32_t epoch, generation;
    int page;
    bool usb, active, pending, switching;
    int64_t received;
    char member[16], address[80], code[8], result[96], title[96], detail[256], progress[96];
    ink_ui_page_spec_t model;
    ink_ui_field_spec_t fields[8];
    ink_ui_action_spec_t actions[4];
} P INK_PSRAM_BSS;
static void create(lv_obj_t *screen, void *arg);
static void destroy(void);
static void draw(void);
static int snapshot(char *out, size_t cap) { return ink_ui_semantics_snapshot(&P.model, out, cap); }
#define PAGE(n_) {.name = n_, .app = "transfer", .create = create, .destroy = destroy, .snapshot = snapshot}
static const ink_activity_t PAGES[] = {PAGE("Transfer"), PAGE("TransferLive"), PAGE("TransferUsbBusy"), PAGE("TransferUsbOn"), PAGE("TransferUsbFail")};
static bool current(void) { return P.screen && P.epoch == ink_member_epoch() && !strcmp(ink_ui_page_name(), NAMES[P.page]); }
static void show(int page) {
    if (page != P.page) {
        P.switching = true;
        ink_ui_drop_app("transfer"); ink_ui_push(&PAGES[page], (void *)(intptr_t)page);
        P.switching = false;
    } else { draw(); ink_ui_content_changed(); }
}
static void reply(void *ctx, const ink_receipt_t *rc);
static void request(const char *cap, ink_source_t src) {
    ink_call_opts_t opts = {.src = src, .member = P.member, .reply_exec = INK_EXEC_UI,
        .done = reply, .ctx = (void *)(uintptr_t)P.generation};
    P.pending = true;
    if (ink_cap_call(cap, NULL, &opts)) P.pending = false;
}
static void poll(lv_timer_t *timer) {
    (void)timer;
    if (current() && !P.pending) request(P.usb ? "usb.status" : "transfer.status", INK_SRC_SYSTEM);
}
static void clicked(lv_event_t *e) {
    if (!current() || P.pending) return;
    const ink_ui_action_spec_t *a = lv_event_get_user_data(e);
    request(a->capability, INK_SRC_TOUCH);
}
static void button(const char *label, const char *cap, int x, int y, int w, bool primary) {
    ink_ui_action_spec_t *a = &P.actions[P.model.action_count++];
    *a = (ink_ui_action_spec_t){.id = cap, .label = label, .capability = cap, .enabled = true};
    ink_ui_pill(P.screen, label, x, y, w, INK_PRIMARY, primary, clicked, a);
}
static void text(const char *value, int font, int y) {
    lv_obj_t *o = ink_ui_label(P.screen, value, font, font == INK_FONT_NOTE ? INK_WEIGHT_REGULAR : INK_WEIGHT_BOLD,
        INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
}
static void received_rows(void) {
    for (unsigned i = 0; i < 2; i++) if (P.names[i][0]) {
        int y = INK_CANVAS_H / 2 + INK_LINE_NOTE + (int)i * INK_CONTROL;
        if (!P.file_rows[i]) {
            P.file_rows[i] = ink_ui_list_row(P.screen, "file", P.names[i], NULL,
                INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN, false, NULL, NULL);
            lv_obj_delete(lv_obj_get_child(P.file_rows[i], 0));
            ink_ui_group_divider(P.file_rows[i], INK_CONTROL - INK_STROKE, INK_CANVAS_W - 2 * INK_MARGIN);
        }
        else {
            lv_obj_t *label = lv_obj_get_child(P.file_rows[i], 0);
            while (label && !lv_obj_check_type(label, &lv_label_class)) label = lv_obj_get_child(P.file_rows[i], lv_obj_get_index(label) + 1);
            if (label && strcmp(lv_label_get_text(label), P.names[i])) lv_label_set_text(label, P.names[i]);
        }
    }
}
static void draw(void) {
    lv_obj_clean(P.screen); P.file_rows[0] = P.file_rows[1] = P.progress_label = NULL;
    const char *title = P.page == WIRELESS ? TR("从手机或电脑传入") : P.page == USB_FAIL ? TR("传文件失败") : TR("和电脑传文件");
    P.model = (ink_ui_page_spec_t){NAMES[P.page], title, {NAMES[P.page], P.epoch}, P.fields, 8, P.actions, 1};
    P.actions[0] = (ink_ui_action_spec_t){.id = "back", .label = TR("返回"), .capability = "shell.back", .enabled = true};
    P.fields[0] = (ink_ui_field_spec_t){.id = "member", .role = "text", .label = TR("家人"), .value = P.member};
    P.fields[1] = (ink_ui_field_spec_t){.id = "address", .role = "text", .label = TR("电脑地址"), .value = P.active ? (P.address[0] ? P.address : TR("还没连上 Wi-Fi")) : TR("尚未开启")};
    P.fields[2] = (ink_ui_field_spec_t){.id = "code", .role = "text", .label = TR("一次性数字"), .value = P.active ? P.code : "", .secret = true};
    P.fields[3] = (ink_ui_field_spec_t){.id = "progress", .role = "text", .label = TR("接收进度"), .value = P.progress};
    P.fields[4] = (ink_ui_field_spec_t){.id = "status", .role = "text", .label = TR("说明"), .value = P.detail};
    P.fields[5] = (ink_ui_field_spec_t){.id = "title", .role = "text", .label = TR("状态"), .value = P.title};
    P.fields[6] = (ink_ui_field_spec_t){.id = "connection", .role = "text", .label = TR("连接方式"), .value = P.connection};
    P.fields[7] = (ink_ui_field_spec_t){.id = "received_files", .role = "text", .label = TR("最近收到的文件"), .value = P.file_text};
    ink_shell_statusbar(P.screen, true);
    if (P.page != USB_ON) ink_shell_topbar(P.screen, title);
    int w = INK_CANVAS_W - 2 * INK_MARGIN, bottom = INK_CANVAS_H - INK_MARGIN - INK_PRIMARY;
    int middle = INK_CANVAS_H / 2, icon_y = middle - INK_CIRCLE_LARGE;
    if (P.page == WIRELESS) {
        text(TR("用手机扫，或在电脑上打开"), INK_FONT_LIST, INK_STATUS_BAR + INK_PRIMARY + INK_GAP);
        if (P.active && P.address[0]) {
            int edge = 2 * INK_CONTROL, y = INK_STATUS_BAR + INK_PRIMARY + INK_LINE_LIST + INK_GAP;
            lv_obj_t *qr = lv_qrcode_create(P.screen);
            lv_qrcode_set_size(qr, ink_ui_px(edge)); lv_qrcode_set_dark_color(qr, lv_color_black()); lv_qrcode_set_light_color(qr, lv_color_white());
            lv_obj_set_pos(qr, ink_ui_px((INK_CANVAS_W - edge) / 2), ink_ui_px(y));
            lv_obj_set_style_pad_all(qr, 0, 0); lv_obj_set_style_border_width(qr, 0, 0);
            lv_qrcode_update(qr, P.address, (uint32_t)strlen(P.address));
            text(P.address, INK_FONT_NOTE, y + edge + INK_GAP_L);
            if (strcmp(P.code, "000000")) {
                text(TR("再输这个数字"), INK_FONT_LIST, middle);
                text(P.code, INK_FONT_HEADING, middle + INK_LINE_LIST);
            }
            text(P.connection, INK_FONT_NOTE, y + edge + INK_GAP_L + INK_LINE_NOTE);
        } else if (P.active) text(TR("还没连上 Wi-Fi"), INK_FONT_LIST, middle);
        /* The note is one line in Chinese and two in English: it and the status above it move up a line when it
         * wraps, so the buttons never cover its end (i18n-shell-remaining, Transfer-en). */
        const char *note = !strcmp(P.result, TR("文件已收到")) ? TR("文件已保存，可在文件中打开。") : TR("可以管理当前家人的文件。关掉这页就断开。");
        const char *status = P.active || P.received > 0 ? P.progress : P.detail;
        int note_y = bottom - INK_PRIMARY - INK_GAP - ink_ui_text_height(note, INK_FONT_NOTE, INK_WEIGHT_REGULAR, w) - INK_GAP_S;
        (void)status;
        text(note, INK_FONT_NOTE, note_y);
        if (P.files || (P.active && !strcmp(P.code, "000000"))) {
            P.progress_label = ink_ui_line(P.screen, P.progress, INK_FONT_NOTE, INK_WEIGHT_REGULAR,
                INK_MARGIN, middle, w); received_rows();
        }
        button(TR("插电脑当 U 盘"), "transfer.usb", INK_MARGIN, bottom - INK_PRIMARY - INK_GAP, w, false);
        button(P.active ? TR("断开") : TR("开始"), P.active ? "transfer.stop" : "transfer.start", INK_MARGIN, bottom, (w - INK_GAP) / 2, !P.active);
        button(TR("关闭"), "shell.back", INK_MARGIN + (w + INK_GAP) / 2, bottom, (w - INK_GAP) / 2, false);
    } else {
        ink_ui_icon(P.screen, P.page == LIVE ? "download" : P.page == USB_ON ? "usb" : "sd",
            INK_ICON_STATE, INK_ICON_LINE, (INK_CANVAS_W - INK_ICON_STATE) / 2, icon_y, false);
        text(P.title, INK_FONT_SUBTITLE, middle + INK_GAP_XL);
        text(P.detail, INK_FONT_NOTE, middle + INK_GAP_XL + INK_LINE_SUBTITLE + INK_GAP);
        if (P.page == LIVE) {
            text(P.progress, INK_FONT_LIST, bottom - INK_PRIMARY - INK_GAP_L);
            button(TR("断开"), "transfer.stop", INK_MARGIN, bottom, (w - INK_GAP) / 2, false);
            button(TR("关闭"), "shell.back", INK_MARGIN + (w + INK_GAP) / 2, bottom, (w - INK_GAP) / 2, false);
        } else if (P.page == USB_BUSY) {
            button(TR("返回先停下"), "shell.back", INK_MARGIN, bottom, (w - INK_GAP) / 2, false);
            button(TR("开始连接"), "usb.mount", INK_MARGIN + (w + INK_GAP) / 2, bottom, (w - INK_GAP) / 2, true);
        } else button(P.page == USB_ON ? TR("结束传输") : TR("知道了"), P.page == USB_ON ? "usb.unmount" : "shell.back", INK_MARGIN, bottom, w, true);
    }
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    if (!current() || (uint32_t)(uintptr_t)ctx != P.generation) return;
    P.pending = false;
    if (rc->status == INK_RC_PENDING_CONFIRMATION || rc->status == INK_RC_CANCELLED) return;
    if (rc->status != INK_RC_DONE) {
        snprintf(P.title, sizeof P.title, "%s", !strcmp(rc->detail, "busy") ? TR("有应用在用存储卡") : TR("还不能开始传文件"));
        snprintf(P.detail, sizeof P.detail, "%s", !strcmp(rc->detail, "busy") ? TR("不能自动停下应用。请返回停下读书、播放或下载，再来连接。") : TR("请检查无线网络、数据线和存储卡后再试。"));
        if (P.usb && (!strcmp(rc->detail, "no card") || !strcmp(rc->detail, "read or write error"))) {
            show(USB_FAIL);
            if (!strcmp(rc->detail, "no card")) {
                snprintf(P.title, sizeof P.title, "%s", TR("请先插入存储卡"));
                snprintf(P.detail, sizeof P.detail, "%s", TR("没有检测到存储卡。插好后重新连接，不会自动格式化。")); draw();
            }
        }
        else show(P.usb ? USB_BUSY : WIRELESS);
        return;
    }
    if (!rc->result) { poll(NULL); return; }
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    if (!j || ink_json_parse(j, rc->result, strlen(rc->result))) { osal_free(j); return; }
    if (P.usb) {
        bool lent = false, connected = false;
        lent = ink_json_eq(j, ink_json_find(j, 0, "lent"), "true"); connected = ink_json_eq(j, ink_json_find(j, 0, "connected"), "true");
        if (lent && P.page != USB_ON) show(USB_ON);
        else if (!connected && P.page == USB_BUSY && strcmp(P.title, TR("请先连上电脑"))) {
            snprintf(P.title, sizeof P.title, TR("请先连上电脑")); show(USB_BUSY);
        }
    } else {
        bool active = false; int64_t port = 0, received = 0; char result[96] = "", code[8] = "";
        active = ink_json_eq(j, ink_json_find(j, 0, "active"), "true"); ink_json_int(j, ink_json_find(j, 0, "port"), &port);
        ink_json_int(j, ink_json_find(j, 0, "received"), &received);
        ink_json_str(j, ink_json_find(j, 0, "result"), result, sizeof result); ink_json_str(j, ink_json_find(j, 0, "code"), code, sizeof code);
        char host[64] = "", address[80] = "";
        ink_json_str(j, ink_json_find(j, 0, "host"), host, sizeof host);
        if (host[0]) snprintf(address, sizeof address, "http://%s:%u/", host, (unsigned)(uint16_t)port);
        int64_t files = 0; char last[64] = "", connection[16] = "";
        ink_json_int(j, ink_json_find(j, 0, "files"), &files);
        if (!files && !strcmp(result, TR("文件已收到"))) files = 1;
        ink_json_str(j, ink_json_find(j, 0, "last_file"), last, sizeof last);
        ink_json_str(j, ink_json_find(j, 0, "connection"), connection, sizeof connection);
        bool row_only = P.page == WIRELESS && (P.files || !strcmp(P.code, "000000")) && active == P.active && !strcmp(P.code, code) && !strcmp(P.address, address);
        if (files > P.files && last[0]) snprintf(P.names[((unsigned)files - 1) % 2], sizeof P.names[0], "%s", last);
        bool file_change = P.files != (unsigned)files; P.files = (unsigned)files;
        snprintf(P.file_text, sizeof P.file_text, "%s%s%s", P.names[0], P.names[1][0] ? "\n" : "", P.names[1]);
        snprintf(P.connection, sizeof P.connection, "%s", !strcmp(connection, "hotspot") ? TR("设备热点") : "Wi-Fi");
        bool changed = file_change || active != P.active || received != P.received || strcmp(P.result, result) || strcmp(P.code, code) || strcmp(P.address, address);
        P.active = active; P.received = received;
        snprintf(P.result, sizeof P.result, "%s", result); snprintf(P.code, sizeof P.code, "%s", code);
        snprintf(P.address, sizeof P.address, "%s", address);
        snprintf(P.progress, sizeof P.progress, active ? TR("接收中，%lld 字节；尚未保存") : !strcmp(result, TR("文件已收到")) ? TR("已收到 1 个，%lld 字节") : TR("已接收 %lld 字节；未确认保存"), (long long)received);
        if (P.files) snprintf(P.progress, sizeof P.progress, TR("已收到 %u 个，%lld 字节"), P.files, (long long)received);
        if (active && !received) snprintf(P.progress, sizeof P.progress, "%s", TR("等待传入"));
        snprintf(P.title, sizeof P.title, "%.90s", active ? received > 0 ? TR("正在接收文件") : TR("等待传入") : result);
        snprintf(P.detail, sizeof P.detail, "%s", active ? TR("接收完成后保存到‘传进来的’。现在还不能打开。") : !strcmp(result, TR("文件已收到")) ? TR("文件已保存，可在文件中打开。") : result);
        if (changed) {
            if (row_only) {
                if (P.progress_label) lv_label_set_text(P.progress_label, P.progress);
                received_rows(); ink_ui_content_changed();
            } else show(active && received > 0 && !P.files ? LIVE : WIRELESS);
        }
    }
    osal_free(j);
}
static void create(lv_obj_t *screen, void *arg) {
    P.screen = screen; P.page = (int)(intptr_t)arg; P.epoch = ink_member_epoch(); P.generation++; P.pending = false;
    snprintf(P.member, sizeof P.member, "%s", ink_member_current()->id);
    if (P.page == USB_ON) {
        snprintf(P.title, sizeof P.title, TR("存储卡已经连到电脑"));
        snprintf(P.detail, sizeof P.detail, TR("这期间书和音频打不开。用完在电脑上弹出，或者点‘结束’。"));
    } else if (P.page == USB_FAIL) {
        snprintf(P.title, sizeof P.title, TR("读不了这张存储卡"));
        snprintf(P.detail, sizeof P.detail, TR("不会自动格式化。\n请到电脑上修复，再插回来。"));
    }
    draw(); P.timer = lv_timer_create(poll, 1000, NULL); poll(NULL);
}
static void destroy(void) {
    if (P.timer) lv_timer_delete(P.timer);
    P.timer = NULL; P.screen = NULL; P.generation++;
    if (!P.usb && !P.switching) {
        ink_call_opts_t opts = {.src = INK_SRC_SYSTEM, .member = P.member};
        ink_cap_call("transfer.stop", NULL, &opts);
    }
}
static void open_page(ink_call_t *c) {
    ink_shell_close(); ink_ui_drop_app("transfer");
    P.usb = !strcmp(c->cap->id, "transfer.usb"); P.active = false; P.received = 0; P.files = 0;
    memset(P.names, 0, sizeof P.names); P.file_text[0] = 0;
    P.result[0] = P.code[0] = P.progress[0] = 0;
    snprintf(P.title, sizeof P.title, TR("准备连电脑"));
    snprintf(P.detail, sizeof P.detail, "%s", P.usb ? TR("开始前请先停下使用存储卡的应用。占用的应用暂不能查看。") : TR("可以管理当前家人的文件。关掉这页就断开。"));
    ink_ui_push(&PAGES[P.usb ? USB_BUSY : WIRELESS], (void *)(intptr_t)(P.usb ? USB_BUSY : WIRELESS));
}
static void confirmed(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id;
    char cap[48] = "", status[24] = "", member[16] = "";
    ink_args_get_str(a, "cap", cap, sizeof cap); ink_args_get_str(a, "status", status, sizeof status);
    ink_args_get_str(a, "member", member, sizeof member);
    if (!current() || !P.usb || strcmp(cap, "usb.mount") || strcmp(member, P.member)) return;
    if (!strcmp(status, "failed")) {
        ink_receipt_t rc = {.status = INK_RC_FAILED};
        ink_args_get_str(a, "detail", rc.detail, sizeof rc.detail);
        reply((void *)(uintptr_t)P.generation, &rc);
    } else poll(NULL);
}
static void usb_changed(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx; (void)id;
    bool lent = true; ink_args_get_bool(a, "lent", &lent);
    if (!current() || !P.usb || P.page != USB_ON || lent) return;
    int64_t card = 0; ink_state_get_int(ink_state_find("storage.card"), &card);
    if (card) ink_ui_pop(); else show(USB_FAIL);
}
int ink_transfer_pages_init(void) {
    static const ink_cap_t caps[] = {
        {.id = "transfer.open", .title = "打开从手机或电脑传入页面", .effect = INK_FX_NAVIGATE, .run = open_page},
        {.id = "transfer.usb", .title = "打开连电脑页面", .effect = INK_FX_NAVIGATE, .run = open_page}
    };
    return ink_cap_register(caps, 2) || ink_cap_subscribe("confirm.result", confirmed, NULL, INK_EXEC_UI) ||
        ink_cap_subscribe("usb.changed", usb_changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
