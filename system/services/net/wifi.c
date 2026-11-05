/*
 * Wi-Fi management (system services design §11.1): the networks this device knows, finding new ones. Passwords go in and
 * never come out: the collection is closed to the AI, and they are in no snapshot, log or receipt. Choosing between Wi-Fi
 * and LTE stays with the net service (D-58): this only says which networks are known.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_net.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/crc.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"
#include "ink/sys/net.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"

#define MAX_KNOWN 8
#define MAX_SEEN 12
#define SCAN_MS 4000
#define JOIN_MS 10000

/* Closed: no ai_read, its name is not offered to anyone. */
static const ink_db_def_t KNOWN = {.name = "net.wifi", .title = "已知的 Wi-Fi", .scope = INK_DB_DEVICE, .schema = 1, .max_records = MAX_KNOWN};

static struct {
    int h_state, h_ssid;
} W INK_PSRAM_BSS;

static void fail(ink_call_t *c, ink_rc_t st, const char *why) {
    c->rc.status = st;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
}

static uint32_t ssid_id(const char *ssid) {
    uint32_t id = ink_crc32(0, ssid, strlen(ssid)) & 0x7fffffff;
    return id ? id : 1;
}

static void told(const char *what, const char *ssid) {
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (a) {
        ink_args_init(a);
        ink_args_str(a, "what", what);
        ink_args_str(a, "ssid", ssid);
        ink_cap_emit("wifi.changed", a);
        osal_free(a);
    }
}

/* The platform's list from what is stored. Returns how many. */
static int publish_known(void) {
    ink_db_t *db = ink_db_open(&KNOWN, NULL);
    hal_wifi_known_t *list = osal_malloc(sizeof *list * MAX_KNOWN, OSAL_MEM_EXTERNAL);
    int n = 0;
    if (db && list) {
        ink_db_cursor_t cur = {0};
        ink_rec_t r;
        while (n < MAX_KNOWN && ink_db_next(db, &cur, &r, &list[n], sizeof list[n]) == 1)
            if (r.len == sizeof list[n]) n++;
        hal_wifi_set_known(list, n);
    }
    osal_free(list);
    ink_db_close(db);
    return n;
}

static void cap_scan(ink_call_t *c) { /* exec.bg */
    hal_wifi_ap_t *ap = osal_malloc(sizeof *ap * MAX_SEEN, OSAL_MEM_EXTERNAL);
    char *out = osal_malloc(1536, OSAL_MEM_EXTERNAL);
    if (!ap || !out) fail(c, INK_RC_FAILED, "out of memory");
    else {
        ink_state_set_str(W.h_state, "scanning");
        ink_power_hold(INK_NEED_AWAKE);
        int n = hal_wifi_scan(ap, MAX_SEEN, SCAN_MS);
        ink_power_release(INK_NEED_AWAKE);
        ink_state_set_str(W.h_state, "idle");
        ink_jw_t w;
        ink_jw_init(&w, out, 1536);
        ink_jw_raw(&w, "[");
        for (int i = 0; i < n; i++) {
            ink_jw_raw(&w, i ? ",{\"ssid\":" : "{\"ssid\":");
            ink_jw_str(&w, ap[i].ssid);
            ink_jw_raw(&w, ",\"rssi\":");
            ink_jw_int(&w, ap[i].rssi);
            ink_jw_raw(&w, ap[i].secured ? ",\"secured\":true}" : ",\"secured\":false}");
        }
        ink_jw_raw(&w, "]");
        if (w.overflow) fail(c, INK_RC_FAILED, "too long");
        else ink_cap_result(c, out);
    }
    osal_free(out);
    osal_free(ap);
}

static void cap_list(ink_call_t *c) { /* exec.bg: names only */
    ink_db_t *db = ink_db_open(&KNOWN, NULL);
    hal_wifi_known_t *k = osal_malloc(sizeof *k, OSAL_MEM_EXTERNAL);
    char *out = osal_malloc(512, OSAL_MEM_EXTERNAL);
    if (!db || !k || !out) fail(c, INK_RC_FAILED, "out of memory");
    else {
        ink_jw_t w;
        ink_jw_init(&w, out, 512);
        ink_jw_raw(&w, "[");
        ink_db_cursor_t cur = {0};
        ink_rec_t r;
        bool first = true;
        while (ink_db_next(db, &cur, &r, k, sizeof *k) == 1) {
            if (!first) ink_jw_raw(&w, ",");
            first = false;
            ink_jw_str(&w, k->ssid);
        }
        ink_jw_raw(&w, "]");
        if (w.overflow) fail(c, INK_RC_FAILED, "too long");
        else ink_cap_result(c, out);
    }
    osal_free(out);
    osal_free(k);
    ink_db_close(db);
}

