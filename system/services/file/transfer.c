/* One authorization, one member, one file; driven by the shared BG executor. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "ink/hal_device.h"
#include "ink/hal_net.h"
#include "ink/hal_transfer.h"
#include "ink/osal.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/svc.h"
#include "file_internal.h"

#define INBOX "/me/收件箱/"
static struct {
    file_context_t actor;
    uint32_t epoch, code;
    uint16_t port;
    bool active, connected, used;
    uint8_t misses; /* wrong codes this authorization; the third closes it, so a code cannot be guessed */
    uint64_t expires, idle, left;
    size_t head_len;
    ink_fs_file_t *file;
    char head[2048], buf[2048], path[INK_FS_PATH_MAX], part[INK_FS_PATH_MAX];
    const char *result, *response;
    ink_exec_job_t job;
    ink_svc_deadline_t tick;
} T INK_PSRAM_BSS;

static const char *reason(int r) {
    switch (r) {
    case INK_FS_ERR_USB: return "U 盘模式已开启";
    case INK_FS_ERR_NO_CARD: return "未插存储卡";
    case INK_FS_ERR_NO_SPACE: return "存储空间不足";
    case INK_FS_ERR_EXISTS: return "文件已存在";
    case INK_FS_ERR_PATH: return "文件名不合法";
    default: return "文件操作失败";
    }
}

static void finish(const char *result) {
    hal_transfer_close(1);
    if (T.file) { ink_fs_close(T.file); T.file = NULL; }
    if (T.part[0]) fs_remove_for(&T.actor, T.part, 0);
    if (T.active) {
        ink_fs_card_release();
        ink_power_release(INK_NEED_AWAKE);
    }
    T.active = false; T.code = 0; T.result = result; T.response = NULL;
    ink_svc_cancel(&T.tick);
}

static void reply(int status, const char *text, bool html) {
    char header[180];
    int n = snprintf(header, sizeof header, "HTTP/1.0 %d Result\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %zu\r\n\r\n",
                     status, html ? "text/html" : "text/plain", strlen(text));
    if (hal_transfer_write(header, (size_t)n) == 0) hal_transfer_write(text, strlen(text));
    T.response = text;
}

static const char PAGE[] =
    "<!doctype html><meta charset=utf-8><title>无线传文件</title><form>"
    "<label>授权码<input id=c inputmode=numeric pattern='[0-9]{6}' required></label>"
    "<input id=f type=file required><button>上传</button></form><p id=r></p><script>"
    "document.querySelector('form').onsubmit=async e=>{e.preventDefault();try{"
    "let x=await fetch('/upload',{method:'POST',headers:{'X-Ink-Code':c.value,"
    "'X-Ink-Name':encodeURIComponent(f.files[0].name)},body:f.files[0]});r.textContent=await x.text()"
    "}catch(e){r.textContent='连接已断开，请重新授权'}};</script>";

static bool decode_name(char *name, const char *encoded) {
    size_t n = 0;
    while (*encoded) {
        unsigned v = (unsigned char)*encoded++;
        if (v == '%') {
            char h[3] = {encoded[0], encoded[0] ? encoded[1] : 0, 0}, *end;
            if (!h[0] || !h[1] || strspn(h, "0123456789abcdefABCDEF") != 2) return false;
            v = (unsigned)strtoul(h, &end, 16); encoded += 2;
        }
        if (v < 32 || v == 127 || v == '/' || v == '\\' || n >= INK_FS_NAME_MAX - 6) return false;
        name[n++] = (char)v;
    }
    name[n] = 0;
    return n && name[0] != '.' && !(n >= 5 && !strcmp(name + n - 5, ".part")) && !(n >= 4 && !strcmp(name + n - 4, ".tmp"));
}

