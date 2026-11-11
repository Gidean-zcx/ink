/* Enrollment presenters; services own network, identity and restore state. */
/* ui-scale-runtime: dimension x,y,w,h Theme-derived shared layout coordinates. */
/* ui-scale-runtime: font font Callers use the subtitle, heading and note theme sizes. */
/* ui-scale-runtime: dimension B.wifi_page Page count times the theme-sized Wi-Fi row height. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/cap.h"
#include "ink/sys/data.h"
#include "ink/sys/i18n.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/pairing.h"
#include "ink/sys/state.h"
#include "ink/sys/textfield.h"
#include "ink/sys/time.h"
#include "ink/ui_shell.h"

enum { FIRST, WIFI, PASSWORD, JOINING, JOIN_FAIL, CLAIM, NO_HUB, CONFIRM, RESTORE, HELLO, REVOKED };
enum { PAIR_REPLY, SCAN_REPLY, JOIN_REPLY, HOTSPOT_REPLY, RESTORE_REPLY };
enum { BOOT_WIFI_MAX = 12 };
static const char *const TITLES[] = {TR_NOOP("你好"), TR_NOOP("选一个 Wi-Fi"), TR_NOOP("网络密码"), TR_NOOP("正在连接网络"), TR_NOOP("没能连接网络"),
    TR_NOOP("用手机扫一下，加入家庭"), TR_NOOP("找不到家庭服务器"), TR_NOOP("加入这个家庭？"), TR_NOOP("备份和恢复"), TR_NOOP("不懂就问"), TR_NOOP("这台设备不在家庭里了")};
static struct {
    int page, count, wifi_page;
    int resume;
    bool computer;
    bool secured[BOOT_WIFI_MAX];
    ink_db_t *progress;
    uint32_t epoch;
    lv_obj_t *screen, *field, *qr;
    char ssid[BOOT_WIFI_MAX][33], picked[33], claim[256], txid[40], family[64], by[64], check[24], preview[33], message[192];
    ink_ui_field_spec_t fields[5];
    ink_ui_action_spec_t actions[8];
    char ids[8][32], args[8][192];
    ink_ui_page_spec_t model;
    ink_exec_job_t focus;
} B INK_PSRAM_BSS;
static const ink_db_def_t PROGRESS = {.name = "boot.progress", .title = "开机设置进度", .scope = INK_DB_DEVICE,
    .schema = 1, .json = true, .max_records = 1};
static void remember(int page) {
    if (!B.progress) return;
    char saved[256]; ink_jw_t w; ink_jw_init(&w, saved, sizeof saved);
    ink_jw_raw(&w, "{\"network\":"); ink_jw_str(&w, B.picked); ink_jw_raw(&w, "}");
    ink_rec_t old; char previous[256];
    if (ink_db_get(B.progress, 1, &old, previous, sizeof previous) == INK_DB_OK && old.sort == page &&
        old.len == strlen(saved) && !memcmp(previous, saved, old.len)) return;
    ink_db_write_t rec = {.id = 1, .sort = page, .data = saved, .len = strlen(saved), .src = INK_SRC_SYSTEM};
    if (ink_db_put(B.progress, &rec, NULL) != INK_DB_OK) snprintf(B.message, sizeof B.message, TR("设置进度还没保存，请稍后再试。"));
}
static void create(lv_obj_t *screen, void *arg);
static void destroy(void) { if (B.page == HELLO) remember(-1); ink_text_blur(0); B.screen = B.field = B.qr = NULL; }
static int snapshot(char *out, size_t cap) {
    return B.field ? ink_textfield_short_snapshot(B.field, out, cap) : ink_ui_semantics_snapshot(&B.model, out, cap);
}
static bool key(ink_key_t code, bool long_press);
#define PAGE(name_) {.name = name_, .app = "boot", .create = create, .destroy = destroy, .snapshot = snapshot, .on_key = key}
static const ink_activity_t PAGES[] = {PAGE("FirstBoot"), PAGE("BootWifi"), PAGE("BootWifiPass"), PAGE("BootJoining"),
    PAGE("BootJoinFail"), PAGE("BootClaim"), PAGE("BootNoHub"), PAGE("BootConfirm"), PAGE("BootRestore"),
    PAGE("BootHello"), PAGE("BootRevoked")};
_Static_assert(sizeof TITLES / sizeof *TITLES == sizeof PAGES / sizeof *PAGES, "boot page titles");
_Static_assert(2 + 3 <= sizeof B.fields / sizeof *B.fields, "boot Wi-Fi field capacity");
static bool current(void) { return B.screen && B.epoch == ink_member_epoch() && ink_ui_page_current() &&
    !strcmp(ink_ui_page_name(), PAGES[B.page].name); }
static void show(int page) {
    if (page != CLAIM) B.computer = false;
    ink_ui_drop_app("boot");
    ink_ui_push(&PAGES[page], NULL);
}
static void reply(void *ctx, const ink_receipt_t *rc);
static void request(const char *id, const ink_args_t *args, int kind, ink_source_t source) {
    ink_call_opts_t o = {.src = source, .reply_exec = INK_EXEC_UI, .done = reply,
        .ctx = (void *)(((uintptr_t)ink_member_epoch() << 4) | (unsigned)kind)};
    ink_cap_call(id, args, &o);
}
/* The page needs the date and the count, not the item list, which outgrows a fixed-size JSON reader. */
static const ink_args_t *summary(void) {
    static ink_args_t a;
    ink_args_init(&a);
    ink_args_bool(&a, "summary", true);
    return &a;
}
static void text(const char *s, int font, int y) {
    lv_obj_t *label = ink_ui_label(B.screen, s, font, font == INK_FONT_NOTE ? INK_WEIGHT_REGULAR : INK_WEIGHT_BOLD,
        INK_MARGIN, y, INK_CANVAS_W - 2 * INK_MARGIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}
static void clicked(lv_event_t *e) {
    const ink_ui_action_spec_t *a = lv_event_get_user_data(e);
    ink_ui_action_target_t target = {ink_ui_content_version(), B.model.object};
    ink_receipt_t rc;
    ink_ui_semantics_action(&B.model, a->id, &target, INK_SRC_TOUCH, &rc);
}
static const ink_ui_action_spec_t *action(const char *label, const char *cap, const char *value) {
    size_t i = B.model.action_count++;
    ink_jw_t w; ink_jw_init(&w, B.args[i], sizeof B.args[i]);
    ink_jw_raw(&w, "{"); ink_jw_str(&w, !strcmp(cap, "pairing.accept") || !strcmp(cap, "pairing.reject") ? "txid" :
        !strcmp(cap, "restore.apply") ? "preview" : "id");
    ink_jw_raw(&w, ":"); ink_jw_str(&w, value); ink_jw_raw(&w, "}");
    snprintf(B.ids[i], sizeof B.ids[i], "%s", !strcmp(cap, "shell.launch") ? value : cap);
    B.actions[i] = (ink_ui_action_spec_t){B.ids[i], label, cap, B.args[i], {NULL, 0}, true};
    return &B.actions[i];
}
static void button(const char *label, const char *cap, const char *value, int x, int y, int w, bool primary) {
    const ink_ui_action_spec_t *a = action(label, cap, value);
    ink_ui_pill(B.screen, label, x, y, w, INK_CONTROL, primary, clicked, (void *)a);
}
static void bottom(const char *label, const char *cap, const char *value, bool primary) {
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    const ink_ui_action_spec_t *a = action(label, cap, value);
    ink_ui_pill(B.screen, label, primary ? INK_MARGIN + (w + INK_GAP) / 2 : INK_MARGIN,
        INK_CANVAS_H - INK_BLOCK - INK_PRIMARY, (w - INK_GAP) / 2, INK_PRIMARY, primary, clicked, (void *)a);
}
static void qr(const char *value) {
    if (!B.qr) {
        int size = 3 * INK_PRIMARY + 2 * INK_MARGIN;
        B.qr = lv_qrcode_create(B.screen);
        lv_obj_remove_style_all(B.qr);
        lv_qrcode_set_size(B.qr, ink_ui_px(size));
        lv_qrcode_set_dark_color(B.qr, ink_ui_ink()); lv_qrcode_set_light_color(B.qr, ink_ui_paper());
        lv_obj_set_pos(B.qr, ink_ui_px((INK_CANVAS_W - size) / 2), ink_ui_px(2 * INK_PRIMARY + INK_GAP));
    }
    lv_qrcode_update(B.qr, value, (uint32_t)strlen(value));
}
static void password_entered(void *ctx, const char *value) {
    (void)ctx;
    if (!current()) return;
    if (!B.picked[0]) { snprintf(B.picked, sizeof B.picked, "%s", value); show(PASSWORD); return; }
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    if (!args) return;
    ink_args_init(args); ink_args_str(args, "ssid", B.picked); ink_args_str(args, "password", value);
    request("wifi.connect", args, JOIN_REPLY, INK_SRC_TOUCH);
    memset(args, 0, sizeof *args); osal_free(args);
    snprintf(B.message, sizeof B.message, TR("正在连接 %s"), B.picked);
    show(JOINING);
}
static void focus_password(void *ctx) {
    (void)ctx;
    if (current() && B.page == PASSWORD) ink_textfield_short_focus(B.field);
}
static void create(lv_obj_t *screen, void *arg) {
    (void)arg; B.screen = screen; B.epoch = ink_member_epoch();
    for (B.page = 0; B.page < REVOKED; B.page++) if (!strcmp(ink_ui_page_name(), PAGES[B.page].name)) break;
    remember(B.page);
    memset(B.fields, 0, sizeof B.fields);
    B.model = (ink_ui_page_spec_t){PAGES[B.page].name, TR(TITLES[B.page]), {PAGES[B.page].name, 1}, B.fields, 2, B.actions, 0};
    B.fields[0] = (ink_ui_field_spec_t){"message", "text", TR("说明"), B.message, {NULL, 0}, B.page == CLAIM};
    B.fields[1] = (ink_ui_field_spec_t){"network", "text", TR("网络"), B.picked, {NULL, 0}, false};
    ink_shell_statusbar(screen, B.page == HELLO || B.page == REVOKED);
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    if (B.page == JOIN_FAIL || B.page == NO_HUB || B.page == RESTORE || B.page == REVOKED)
        ink_ui_circle(screen, B.page == JOIN_FAIL ? "wifioff" : B.page == NO_HUB ? "cloudoff" : B.page == RESTORE ? "download" : "family",
            (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_CANVAS_H / 2 - INK_CIRCLE_LARGE - INK_GAP,
            INK_CIRCLE_LARGE, INK_ICON_STATE, INK_SURFACE_STATIC, NULL, NULL);
    if (B.page == WIFI || B.page == PASSWORD) {
        button(TR("返回"), "shell.launch", "boot.back", INK_MARGIN, INK_STATUS_BAR, INK_CONTROL, false);
        ink_ui_line(screen, B.page == WIFI ? TR(TITLES[WIFI]) : B.picked[0] ? B.picked : TR("其他网络"), INK_FONT_SUBTITLE, INK_WEIGHT_BOLD,
            2 * INK_MARGIN + INK_CONTROL, INK_STATUS_BAR + INK_GAP, w - INK_CONTROL - INK_MARGIN);
    } else text(B.page == CLAIM && B.computer ? TR("用电脑加入家庭") : TR(TITLES[B.page]), B.page == FIRST ? INK_FONT_HEADING : INK_FONT_SUBTITLE,
        B.page == FIRST || B.page == CLAIM ? INK_PRIMARY : INK_CANVAS_H / 2);
    if (B.page == FIRST) {
        text(TR("用手机设置这台设备"), INK_FONT_LIST, INK_PRIMARY + INK_LINE_HEADING);
        request("pairing.hotspot_start", NULL, HOTSPOT_REPLY, INK_SRC_SYSTEM);
        text(TR("连接网络，选择谁来用"), INK_FONT_NOTE, INK_CANVAS_H / 2 + INK_PRIMARY);
        /* "先用起来" as on the no-hub page: without a network or a family server the device is still usable (trial, 2026-11-11) */
        button(TR("先用起来"), "shell.launch", "boot.hello", INK_MARGIN,
            INK_CANVAS_H - INK_BLOCK - 2 * INK_CONTROL - INK_GAP, w, false);
        button(TR("在设备上设置"), "shell.launch", "boot.wifi", INK_MARGIN,
            INK_CANVAS_H - INK_BLOCK - INK_CONTROL, w, true);
    } else if (B.page == WIFI) {
        if (!B.count) text(TR("还没找到网络，可以再找一次"), INK_FONT_NOTE, 2 * INK_PRIMARY);
        for (int i = B.wifi_page * 3; i < B.count && i < (B.wifi_page + 1) * 3; i++) {
            char id[32]; snprintf(id, sizeof id, "boot.pick%d", i);
            int y = INK_STATUS_BAR + INK_CONTROL + INK_GAP + (i % 3) * (INK_CONTROL + INK_GAP);
            const ink_ui_action_spec_t *a = action(B.ssid[i], "shell.launch", id);
            ink_ui_list_row(screen, "wifi", B.ssid[i], B.secured[i] ? TR("需要密码") : TR("开放网络"),
                INK_MARGIN, y, w, false, clicked, (void *)a);
            B.fields[B.model.field_count++] = (ink_ui_field_spec_t){B.ssid[i], "text", TR("附近网络"), B.ssid[i], {NULL, 0}, false};
        }
        button(TR("其他网络"), "shell.launch", "boot.other", INK_MARGIN,
            INK_STATUS_BAR + INK_CONTROL + INK_GAP + 3 * (INK_CONTROL + INK_GAP), w, false);
        bottom(TR("再找一次"), "shell.launch", "boot.wifi", B.count <= 3);
        if (B.count > 3) bottom(TR("下一组"), "shell.launch", "boot.next", true);
    } else if (B.page == PASSWORD) {
        bool secret = B.picked[0] != 0;
        ink_text_cfg_t cfg = {.type = secret ? INK_TEXT_PASSWORD : INK_TEXT_PLAIN, .enter = secret ? INK_ENTER_DONE : INK_ENTER_NEXT,
            .max = secret ? 64 : 32, .ai_hidden = secret, .hint = secret ? TR("输入网络密码") : TR("输入网络名称")};
        B.field = ink_textfield_create(screen, &cfg, password_entered, NULL, false);
        lv_obj_set_pos(B.field, ink_ui_px(INK_MARGIN), ink_ui_px(INK_STATUS_BAR + INK_CONTROL + INK_GAP));
        ink_textfield_style(B.field, secret ? INK_FIELD_PASSWORD : INK_FIELD_SINGLE, secret ? TR("网络密码 · 完成后连接") : TR("网络名称"));
        B.fields[2] = (ink_ui_field_spec_t){secret ? "password" : "ssid", "textbox", secret ? TR("网络密码") : TR("网络名称"), "", {NULL, 0}, secret};
        B.model.field_count = 3;
        const ink_ui_action_spec_t *focus = action(TR("输入密码"), "shell.launch", "boot.focus");
        if (!ink_textfield_bind_short(B.field, &B.model, &B.fields[2], focus))
            ink_exec_post(INK_EXEC_UI, &B.focus, focus_password, NULL);
    } else if (B.page == CLAIM) {
        if (B.computer) {
            const char *code = strstr(B.claim, "/claim/");
            if (code) {
                char address[256]; snprintf(address, sizeof address, "%.*s", (int)(code - B.claim), B.claim);
                text(address, INK_FONT_NOTE, 3 * INK_PRIMARY);
                snprintf(B.message, sizeof B.message, TR("加入码 %.8s\n确认码 %.12s\n填到电脑上，再回设备确认加入。"), code + 7, B.check);
            }
        } else {
            if (B.claim[0]) qr(B.claim);
            snprintf(B.message, sizeof B.message, TR("用手机扫码。码过期后会自动更换。"));
        }
        text(B.message, INK_FONT_NOTE, INK_CANVAS_H / 2 + INK_PRIMARY + INK_GAP);
        bottom(TR("返回选网"), "shell.launch", "boot.wifi", false);
        bottom(B.computer ? TR("用手机扫码") : TR("用电脑加入"), "shell.launch", "boot.computer", true);
        B.fields[2] = (ink_ui_field_spec_t){"claim", "text", TR("加入链接"), B.claim, {NULL, 0}, true};
        B.fields[3] = (ink_ui_field_spec_t){"check", "text", TR("确认码"), B.check, {NULL, 0}, true}; B.model.field_count = 4;
    } else if (B.page == CONFIRM) {
        ink_ui_circle(screen, "family", (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_CANVAS_H / 2 - INK_CIRCLE_LARGE - INK_GAP,
            INK_CIRCLE_LARGE, INK_ICON_STATE, INK_SURFACE_BLACK, NULL, NULL);
        snprintf(B.message, sizeof B.message, TR("%s邀请你加入“%s”。请在设备上核对。"), B.by, B.family);
        bottom(TR("不是我们家"), "pairing.reject", B.txid, false); bottom(TR("加入"), "pairing.accept", B.txid, true);
    } else if (B.page == RESTORE) {
        bottom(TR("当新设备用"), "shell.launch", "boot.hello", !B.preview[0]);
        if (B.preview[0]) bottom(TR("放回来"), "restore.apply", B.preview, true);
        else bottom(TR("再查一次"), "shell.launch", "boot.restore", false);
    } else if (B.page == HELLO) {
        snprintf(B.message, sizeof B.message, "%s", ink_hub_online() ?
            TR("打开示例，按住 AI 键问“夜邮是什么意思”。") : TR("示例离线也能读。AI 还要等家庭服务器设置好。"));
        bottom(TR("跳过"), "system.home", "", false); bottom(TR("看看示例"), "reader.open_sample", "", true);
    } else if (B.page == JOIN_FAIL) {
        bottom(TR("换个网络"), "shell.launch", "boot.wifi", false); bottom(TR("重新输入"), "shell.launch", "boot.password", true);
    } else if (B.page == NO_HUB) {
        snprintf(B.message, sizeof B.message, TR("家庭服务器未连接，AI 和备份暂不可用。可先用；手机设置请返回选网，再返回首页扫码。"));
        button(TR("返回选网"), "shell.launch", "boot.wifi", INK_MARGIN, INK_CANVAS_H - INK_BLOCK - INK_PRIMARY - INK_CONTROL - INK_GAP, w, false);
        bottom(TR("先用起来"), "shell.launch", "boot.hello", false); bottom(TR("再试一次"), "shell.launch", "boot.retry", true);
    } else if (B.page == REVOKED) {
        snprintf(B.message, sizeof B.message, TR("这台设备已被移出家庭，AI 和备份不可用。可回主页查看本地书和备忘，资料是否保留以实际列表为准。"));
        bottom(TR("知道了"), "system.home", "", false); bottom(TR("重新加入"), "shell.launch", "boot.restart", true);
    } else bottom(TR("取消"), "shell.launch", "boot.wifi", false);
    if (B.page != PASSWORD && B.page != FIRST && B.page != WIFI && B.page != CLAIM)
        text(B.message, INK_FONT_NOTE, INK_CANVAS_H / 2 + INK_LINE_SUBTITLE + INK_GAP);
    if (B.page != PASSWORD && B.page != HELLO) for (int i = 0; i < 3; i++) {
        int step = B.page < CLAIM ? 0 : B.page < RESTORE ? 1 : 2;
        ink_ui_box(screen, INK_CANVAS_W / 2 - INK_RADIUS_PANEL + i * INK_RADIUS_PANEL,
            INK_CANVAS_H - INK_MARGIN, i == step ? INK_ICON_MEDIUM : INK_GAP_M, INK_GAP_M, -1,
            i == step ? INK_SURFACE_BLACK : INK_SURFACE_WHITE, i != step);
    }
}
static void reply(void *ctx, const ink_receipt_t *rc) {
    uintptr_t tag = (uintptr_t)ctx; int kind = (int)(tag & 15);
    if ((uint32_t)(tag >> 4) != ink_member_epoch() || !current()) return;
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    bool ok = j && rc->result && ink_json_parse(j, rc->result, strlen(rc->result)) == 0;
    if (kind == SCAN_REPLY && B.page == WIFI) {
        B.count = B.wifi_page = 0;
        if (ok) for (int i = 1; i < j->ntok && B.count < BOOT_WIFI_MAX; i++) if (j->tok[i].type == 1) {
            if (ink_json_str(j, ink_json_find(j, i, "ssid"), B.ssid[B.count], sizeof B.ssid[0]) > 0)
                B.secured[B.count++] = ink_json_eq(j, ink_json_find(j, i, "secured"), "true");
            int end = j->tok[i].end;
            while (i + 1 < j->ntok && j->tok[i + 1].start < end) i++;
        }
        show(WIFI);
    } else if (kind == JOIN_REPLY && B.page == JOINING) {
        snprintf(B.message, sizeof B.message, "%s", rc->status == INK_RC_DONE ? TR("正在寻找家庭服务器") :
            !strcmp(rc->detail, "wrong password") ? TR("密码不对，再输一次，注意大小写。") :
            !strcmp(rc->detail, "that network is not around") ? TR("找不到这个网络，请靠近路由器或换个网络。") :
            !strcmp(rc->detail, "it took too long") ? TR("连接超时，请重试或换个网络。") :
            !strcmp(rc->detail, "Wi-Fi is off") ? TR("Wi-Fi 已关闭，请到网络设置打开后重试。") : TR("没能连上网络，可以重试或换个网络。"));
        show(rc->status == INK_RC_DONE ? CLAIM : JOIN_FAIL);
        if (rc->status == INK_RC_DONE) request("pairing.status", NULL, PAIR_REPLY, INK_SRC_SYSTEM);
    } else if (kind == HOTSPOT_REPLY && B.page == FIRST) {
        char value[128] = "";
        if (ok && ink_json_str(j, ink_json_find(j, 0, "wifi_qr"), value, sizeof value) > 0) qr(value);
        memset(value, 0, sizeof value);
    } else if (kind == RESTORE_REPLY && B.page == RESTORE) {
        if (ok) ink_json_str(j, ink_json_find(j, 0, "preview"), B.preview, sizeof B.preview);
        snprintf(B.message, sizeof B.message, "%s", !ok ? TR("暂时查不了备份，可以先当新设备用。") : ink_json_eq(j, ink_json_find(j, 0, "phase"), "failed") ?
            TR("还没有可恢复的备份，可以当新设备用。") : B.preview[0] ? TR("找到了这位家人的备份。恢复前会请你确认覆盖。设备设置和密码需要重新设置。") : TR("正在查找或恢复这位家人的备份。"));
        int64_t created = 0;
        if (ok && B.preview[0] && ink_json_int(j, ink_json_find(j, 0, "created_s"), &created) && created > 0) {
            ink_tm_t tm; ink_time_to_local(created, &tm);
            int64_t items = 0;
            ink_json_int(j, ink_json_find(j, 0, "item_count"), &items);
            snprintf(B.message, sizeof B.message, TR("%d月%d日 %02d:%02d，%s的备份，共%d项。会覆盖现有资料；设置和密码需重设。"),
                tm.month, tm.day, tm.hour, tm.min, ink_member_current()->name, (int) items);
        }
        if (ok && ink_json_eq(j, ink_json_find(j, 0, "phase"), "done")) show(HELLO); else show(RESTORE);
    } else if (kind == PAIR_REPLY && ok) {
        ink_json_str(j, ink_json_find(j, 0, "claim"), B.claim, sizeof B.claim);
        ink_json_str(j, ink_json_find(j, 0, "txid"), B.txid, sizeof B.txid);
        ink_json_str(j, ink_json_find(j, 0, "family"), B.family, sizeof B.family);
        ink_json_str(j, ink_json_find(j, 0, "by"), B.by, sizeof B.by);
        ink_json_str(j, ink_json_find(j, 0, "check"), B.check, sizeof B.check);
        if (ink_pairing_state() == INK_PAIR_REVOKED) show(REVOKED);
        else if (ink_pairing_state() == INK_PAIR_BOUND && B.page < RESTORE) {
            B.preview[0] = 0; snprintf(B.message, sizeof B.message, TR("正在查找这位家人的备份。"));
            show(RESTORE); request("restore.preview", summary(), RESTORE_REPLY, INK_SRC_SYSTEM);
        } else if (B.page == CLAIM || B.page == NO_HUB || B.page == CONFIRM || B.page == FIRST) {
            if (B.txid[0] && B.claim[0] && ink_pairing_state() == INK_PAIR_PAIRING) show(CONFIRM);
            else if (B.claim[0]) show(CLAIM);
            else if (B.page != FIRST) show(NO_HUB);
        }
    }
    osal_free(j);
}
bool ink_boot_flow_launch(ink_call_t *c) {
    char id[32]; ink_args_get_str(&c->args, "id", id, sizeof id);
    if (strcmp(id, "boot") && strncmp(id, "boot.", 5)) return false;
    if (ink_member_current()->role == INK_ROLE_CHILD || !ink_member_space_open()) c->rc.status = INK_RC_REJECTED;
    else if (!strcmp(id, "boot")) { B.message[0] = 0; show(FIRST); request("pairing.status", NULL, PAIR_REPLY, c->src); }
    else if (!current()) c->rc.status = INK_RC_STALE;
    else if (!strcmp(id, "boot.back") && B.page == WIFI) show(FIRST);
    else if (!strcmp(id, "boot.wifi") || !strcmp(id, "boot.back")) {
        request("pairing.hotspot_stop", NULL, HOTSPOT_REPLY, c->src);
        B.message[0] = 0; show(WIFI); request("wifi.scan", NULL, SCAN_REPLY, c->src);
    } else if (!strncmp(id, "boot.pick", 9)) {
        int i = atoi(id + 9);
        if (B.page != WIFI || i < 0 || i >= B.count) c->rc.status = INK_RC_STALE;
        else {
            snprintf(B.picked, sizeof B.picked, "%s", B.ssid[i]);
            if (B.secured[i]) show(PASSWORD); else password_entered(NULL, "");
        }
    } else if (!strcmp(id, "boot.other") && B.page == WIFI) { B.picked[0] = 0; show(PASSWORD); }
    else if (!strcmp(id, "boot.focus") && B.page == PASSWORD) ink_textfield_short_focus(B.field);
    else if (!strcmp(id, "boot.password") && B.picked[0]) show(PASSWORD);
    else if (!strcmp(id, "boot.next") && B.page == WIFI && B.count > 3) { B.wifi_page = (B.wifi_page + 1) % ((B.count + 2) / 3); show(WIFI); }
    else if (!strcmp(id, "boot.hello")) show(HELLO);
    else if (!strcmp(id, "boot.restore")) request("restore.preview", summary(), RESTORE_REPLY, c->src);
    else if (!strcmp(id, "boot.restart")) { request("pairing.restart", NULL, PAIR_REPLY, c->src); show(FIRST); }
    else if (!strcmp(id, "boot.retry")) { request("pairing.restart", NULL, PAIR_REPLY, c->src); request("pairing.status", NULL, PAIR_REPLY, c->src); }
    else if (!strcmp(id, "boot.computer")) { B.computer = !B.computer; show(CLAIM); }
    else c->rc.status = INK_RC_REJECTED;
    return true;
}
static bool key(ink_key_t code, bool long_press) {
    if (long_press || (code != INK_KEY_BACK && code != INK_KEY_HOME)) return false;
    ink_receipt_t rc;
    ink_cap_invoke_json(code == INK_KEY_HOME || B.page == FIRST ? "system.home" : "shell.launch",
        "{\"id\":\"boot.back\"}", INK_SRC_KEY, 0, &rc);
    return true;
}
static void changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (current()) request("pairing.status", NULL, PAIR_REPLY, INK_SRC_SYSTEM);
    else if (ink_pairing_state() == INK_PAIR_REVOKED && ink_member_space_open() && ink_member_current()->role != INK_ROLE_CHILD) show(REVOKED);
}
static void backup_changed(void *ctx, const char *id, const ink_args_t *args) {
    (void)ctx; (void)id; (void)args;
    if (current() && B.page == RESTORE) request("backup.manifest", summary(), RESTORE_REPLY, INK_SRC_SYSTEM);
}
void ink_boot_flow_resume(void) {
    if (ink_factory_reset_completed()) { ink_factory_done_open(); return; }
    int page = B.resume;
    if (page < FIRST || page > REVOKED) return;
    if (page == JOINING) page = B.picked[0] ? PASSWORD : WIFI;
    if (page == CONFIRM) page = CLAIM;
    show(page);
    if (page == WIFI) request("wifi.scan", NULL, SCAN_REPLY, INK_SRC_SYSTEM);
    else if (page == RESTORE) request("restore.preview", summary(), RESTORE_REPLY, INK_SRC_SYSTEM);
    else if (page <= CONFIRM) request("pairing.status", NULL, PAIR_REPLY, INK_SRC_SYSTEM);
}
int ink_boot_flow_init(void) {
    B.resume = -1;
    if (ink_db_register(&PROGRESS) == 0) B.progress = ink_db_open(&PROGRESS, NULL);
    if (!ink_members_configured() && ink_pairing_state() != INK_PAIR_BOUND) B.resume = FIRST;
    char saved[256]; ink_rec_t rec;
    if (B.progress && ink_db_get(B.progress, 1, &rec, saved, sizeof saved - 1) == INK_DB_OK && rec.len < sizeof saved) {
        saved[rec.len] = 0;
        ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
        if (j && ink_json_parse(j, saved, rec.len) == 0 && rec.sort >= -1 && rec.sort <= REVOKED) {
            B.resume = (int)rec.sort;
            ink_json_str(j, ink_json_find(j, 0, "network"), B.picked, sizeof B.picked);
        }
        osal_free(j);
    }
    return ink_cap_subscribe("pairing.changed", changed, NULL, INK_EXEC_UI) ||
        ink_cap_subscribe("backup.", backup_changed, NULL, INK_EXEC_UI) ? -1 : 0;
}