static void cap_connect(ink_call_t *c) { /* exec.bg */
    hal_wifi_known_t *k = osal_calloc(1, sizeof *k, OSAL_MEM_EXTERNAL);
    if (!k) {
        fail(c, INK_RC_FAILED, "out of memory");
        return;
    }
    ink_args_get_str(&c->args, "ssid", k->ssid, sizeof k->ssid);
    int password = ink_args_get_str(&c->args, "password", k->password, sizeof k->password);
    if (password > 0 && (c->src == INK_SRC_AI || c->src == INK_SRC_REMOTE || c->src == INK_SRC_APP)) {
        memset(k, 0, sizeof *k);
        osal_free(k);
        fail(c, INK_RC_REJECTED, "passwords are entered by the person on the device");
        return;
    }
    ink_db_t *known = ink_db_open(&KNOWN, NULL);
    hal_wifi_known_t *prior = osal_calloc(1, sizeof *prior, OSAL_MEM_EXTERNAL);
    int remembered = known && prior ? ink_db_get(known, ssid_id(k->ssid), NULL, prior, sizeof *prior) : INK_DB_ERR_NOMEM;
    bool had = remembered == INK_DB_OK && !strcmp(prior->ssid, k->ssid);
    bool can_undo = remembered == INK_DB_ERR_NOT_FOUND;
    if (password < 0 && had) snprintf(k->password, sizeof k->password, "%s", prior->password);
    if (prior) { memset(prior, 0, sizeof *prior); osal_free(prior); }
    if (known) ink_db_close(known);
    ink_state_set_str(W.h_state, "joining");
    ink_state_set_str(W.h_ssid, k->ssid);
    ink_power_hold(INK_NEED_AWAKE);
    int r = hal_wifi_connect(k->ssid, k->password, JOIN_MS);
    ink_power_release(INK_NEED_AWAKE);
    if (r == HAL_WIFI_OK) {
        ink_db_t *db = ink_db_open(&KNOWN, NULL);
        ink_db_write_t rec = {.written_ver = &c->rc.revision, .id = ssid_id(k->ssid), .data = k, .len = sizeof *k, .src = (uint8_t)c->src};
        int saved = db ? ink_db_put(db, &rec, NULL) : INK_DB_ERR_ARG;
        ink_db_close(db);
        if (saved != INK_DB_OK) ink_probe_count("wifi.not_saved");
        publish_known();
        ink_state_set_str(W.h_state, "connected");
        told("connected", k->ssid);
        snprintf(c->rc.say, sizeof c->rc.say, "连上了 %.60s", k->ssid);
        snprintf(c->rc.object, sizeof c->rc.object, "wifi:%.32s", k->ssid);
        if (can_undo) {
            ink_args_t *undo = osal_malloc(sizeof *undo, OSAL_MEM_EXTERNAL);
            if (undo) {
                ink_args_init(undo);
                ink_args_str(undo, "ssid", k->ssid);
                ink_args_int(undo, "if_revision", c->rc.revision);
                ink_cap_undoable(c, "wifi.forget", undo, 10000);
                osal_free(undo);
            }
        }
    } else {
        ink_state_set_str(W.h_state, "failed");
        ink_state_set_str(W.h_ssid, "");
        fail(c, INK_RC_FAILED, r == HAL_WIFI_ERR_AUTH ? "wrong password" : r == HAL_WIFI_ERR_NOT_FOUND ? "that network is not around" : "it took too long");
    }
    memset(k, 0, sizeof *k);
    osal_free(k);
}

static void cap_forget(ink_call_t *c) { /* exec.bg */
    char ssid[33];
    ink_args_get_str(&c->args, "ssid", ssid, sizeof ssid);
    ink_db_t *db = ink_db_open(&KNOWN, NULL);
    int64_t revision = 0;
    ink_args_get_int(&c->args, "if_revision", &revision);
    int r = db ? ink_db_del_if_ver(db, ssid_id(ssid), (uint8_t)c->src, (uint32_t)revision) : INK_DB_ERR_ARG;
    ink_db_close(db);
    if (r != INK_DB_OK) {
        fail(c, revision ? INK_RC_STALE : INK_RC_FAILED, "network changed or absent");
        return;
    }
    publish_known();
    ink_state_set_str(W.h_ssid, "");
    ink_state_set_str(W.h_state, "idle");
    told("forgotten", ssid);
}

static const ink_cap_t WIFI_CAPS[] = {
    {.id = "wifi.scan", .title = "找附近的 Wi-Fi（要几秒）", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .timeout_ms = 10000, .exec = INK_EXEC_BG, .run = cap_scan},
    {.id = "wifi.list", .title = "这台设备记得的 Wi-Fi（只有名字）", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .run = cap_list},
    {.id = "wifi.connect", .title = "连接一个 Wi-Fi 并记住它", .ask = "连接 Wi-Fi：{ssid}",
     .input = "{\"type\":\"object\",\"properties\":{\"ssid\":{\"type\":\"string\",\"maxLength\":32},\"password\":{\"type\":\"string\",\"maxLength\":64}},\"required\":[\"ssid\"]}",
     .effect = INK_FX_SYSTEM, .timeout_ms = 20000, .exec = INK_EXEC_BG, .run = cap_connect},
    {.id = "wifi.forget", .title = "忘掉一个 Wi-Fi", .ask = "忘掉 Wi-Fi：{ssid}", .input = "{\"type\":\"object\",\"properties\":{\"ssid\":{\"type\":\"string\",\"maxLength\":32}},\"required\":[\"ssid\"]}",
     .effect = INK_FX_LOCAL_DESTRUCTIVE, .ask_by_hand = true, .exec = INK_EXEC_BG, .run = cap_forget},
    {.id = "wifi.changed", .title = "Wi-Fi 的情况变了（连上了、忘掉了）", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

static const ink_state_def_t ST_STATE = {.id = "wifi.state", .type = INK_ST_STR, .title = "Wi-Fi（idle、scanning、joining、connected、failed）"};
static const ink_state_def_t ST_SSID = {.id = "wifi.ssid", .type = INK_ST_STR, .title = "连的 Wi-Fi"};

int ink_wifi_init(void) {
    W.h_state = ink_state_define(&ST_STATE);
    W.h_ssid = ink_state_define(&ST_SSID);
    ink_state_set_str(W.h_state, "idle");
    ink_state_set_str(W.h_ssid, "");
    if (ink_db_register(&KNOWN) != 0) return -1;
    if (publish_known()) ink_probe_count("wifi.known_loaded");
    return ink_cap_register(WIFI_CAPS, (int)(sizeof WIFI_CAPS / sizeof WIFI_CAPS[0]));
}