static void headers(void) {
    if (!strncmp(T.head, "GET / HTTP/1.", 13)) { reply(200, PAGE, true); return; }
    char *save, *line = strtok_r(T.head, "\r\n", &save), *code = NULL, *name = NULL, *length = NULL;
    if (!line || (strcmp(line, "POST /upload HTTP/1.1") && strcmp(line, "POST /upload HTTP/1.0"))) { reply(400, "请求不合法", false); return; }
    while ((line = strtok_r(NULL, "\r\n", &save))) {
        char *colon = strchr(line, ':');
        if (!colon) { reply(400, "请求不合法", false); return; }
        *colon++ = 0; while (*colon == ' ') colon++;
        if (!strcasecmp(line, "X-Ink-Code") && !code) code = colon;
        else if (!strcasecmp(line, "X-Ink-Name") && !name) name = colon;
        else if (!strcasecmp(line, "Content-Length") && !length) length = colon;
        else if (!strcasecmp(line, "Transfer-Encoding")) { reply(400, "不支持此传输方式", false); return; }
    }
    bool valid_length = length && *length && strlen(length) <= 18 && strspn(length, "0123456789") == strlen(length);
    if (valid_length) T.left = strtoull(length, NULL, 10);
    char expected[8]; snprintf(expected, sizeof expected, "%06u", T.code);
    if (!code || strcmp(code, expected)) {
        T.used = ++T.misses >= 3; /* used: closes once this answer is out */
        reply(403, T.used ? "授权码错误次数过多，已关闭" : "授权码不正确或已失效", false);
        return;
    }
    T.used = true; T.code = 0;
    char filename[INK_FS_NAME_MAX];
    if (!name || !decode_name(filename, name) || !valid_length) {
        reply(400, "文件名或长度不合法", false); return;
    }
    snprintf(T.path, sizeof T.path, INBOX "%s", filename);
    ink_fs_stat_t st;
    int r = fs_stat_for(&T.actor, T.path, &st);
    if (r != INK_FS_ERR_NOT_FOUND) { reply(r == 0 ? 409 : 400, r == 0 ? "文件已存在" : "文件名不合法", false); return; }
    uint64_t free_bytes = 0, total;
    r = ink_fs_free(&free_bytes, &total);
    if (r || T.left > free_bytes) { reply(507, r ? reason(r) : "存储空间不足", false); return; }
    snprintf(T.part, sizeof T.part, INBOX "%s.part", filename);
    r = fs_open_for(&T.actor, T.part, INK_FS_WRITE | INK_FS_CREATE | INK_FS_EXCL, &T.file);
    if (r) { T.part[0] = 0; reply(409, reason(r), false); }
}

static void poll_upload(void *arg);
static void tick(void *arg) { (void)arg; ink_exec_post(INK_EXEC_BG, &T.job, poll_upload, NULL); }
static void poll_upload(void *arg) {
    (void)arg;
    if (!T.active) return;
    uint64_t wait_us = 20000;
    hal_link_status_t link; hal_net_link_status(&link);
    if ((!T.used && !T.response && osal_now_us() >= T.expires) || (T.connected && osal_now_us() >= T.idle)) { finish("授权或接收已超时"); return; }
    if (T.epoch != ink_member_epoch() || !ink_member_space_open()) { finish("成员已变化"); return; }
    if (!link.up || link.type != HAL_LINK_WIFI) { finish("无线连接已断开"); return; }
    if (!T.connected) {
        int r = hal_transfer_accept();
        if (r < 0) { finish("连接失败"); return; }
        T.connected = r == 1; T.idle = osal_now_us() + 10000000;
        if (T.connected) { T.left = 0; T.response = NULL; }
    }
    if (T.connected) {
        int n = hal_transfer_read(T.buf, T.file || T.response ? sizeof T.buf : 1);
        if (n < 0) {
            if (T.used) { finish(T.response ? T.response : "传输中断"); return; }
            hal_transfer_close(0); T.connected = false; T.head_len = 0; T.response = NULL;
        } else if (n > 0) {
            T.idle = osal_now_us() + 10000000;
            if (T.response) {
                T.left -= (uint64_t)n < T.left ? (uint64_t)n : T.left;
            } else if (!T.file) {
                wait_us = 100;
                if (!T.buf[0]) reply(400, "请求不合法", false);
                else {
                    T.head[T.head_len++] = T.buf[0]; T.head[T.head_len] = 0;
                    if (T.head_len >= 4 && !strcmp(T.head + T.head_len - 4, "\r\n\r\n")) headers();
                    else if (T.head_len == sizeof T.head - 1) reply(400, "请求过长", false);
                }
            } else {
                if ((uint64_t)n > T.left) { T.left = 0; reply(400, "文件长度不符", false); }
                else {
                    T.left -= (uint64_t)n;
                    int64_t written = ink_fs_write(T.file, T.buf, (size_t)n);
                    if (written != n) reply(507, "存储空间不足或写入失败", false);
                }
            }
        }
        if (T.active && T.file && !T.left && !T.response) {
            int r = ink_fs_sync(T.file); int closed = ink_fs_close(T.file); T.file = NULL;
            if (!r) r = closed;
            if (!r) r = fs_move_for(&T.actor, T.part, T.path, 0, NULL);
            if (!r) T.part[0] = 0;
            reply(r ? 500 : 201, r ? "文件保存失败" : "文件已收到", false);
        }
        if (T.response && !T.left) {
            hal_transfer_close(0); T.connected = false; T.head_len = 0;
            if (T.used) finish(T.response);
            T.response = NULL;
        }
    }
    if (T.active) ink_svc_at(&T.tick, osal_now_us() + wait_us, tick, NULL);
}

