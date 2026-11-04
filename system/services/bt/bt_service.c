/*
 * Bluetooth headsets (system services design §10.4): finding, pairing and connecting them, and what follows. Connected, the
 * sound goes to the headset; gone, it comes back to the speaker and media pauses. How the chip does it is not known yet
 * (ink/hal_bt.h): where the HAL says it cannot, these capabilities say so and the speaker stays.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_bt.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/audio.h"
#include "ink/sys/cap.h"
#include "ink/sys/crc.h"
#include "ink/sys/data.h"
#include "ink/sys/json.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"

#define SCAN_MS 4000
#define MAX_SCAN 8

static const ink_db_def_t DEVICES = {.name = "bt.devices", .title = "配对过的蓝牙耳机", .scope = INK_DB_DEVICE, .schema = 1, .json = true, .ai_read = true, .max_records = 16};

static struct {
    int h_available, h_connected, h_device;
    char current[18];
    ink_exec_job_t lost_job;
} BT INK_PSRAM_BSS;

static void fail(ink_call_t *c, ink_rc_t st, const char *why) {
    c->rc.status = st;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
}

static bool usable(ink_call_t *c) {
    if (hal_bt_available()) return true;
    fail(c, INK_RC_FAILED, "this device cannot use a Bluetooth headset yet");
    return false;
}

static uint32_t addr_id(const char *addr) {
    uint32_t id = ink_crc32(0, addr, strlen(addr)) & 0x7fffffff;
    return id ? id : 1;
}

static void told(const char *what) {
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (a) {
        ink_args_init(a);
        ink_args_str(a, "what", what);
        ink_cap_emit("bt.changed", a);
        osal_free(a);
    }
}

static void set_connected(const char *addr) {
    bool on = addr && addr[0];
    ink_svc_lock();
    snprintf(BT.current, sizeof BT.current, "%s", on ? addr : "");
    ink_svc_unlock();
    ink_state_set_bool(BT.h_connected, on);
    ink_state_set_str(BT.h_device, on ? addr : "");
    ink_audio_set_route(on ? INK_ROUTE_HEADSET : INK_ROUTE_SPEAKER);
    told(on ? "connected" : "disconnected");
}

static void lost_job(void *arg) { /* exec.bg */
    (void)arg;
    ink_probe_count("bt.lost");
    set_connected(NULL);
}

static void on_link(void *ctx, hal_bt_event_t ev) { /* where the news arrives: an event, maybe */
    (void)ctx;
    if (ev == HAL_BT_EV_LOST) ink_exec_post(INK_EXEC_BG, &BT.lost_job, lost_job, NULL);
}

static void cap_scan(ink_call_t *c) { /* exec.bg */
    if (!usable(c)) return;
    hal_bt_device_t *d = osal_malloc(sizeof *d * MAX_SCAN, OSAL_MEM_EXTERNAL);
    char *out = osal_malloc(1024, OSAL_MEM_EXTERNAL);
    if (!d || !out) fail(c, INK_RC_FAILED, "out of memory");
    else {
        ink_power_hold(INK_NEED_AWAKE);
        int n = hal_bt_scan(d, MAX_SCAN, SCAN_MS);
        ink_power_release(INK_NEED_AWAKE);
        ink_jw_t w;
        ink_jw_init(&w, out, 1024);
        ink_jw_raw(&w, "[");
        for (int i = 0; i < n; i++) {
            ink_jw_raw(&w, i ? ",{\"addr\":" : "{\"addr\":");
            ink_jw_str(&w, d[i].addr);
            ink_jw_raw(&w, ",\"name\":");
            ink_jw_str(&w, d[i].name);
            ink_jw_raw(&w, "}");
        }
        ink_jw_raw(&w, "]");
        if (w.overflow) fail(c, INK_RC_FAILED, "too long");
        else ink_cap_result(c, out);
    }
    osal_free(out);
    osal_free(d);
}

static void cap_list(ink_call_t *c) { /* exec.bg: the list may need reading */
    ink_db_t *db = ink_db_open(&DEVICES, NULL);
    char *out = osal_malloc(2048, OSAL_MEM_EXTERNAL);
    char *buf = osal_malloc(128, OSAL_MEM_EXTERNAL);
    if (!db || !out || !buf) fail(c, INK_RC_FAILED, "out of memory");
    else {
        ink_jw_t w;
        ink_jw_init(&w, out, 2048);
        ink_jw_raw(&w, "[");
        ink_db_cursor_t cur = {0};
        ink_rec_t r;
        bool first = true;
        while (ink_db_next(db, &cur, &r, buf, 127) == 1) {
            buf[r.len < 127 ? r.len : 127] = 0;
            if (!first) ink_jw_raw(&w, ",");
            first = false;
            ink_jw_raw(&w, buf);
        }
        ink_jw_raw(&w, "]");
        if (w.overflow) fail(c, INK_RC_FAILED, "too long");
        else ink_cap_result(c, out);
    }
    osal_free(buf);
    osal_free(out);
    ink_db_close(db);
}