static void status(ink_call_t *c) {
    if (T.active && T.epoch != ink_member_epoch()) finish("成员已变化");
    int64_t received = 0;
    if (T.epoch == ink_member_epoch()) {
        ink_fs_stat_t st;
        if (T.file) received = ink_fs_tell(T.file);
        else if (T.result && !strcmp(T.result, "文件已收到") && !fs_stat_for(&T.actor, T.path, &st)) received = st.size;
    }
    char out[288];
    snprintf(out, sizeof out, "{\"active\":%s,\"port\":%u,\"code\":\"%06u\",\"directory\":\"" INBOX "\",\"result\":\"%s\",\"received\":%lld}",
             T.active ? "true" : "false", T.port, T.code, T.result ? T.result : "尚未开启", (long long)received);
    ink_cap_result(c, out);
}
static void start(ink_call_t *c) {
    file_context_t actor;
    if (!fs_context_for_call(&actor, c)) return;
    int r = INK_FS_ERR_BUSY;
    hal_link_status_t link; hal_net_link_status(&link);
    if (!T.active && ink_member_space_open() && link.up && link.type == HAL_LINK_WIFI) {
        r = ink_fs_card_hold();
        if (!r) {
            T.actor = actor; T.epoch = ink_member_epoch(); T.port = 0;
            if (hal_transfer_listen(&T.port)) { ink_fs_card_release(); r = INK_FS_ERR_IO; }
            else {
                hal_random_fill(&T.code, sizeof T.code); T.code = 100000 + T.code % 900000;
                T.active = true; T.connected = T.used = false; T.misses = 0; T.head_len = 0; T.part[0] = 0;
                T.expires = osal_now_us() + 120000000; T.result = "等待上传";
                ink_power_hold(INK_NEED_AWAKE); tick(NULL);
            }
        }
    }
    if (r) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.detail, sizeof c->rc.detail, "%s", r == INK_FS_ERR_BUSY ? "传输已开启、空间未解锁或无线网络不可用" : reason(r)); }
    else status(c);
}
static void stop(ink_call_t *c) { finish("传输已关闭"); status(c); }
static const ink_cap_t TRANSFER_CAPS[] = {
    {.id = "transfer.start", .title = "开启一次无线传文件授权（两分钟内有效）", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .permission = "file", .exec = INK_EXEC_BG, .run = start},
    {.id = "transfer.stop", .title = "关闭无线传文件", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .permission = "file", .exec = INK_EXEC_BG, .run = stop},
    {.id = "transfer.status", .title = "查看无线传文件授权和结果", .effect = INK_FX_READ, .child_ok = true, .permission = "file", .exec = INK_EXEC_BG, .run = status},
};
int ink_transfer_caps_init(void) { return ink_cap_register(TRANSFER_CAPS, sizeof TRANSFER_CAPS / sizeof TRANSFER_CAPS[0]); }