static void cap_pair(ink_call_t *c) { /* exec.bg */
    char addr[18], name[32] = "";
    ink_args_get_str(&c->args, "addr", addr, sizeof addr);
    ink_args_get_str(&c->args, "name", name, sizeof name);
    if (!usable(c)) return;
    ink_power_hold(INK_NEED_AWAKE); /* the chip does not sleep in the middle of it */
    int paired = hal_bt_pair(addr);
    ink_power_release(INK_NEED_AWAKE);
    if (paired != 0) {
        fail(c, INK_RC_FAILED, "it did not pair: is it on and looking?");
        return;
    }
    ink_db_t *db = ink_db_open(&DEVICES, NULL);
    if (db) {
        char buf[96];
        ink_jw_t w;
        ink_jw_init(&w, buf, sizeof buf);
        ink_jw_raw(&w, "{\"addr\":");
        ink_jw_str(&w, addr);
        ink_jw_raw(&w, ",\"name\":");
        ink_jw_str(&w, name);
        ink_jw_raw(&w, "}");
        ink_db_write_t rec = {.id = addr_id(addr), .data = buf, .len = w.len, .src = (uint8_t)c->src};
        if (!w.overflow) ink_db_put(db, &rec, NULL);
        ink_db_close(db);
    }
    told("paired");
}

static void cap_connect(ink_call_t *c) { /* exec.bg */
    char addr[18];
    ink_args_get_str(&c->args, "addr", addr, sizeof addr);
    if (!usable(c)) return;
    ink_power_hold(INK_NEED_AWAKE);
    int linked = hal_bt_connect(addr);
    ink_power_release(INK_NEED_AWAKE);
    if (linked != 0) {
        fail(c, INK_RC_FAILED, "it did not connect: is it on, near and paired?");
        return;
    }
    set_connected(addr);
    snprintf(c->rc.say, sizeof c->rc.say, "耳机连上了");
}

static void cap_disconnect(ink_call_t *c) { /* exec.bg */
    if (!usable(c)) return;
    hal_bt_disconnect();
    set_connected(NULL);
}

static void cap_forget(ink_call_t *c) { /* exec.bg */
    char addr[18];
    ink_args_get_str(&c->args, "addr", addr, sizeof addr);
    if (!usable(c)) return;
    char current[18];
    ink_svc_lock();
    snprintf(current, sizeof current, "%s", BT.current);
    ink_svc_unlock();
    hal_bt_forget(addr);
    if (!strcmp(current, addr)) set_connected(NULL);
    ink_db_t *db = ink_db_open(&DEVICES, NULL);
    if (db) {
        ink_db_del(db, addr_id(addr), (uint8_t)c->src);
        ink_db_close(db);
    }
    told("forgotten");
}

#define ADDR_ARG "{\"type\":\"object\",\"properties\":{\"addr\":{\"type\":\"string\",\"maxLength\":17}},\"required\":[\"addr\"]}"

static const ink_cap_t BT_CAPS[] = {
    {.id = "bt.scan", .title = "找附近的蓝牙耳机（要几秒）", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .timeout_ms = 10000, .exec = INK_EXEC_BG, .run = cap_scan},
    {.id = "bt.list", .title = "配对过的耳机", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .run = cap_list},
    {.id = "bt.pair", .title = "配对一副耳机", .input = "{\"type\":\"object\",\"properties\":{\"addr\":{\"type\":\"string\",\"maxLength\":17},\"name\":{\"type\":\"string\",\"maxLength\":31}},\"required\":[\"addr\"]}",
     .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .timeout_ms = 20000, .exec = INK_EXEC_BG, .run = cap_pair},
    {.id = "bt.connect", .title = "连上耳机，声音走耳机", .input = ADDR_ARG, .effect = INK_FX_NAVIGATE, .child_ok = true, .timeout_ms = 20000, .exec = INK_EXEC_BG, .run = cap_connect},
    {.id = "bt.disconnect", .title = "断开耳机，声音回到外放", .effect = INK_FX_NAVIGATE, .child_ok = true, .exec = INK_EXEC_BG, .run = cap_disconnect},
    {.id = "bt.forget", .title = "忘掉一副耳机", .input = ADDR_ARG, .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = cap_forget},
    {.id = "bt.changed", .title = "蓝牙耳机的情况变了", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

static const ink_state_def_t ST_AVAILABLE = {.id = "bt.available", .type = INK_ST_BOOL, .title = "这台设备能用蓝牙耳机"};
static const ink_state_def_t ST_CONNECTED = {.id = "bt.connected", .type = INK_ST_BOOL, .title = "耳机连着"};
static const ink_state_def_t ST_DEVICE = {.id = "bt.device", .type = INK_ST_STR, .title = "连着的耳机"};

int ink_bt_service_init(void) {
    BT.h_available = ink_state_define(&ST_AVAILABLE);
    BT.h_connected = ink_state_define(&ST_CONNECTED);
    BT.h_device = ink_state_define(&ST_DEVICE);
    ink_state_set_bool(BT.h_available, hal_bt_available());
    ink_state_set_bool(BT.h_connected, false);
    ink_state_set_str(BT.h_device, "");
    hal_bt_on_event(on_link, NULL);
    if (ink_db_register(&DEVICES) != 0) return -1;
    return ink_cap_register(BT_CAPS, (int)(sizeof BT_CAPS / sizeof BT_CAPS[0]));
}
