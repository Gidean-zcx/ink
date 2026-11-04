/*
 * System upgrades (see ink/sys/update.h). The package sits on the card; everything about it is checked there first: the
 * signature over its header, the product, the version, each section's hash. Installing takes the application section to the
 * other slot a block at a time through the storage task. What survives a restart is a small file: what is being tried, what
 * did not work, which resources are still to be written.
 *
 * A power cut anywhere: the old slot has not been touched until the boot flag changes, the package is still on the card, and
 * this file says to start over. A new system that does not prove itself in a minute goes back by itself.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_device.h"
#include "ink/hal_ota.h"
#include "ink/hal_power.h"
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/audio.h"
#include "ink/sys/cap.h"
#include "ink/sys/data.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/file.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/media.h"
#include "ink/sys/notify.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/settings.h"
#include "ink/sys/sha256.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/sys/svc.h"
#include "ink/sys/update.h"

#define HEADER_MAX 4096
#define MAX_SECTIONS 6
#define BLOCK 4096
#define MIN_BATTERY 30
#define DOWNLOAD_CHUNK (16 * 1024)
#define DOWNLOAD_PAUSE_BATTERY 15
#define DOWNLOAD_MAX_ERRORS 30
#define WAIT_FOR_ANSWER_US 20000u
#define WAIT_FOR_NET_US 1000000u
#define LOOK_EVERY_US (24ull * 3600 * 1000000)
#define SELF_CHECK_US 60000000ull
#define MAX_SELF_CHECKS 4
#define UPDATE_DIR "/content/.update"
#define CARD_DIR "/content/update"
#define PKG_PATH_MAX 160
#define STATE_FILE "/user/sys/update.json"

enum { SEC_APP = 0, SEC_FONT = 1, SEC_RES = 2 };

typedef struct __attribute__((packed)) {
    uint32_t kind, reserved;
    uint64_t offset, size;
    uint8_t sha256[32];
} sec_t;

typedef struct {
    char version[24], product[32];
    uint32_t code, min_from, flags, nsec, key_id;
    sec_t sec[MAX_SECTIONS];
} pkg_t;

typedef enum {
    PKG_OK = 0, PKG_NO_FILE, PKG_BAD_FORMAT, PKG_BAD_SIGNATURE, PKG_WRONG_PRODUCT, PKG_OLDER, PKG_TOO_OLD_TO_UPGRADE, PKG_BAD_SECTION, PKG_TOO_BIG, PKG_BLOCKED
} pkg_err_t;

static const char *const PKG_ERR[] = {"ok", "no such file", "not an upgrade package", "the signature does not hold", "for another product", "not newer than what runs", "this system is too old for it", "damaged", "too large", "it failed before"};

typedef struct {
    char state[12];   /* "", "writing", "trying", "resources" */
    char version[24];
    uint32_t code;
    char path[PKG_PATH_MAX];
    uint32_t res_done_section;
    uint32_t blocked_code;
} stored_t;

static struct {
    hal_ota_info_t me;
    const char *product;
    stored_t st;
    int h_state, h_version, h_progress;
    /* what the hub offers */
    char offer_version[24];
    uint32_t offer_code;
    uint32_t offer_size;
    uint64_t looked_at_us;
    /* what the card offers: told once */
    uint32_t card_told_code;
    /* downloading */
    int job;
    uint32_t dl_off;
    char dl_final[64], dl_path[72];
    volatile int dl_waiting;     /* 0 nothing out, 1 asked, 2 answered */
    uint8_t *dl_buf;
    volatile size_t dl_got;
    volatile bool dl_failed;
    int dl_errors;
    /* the new system proves itself */
    bool (*checks[MAX_SELF_CHECKS])(void);
    int nchecks;
    ink_exec_job_t self_job, rest_job, scan_job;
    ink_svc_deadline_t self_at;
    bool boot_done;
} U INK_PSRAM_BSS;

static void set_state(const char *s) { ink_state_set_str(U.h_state, s); }

static void fail(ink_call_t *c, ink_rc_t st, const char *why) {
    c->rc.status = st;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
}

/* ---- what survives a restart ---- */

static void save_state(void) {
    char buf[320];
    ink_jw_t w;
    ink_jw_init(&w, buf, sizeof buf);
    ink_jw_raw(&w, "{\"state\":");
    ink_jw_str(&w, U.st.state);
    ink_jw_raw(&w, ",\"version\":");
    ink_jw_str(&w, U.st.version);
    ink_jw_raw(&w, ",\"code\":");
    ink_jw_int(&w, U.st.code);
    ink_jw_raw(&w, ",\"path\":");
    ink_jw_str(&w, U.st.path);
    ink_jw_raw(&w, ",\"res\":");
    ink_jw_int(&w, U.st.res_done_section);
    ink_jw_raw(&w, ",\"blocked\":");
    ink_jw_int(&w, U.st.blocked_code);
    ink_jw_raw(&w, "}");
    if (!w.overflow) ink_storage_write_atomic(STATE_FILE, buf, w.len);
}

static void load_state(void) {
    char *buf = osal_malloc(512, OSAL_MEM_EXTERNAL);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int n = buf && j ? ink_storage_read(STATE_FILE, buf, 511) : -1;
    if (n > 0 && ink_json_parse(j, buf, (size_t)n) == 0) {
        int64_t v = 0;
        ink_json_str(j, ink_json_find(j, 0, "state"), U.st.state, sizeof U.st.state);
        ink_json_str(j, ink_json_find(j, 0, "version"), U.st.version, sizeof U.st.version);
        ink_json_str(j, ink_json_find(j, 0, "path"), U.st.path, sizeof U.st.path);
        if (ink_json_int(j, ink_json_find(j, 0, "code"), &v)) U.st.code = (uint32_t)v;
        if (ink_json_int(j, ink_json_find(j, 0, "res"), &v)) U.st.res_done_section = (uint32_t)v;
        if (ink_json_int(j, ink_json_find(j, 0, "blocked"), &v)) U.st.blocked_code = (uint32_t)v;
    }
    osal_free(j);
    osal_free(buf);
}

/* ---- the package ---- */

/* All of the header and the signature over it. */
static pkg_err_t read_header(const char *path, pkg_t *p) {
    hal_file_t *f = hal_fs_open(path, HAL_O_READ);
    if (!f) return PKG_NO_FILE;
    uint8_t *h = osal_malloc(HEADER_MAX, OSAL_MEM_EXTERNAL);
    pkg_err_t e = PKG_BAD_FORMAT;
    ssize_t n = h ? hal_fs_read(f, h, HEADER_MAX) : -1;
    hal_fs_close(f);
    if (n >= 8 + 8 + 32 + 24 + 16 + 4 + 64 && !memcmp(h, "INKFW1\0\0", 8)) {
        uint32_t format, size;
        memcpy(&format, h + 8, 4);
        memcpy(&size, h + 12, 4);
        const size_t fixed = 16 + 32 + 24 + 16;
        if (format == 1 && size <= HEADER_MAX && (ssize_t)size <= n && size >= fixed + 4 + 64) {
            memcpy(p->product, h + 16, 32);
            memcpy(p->version, h + 48, 24);
            p->product[sizeof p->product - 1] = p->version[sizeof p->version - 1] = 0;
            memcpy(&p->code, h + 72, 4);
            memcpy(&p->min_from, h + 76, 4);
            memcpy(&p->flags, h + 80, 4);
            memcpy(&p->nsec, h + 84, 4);
            if (p->nsec >= 1 && p->nsec <= MAX_SECTIONS && fixed + p->nsec * sizeof(sec_t) + 4 + 64 == size) {
                memcpy(p->sec, h + fixed, p->nsec * sizeof(sec_t));
                memcpy(&p->key_id, h + fixed + p->nsec * sizeof(sec_t), 4);
                uint8_t sig[64], digest[32];
                memcpy(sig, h + size - 64, 64);
                memset(h + size - 64, 0, 64);
                ink_sha256(h, size, digest);
                e = hal_crypto_verify_fw(digest, sig, sizeof sig, p->key_id) ? PKG_OK : PKG_BAD_SIGNATURE;
            }
        }
    }
    osal_free(h);
    return e;
}

static pkg_err_t check_rules(const pkg_t *p) {
    if (strcmp(p->product, U.product)) return PKG_WRONG_PRODUCT;
    if (p->code == U.st.blocked_code) return PKG_BLOCKED;
    if (p->code <= U.me.version_code && !(p->flags & 1)) return PKG_OLDER;
    if (U.me.version_code < p->min_from) return PKG_TOO_OLD_TO_UPGRADE;
    bool has_app = false;
    for (uint32_t i = 0; i < p->nsec; i++)
        if (p->sec[i].kind == SEC_APP) {
            has_app = true;
            if (p->sec[i].size > U.me.slot_size) return PKG_TOO_BIG;
        }
    return has_app ? PKG_OK : PKG_BAD_FORMAT;
}

/* Every section's hash, the whole file read. */
static pkg_err_t check_sections(const char *path, const pkg_t *p) {
    hal_file_t *f = hal_fs_open(path, HAL_O_READ);
    if (!f) return PKG_NO_FILE;
    uint8_t *buf = osal_malloc(BLOCK, OSAL_MEM_EXTERNAL);
    ink_sha256_t *s = osal_malloc(sizeof *s, OSAL_MEM_EXTERNAL);
    pkg_err_t e = buf && s ? PKG_OK : PKG_BAD_SECTION;
    for (uint32_t i = 0; i < p->nsec && e == PKG_OK; i++) {
        uint8_t got[32];
        uint64_t left = p->sec[i].size;
        if (hal_fs_seek(f, (int64_t)p->sec[i].offset, 0) != 0) e = PKG_BAD_SECTION;
        ink_sha256_init(s);
        while (left && e == PKG_OK) {
            ssize_t n = hal_fs_read(f, buf, left < BLOCK ? (size_t)left : BLOCK);
            if (n <= 0) e = PKG_BAD_SECTION;
            else {
                ink_sha256_update(s, buf, (size_t)n);
                left -= (uint64_t)n;
                osal_preempt_point();
            }
        }
        if (e == PKG_OK) {
            ink_sha256_final(s, got);
            if (memcmp(got, p->sec[i].sha256, 32)) e = PKG_BAD_SECTION;
        }
    }
    hal_fs_close(f);
    osal_free(s);
    osal_free(buf);
    return e;
}

static pkg_err_t check_package(const char *path, pkg_t *p) {
    pkg_err_t e = read_header(path, p);
    if (e == PKG_OK) e = check_rules(p);
    if (e == PKG_OK) e = check_sections(path, p);
    return e;
}

/* ---- installing ---- */

/* All of it touches the flash, so all of it in the storage task. */
typedef struct {
    const void *data;
    size_t n;
    int r;
} slot_op_t;

static void slot_begin(void *arg) {
    slot_op_t *o = arg;
    o->r = hal_ota_begin(o->n);
}

static void slot_write(void *arg) {
    slot_op_t *o = arg;
    o->r = hal_ota_write(o->data, o->n);
}

static void slot_finish(void *arg) { /* whole and well formed: the next boot is from there */
    slot_op_t *o = arg;
    o->r = hal_ota_end() == 0 && hal_ota_set_boot() == 0 ? 0 : -1;
}

static void slot_valid(void *arg) {
    (void)arg;
    hal_ota_mark_valid();
}

typedef struct {
    int kind;
    size_t offset, n;
    const void *data;
} res_op_t;

static void res_write(void *arg) {
    res_op_t *o = arg;
    hal_ota_write_resource(o->kind, o->offset, o->data, o->n);
}

static void never_back(void) { /* hal_power_reboot does not come back on the device */
    for (;;) osal_task_delay_us(1000000);
}

/* What stands in the way, in words. */
static const char *not_now(void) {
    if (ink_audio_focus_top() || ink_audio_recording()) return "a call, a recording or something being said comes first";
    ink_media_status_t m;
    ink_media_status(&m);
    if (m.state == INK_MEDIA_PLAYING) return "something is playing";
    hal_battery_t b; /* as it is now: someone who just plugged it in does not wait for the next look */
    hal_power_battery(&b);
    if (b.percent < MIN_BATTERY && !b.charging && !b.external_power) return "the battery is too low: charge it to 30%";
    if (!hal_fs_card_present()) return "no card";
    return NULL;
}

static int install(const char *path, const pkg_t *p) { /* exec.bg, holding the card. It returns only if it could not */
    const sec_t *app = NULL;
    for (uint32_t i = 0; i < p->nsec; i++)
        if (p->sec[i].kind == SEC_APP) app = &p->sec[i];
    hal_file_t *f = hal_fs_open(path, HAL_O_READ);
    uint8_t *buf = osal_malloc(BLOCK, OSAL_MEM_EXTERNAL);
    slot_op_t op = {.n = (size_t)app->size, .r = -1};
    if (f && buf) ink_storage_run(slot_begin, &op, true);
    if (!f || !buf || op.r != 0 || hal_fs_seek(f, (int64_t)app->offset, 0) != 0) {
        if (f) hal_fs_close(f);
        osal_free(buf);
        return -1;
    }
    snprintf(U.st.state, sizeof U.st.state, "writing");
    snprintf(U.st.version, sizeof U.st.version, "%s", p->version);
    U.st.code = p->code;
    snprintf(U.st.path, sizeof U.st.path, "%s", path);
    U.st.res_done_section = 0;
    save_state();
    set_state("installing");
    ink_power_hold(INK_NEED_AWAKE);
    ink_power_hold(INK_NEED_NO_LOCK);
    uint64_t done = 0;
    int bad = 0;
    int last_pct = -1;
    while (done < app->size && !bad) {
        size_t want = app->size - done < BLOCK ? (size_t)(app->size - done) : BLOCK;
        ssize_t n = hal_fs_read(f, buf, want);
        if (n <= 0) bad = 1;
        else {
            op = (slot_op_t){buf, (size_t)n, 0};
            ink_storage_run(slot_write, &op, true); /* between blocks the panel and the readers get their turn */
            if (op.r != 0) bad = 1;
            done += (uint64_t)n;
            int pct = (int)(done * 100 / app->size);
            if (pct != last_pct) {
                last_pct = pct;
                ink_state_set_int(U.h_progress, pct);
            }
        }
    }
    hal_fs_close(f);
    osal_free(buf);
    if (!bad) ink_storage_run(slot_finish, &op, true);
    if (bad || op.r != 0) {
        ink_power_release(INK_NEED_NO_LOCK);
        ink_power_release(INK_NEED_AWAKE);
        U.st.state[0] = 0;
        save_state();
        set_state("failed");
        ink_probe_count("update.write_failed");
        return -1;
    }
    snprintf(U.st.state, sizeof U.st.state, "trying");
    save_state();
    ink_db_flush_all();
    for (int i = 0; i < 200 && (!ink_db_idle() || ink_storage_pending()); i++) osal_task_delay_us(50000);
    set_state("rebooting");
    ink_probe_count("update.installed");
    hal_power_reboot();
    never_back();
    return 0;
}

/* Installing from a path: the whole check again, nothing is taken on trust. Holding the card. */
static void install_cap(ink_call_t *c, const char *path) {
    const char *why = not_now();
    if (why) {
        fail(c, INK_RC_FAILED, why);
        return;
    }
    pkg_t *p = osal_malloc(sizeof *p, OSAL_MEM_EXTERNAL);
    if (!p) {
        fail(c, INK_RC_FAILED, "out of memory");
        return;
    }
    set_state("verifying");
    pkg_err_t e = check_package(path, p);
    if (e != PKG_OK) {
        set_state("idle");
        ink_probe_count("update.package_refused");
        fail(c, INK_RC_FAILED, PKG_ERR[e]);
    } else if (install(path, p) != 0) fail(c, INK_RC_FAILED, "it could not be written: the old system stays");
    osal_free(p);
}

/* ---- looking for packages ---- */

/* The newest good package in this folder: its path. */
static bool newest_in(const char *dir, char *best, size_t cap, pkg_t *out) {
    hal_dir_t *d = hal_fs_opendir(dir);
    if (!d) return false;
    pkg_t *p = osal_malloc(sizeof *p, OSAL_MEM_EXTERNAL);
    char *path = osal_malloc(PKG_PATH_MAX, OSAL_MEM_EXTERNAL);
    hal_dirent_t e;
    bool found = false;
    while (p && path && hal_fs_readdir(d, &e) == 1) {
        size_t n = strlen(e.name);
        if (e.is_dir || n < 7 || strcmp(e.name + n - 6, ".inkfw")) continue;
        snprintf(path, PKG_PATH_MAX, "%s/%s", dir, e.name);
        if (read_header(path, p) != PKG_OK || check_rules(p) != PKG_OK) {
            ink_probe_count("update.package_ignored");
            continue;
        }
        if (found && p->code <= out->code) continue;
        if (check_sections(path, p) != PKG_OK) { /* what is offered can be installed; this costs a read of it, only for what would be chosen */
            ink_probe_count("update.package_ignored");
            continue;
        }
        *out = *p;
        snprintf(best, cap, "%s", path);
        found = true;
    }
    hal_fs_closedir(d);
    osal_free(path);
    osal_free(p);
    return found;
}

static void offer_notification(const char *version, const char *action) {
    char body[64];
    snprintf(body, sizeof body, "可以升级到 %s", version);
    ink_notify_t n = {.channel = "system", .title = "发现系统更新", .body = body, .tag = "update", .action = action, .prio = INK_NOTIFY_NORMAL, .src = INK_SRC_SYSTEM};
    ink_notify_post(&n, NULL);
}

static void scan_job(void *arg) { /* exec.bg: the card, once the file service is up */
    (void)arg;
    if (ink_fs_card_hold() != INK_FS_OK) return;
    pkg_t *p = osal_malloc(sizeof *p, OSAL_MEM_EXTERNAL);
    char *path = osal_malloc(PKG_PATH_MAX, OSAL_MEM_EXTERNAL);
    bool found = p && path && newest_in(CARD_DIR, path, PKG_PATH_MAX, p);
    ink_fs_card_release();
    if (found && p->code != U.card_told_code) {
        U.card_told_code = p->code;
        offer_notification(p->version, "system.upgrade_from_card");
        ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
        if (a) {
            ink_args_init(a);
            ink_args_str(a, "version", p->version);
            ink_args_str(a, "from", "card");
            ink_cap_emit("update.available", a);
            osal_free(a);
        }
    }
    osal_free(path);
    osal_free(p);
}

static void on_card_changed(void *ctx, const char *id, const ink_args_t *a) { /* exec.bg */
    (void)ctx;
    (void)id;
    bool lent = true;
    ink_args_get_bool(a, "lent", &lent);
    if (!lent) ink_exec_post(INK_EXEC_BG, &U.scan_job, scan_job, NULL);
}

static void on_card_state(void *ctx, int h, const char *id) { /* wherever it was seen. The first look is the boot's */
    (void)ctx;
    (void)id;
    int64_t there = 0;
    ink_state_get_int(h, &there);
    if (there && U.boot_done) ink_exec_post(INK_EXEC_BG, &U.scan_job, scan_job, NULL);
}

/* ---- from the hub ---- */

static const char *start_download(void);

typedef struct {
    ink_exec_job_t job;
    ink_call_t *call; /* NULL: the look once a day */
    int status;
    char version[24];
    uint32_t code, size;
} answer_t;

static bool is_newer(uint32_t code) { return code > U.me.version_code && code != U.st.blocked_code; }

static bool on_wifi(void) {
    char type[8] = "";
    ink_state_get_str(ink_state_find("net.type"), type, sizeof type);
    return !strcmp(type, "wifi");
}

static void check_answered(void *arg) { /* exec.bg: the notification waits for the flash */
    answer_t *a = arg;
    if (a->call) {
        if (a->status != INK_HUB_OK) fail(a->call, INK_RC_FAILED, "the hub cannot be reached");
        else {
            char out[128];
            snprintf(out, sizeof out, "{\"current\":\"%s\",\"latest\":\"%.23s\",\"newer\":%s,\"size\":%u}", U.me.version, a->version, is_newer(a->code) ? "true" : "false", (unsigned)a->size);
            ink_cap_result(a->call, out);
        }
        ink_cap_done(a->call);
    }
    if (a->status == INK_HUB_OK && is_newer(a->code)) {
        bool first = a->code != U.offer_code;
        ink_svc_lock();
        snprintf(U.offer_version, sizeof U.offer_version, "%s", a->version);
        U.offer_code = a->code;
        U.offer_size = a->size;
        ink_svc_unlock();
        if (first) {
            if (!U.job) set_state("available");
            offer_notification(a->version, "update.download");
            ink_args_t *ev = osal_malloc(sizeof *ev, OSAL_MEM_EXTERNAL);
            if (ev) {
                ink_args_init(ev);
                ink_args_str(ev, "version", a->version);
                ink_args_str(ev, "from", "hub");
                ink_cap_emit("update.available", ev);
                osal_free(ev);
            }
            /* mobile data costs money: only Wi-Fi by itself */
            if (ink_setting_get("update.auto_download") && on_wifi()) start_download();
        }
    }
    osal_free(a);
}

static void on_check_reply(void *ctx, int status, const ink_json_t *j, int result) { /* hub task */
    ink_call_t *c = ctx;
    int64_t code = 0, size = 0;
    if (status == INK_HUB_OK) {
        ink_json_int(j, ink_json_find(j, result, "version_code"), &code);
        ink_json_int(j, ink_json_find(j, result, "size"), &size);
    }
    if (!c && !is_newer((uint32_t)code)) return; /* the usual answer to the daily look: nothing wakes for it */
    answer_t *a = osal_calloc(1, sizeof *a, OSAL_MEM_EXTERNAL);
    if (a) {
        a->call = c;
        a->status = status;
        a->code = (uint32_t)code;
        a->size = (uint32_t)size;
        if (status == INK_HUB_OK) ink_json_str(j, ink_json_find(j, result, "version"), a->version, sizeof a->version);
    }
    if (!a || ink_exec_post(INK_EXEC_BG, &a->job, check_answered, a) != 0) {
        osal_free(a);
        if (c) {
            fail(c, INK_RC_FAILED, "out of memory");
            ink_cap_done(c);
        }
    }
}

static void cap_check(ink_call_t *c) {
    ink_cap_defer(c);
    ink_hub_demand();
    if (ink_hub_call("update/check", "{}", 20000000, on_check_reply, c) < 0) {
        fail(c, INK_RC_FAILED, "the hub cannot be reached");
        ink_cap_done(c);
    }
}

static void on_online(void *ctx) { /* hub task: a look now and then, when it is online anyway */
    (void)ctx;
    uint64_t now = osal_now_us();
    if (U.looked_at_us && now < U.looked_at_us + LOOK_EVERY_US) return;
    U.looked_at_us = now;
    ink_hub_call("update/check", "{}", 20000000, on_check_reply, NULL);
}

static int b64_value(char ch) {
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9') return ch - '0' + 52;
    if (ch == '+') return 62;
    if (ch == '/') return 63;
    return -1;
}

static size_t b64_decode(const char *s, size_t n, uint8_t *out, size_t cap) {
    uint32_t acc = 0;
    int bits = 0;
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        int v = b64_value(s[i]);
        if (v < 0) continue;
        acc = acc << 6 | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o < cap) out[o++] = (uint8_t)(acc >> bits);
        }
    }
    return o;
}

static void on_chunk(void *ctx, int status, const ink_json_t *j, int result) { /* hub task */
    (void)ctx;
    U.dl_got = 0;
    if (status == INK_HUB_OK) {
        int t = ink_json_find(j, result, "data");
        if (t >= 0) U.dl_got = b64_decode(j->text + j->tok[t].start, (size_t)(j->tok[t].end - j->tok[t].start), U.dl_buf, DOWNLOAD_CHUNK);
    }
    __atomic_store_n(&U.dl_waiting, 2, __ATOMIC_RELEASE);
}

/* Whole: it has to be a package for this device, and what its signature says. */
static ink_job_step_t dl_check(void) { /* exec.bg */
    if (ink_fs_card_hold() != INK_FS_OK) {
        osal_task_delay_us(WAIT_FOR_NET_US);
        return INK_JOB_MORE;
    }
    pkg_t *p = osal_malloc(sizeof *p, OSAL_MEM_EXTERNAL);
    pkg_err_t e = p ? check_package(U.dl_path, p) : PKG_BAD_FORMAT;
    osal_free(p);
    if (e != PKG_OK) {
        hal_fs_remove(U.dl_path);
        ink_fs_card_release();
        ink_probe_count("update.download_bad");
        return INK_JOB_FAILED;
    }
    hal_fs_remove(U.dl_final);
    hal_fs_rename(U.dl_path, U.dl_final);
    ink_fs_card_release();
    return INK_JOB_DONE;
}

/* A piece at a time, and the card held only for as long as a piece takes: it can be lent in between. */
static ink_job_step_t dl_step(void *ctx) { /* exec.bg: realtime sessions first (the job system holds jobs during them) */
    (void)ctx;
    int w = __atomic_load_n(&U.dl_waiting, __ATOMIC_ACQUIRE);
    if (w == 1) {
        osal_task_delay_us(WAIT_FOR_ANSWER_US);
        return INK_JOB_MORE;
    }
    if (w == 2) {
        if (U.dl_got) {
            if (ink_fs_card_hold() != INK_FS_OK) {
                osal_task_delay_us(WAIT_FOR_NET_US);
                return INK_JOB_MORE; /* it waits with the piece */
            }
            hal_file_t *f = hal_fs_open(U.dl_path, HAL_O_WRITE | HAL_O_CREATE | HAL_O_APPEND);
            ssize_t n = f ? hal_fs_write(f, U.dl_buf, U.dl_got) : -1;
            if (f) hal_fs_close(f);
            ink_fs_card_release();
            if (n != (ssize_t)U.dl_got) return INK_JOB_FAILED;
            U.dl_off += (uint32_t)U.dl_got;
            U.dl_errors = 0;
            ink_state_set_int(U.h_progress, (int64_t)((uint64_t)U.dl_off * 100 / (U.offer_size ? U.offer_size : 1)));
        } else if (++U.dl_errors > DOWNLOAD_MAX_ERRORS) { /* the link went and came back is one thing; this is not that */
            return INK_JOB_FAILED;
        } else osal_task_delay_us(WAIT_FOR_NET_US);
        __atomic_store_n(&U.dl_waiting, 0, __ATOMIC_RELEASE);
    }
    if (U.dl_off >= U.offer_size) return dl_check();
    hal_battery_t b;
    hal_power_battery(&b);
    if (!ink_hub_online() || (b.percent < DOWNLOAD_PAUSE_BATTERY && !b.charging && !b.external_power)) { /* until it is back, or charged */
        ink_hub_demand();
        osal_task_delay_us(WAIT_FOR_NET_US);
        return INK_JOB_MORE;
    }
    char params[64];
    uint32_t left = U.offer_size - U.dl_off;
    snprintf(params, sizeof params, "{\"offset\":%u,\"len\":%u}", (unsigned)U.dl_off, (unsigned)(left < DOWNLOAD_CHUNK * 3 / 4 ? left : DOWNLOAD_CHUNK * 3 / 4));
    __atomic_store_n(&U.dl_waiting, 1, __ATOMIC_RELEASE);
    if (ink_hub_call("update/fetch", params, 20000000, on_chunk, NULL) < 0) __atomic_store_n(&U.dl_waiting, 0, __ATOMIC_RELEASE);
    return INK_JOB_MORE;
}

static void dl_finish(void *ctx, ink_job_step_t how, bool cancelled) { /* exec.bg */
    (void)ctx;
    ink_power_release(INK_NEED_NET);
    osal_free(U.dl_buf);
    U.dl_buf = NULL;
    U.job = 0;
    if (how != INK_JOB_DONE || cancelled) {
        set_state(cancelled ? "available" : "failed");
        ink_probe_count("update.download_failed");
        return;
    }
    set_state("ready");
    ink_probe_count("update.downloaded");
    char body[64];
    snprintf(body, sizeof body, "%s 下载好了，可以安装", U.offer_version);
    ink_notify_t n = {.channel = "download", .title = "系统更新", .body = body, .tag = "update", .action = "system.upgrade", .prio = INK_NOTIFY_NORMAL, .src = INK_SRC_SYSTEM};
    ink_notify_post(&n, NULL);
}

/* NULL, or why not. exec.bg */
static const char *start_download(void) {
    if (!U.offer_code) return "nothing to download: ask update.check first";
    if (U.job) return "already downloading";
    int held = ink_fs_card_hold();
    if (held != INK_FS_OK) return held == INK_FS_ERR_USB ? "the card is lent to a computer" : "no card: the package waits on the card";
    hal_fs_mkdir(UPDATE_DIR);
    snprintf(U.dl_final, sizeof U.dl_final, "%s/%s.inkfw", UPDATE_DIR, U.offer_version);
    snprintf(U.dl_path, sizeof U.dl_path, "%s.part", U.dl_final);
    int64_t have = hal_fs_size(U.dl_path);
    U.dl_off = have > 0 && have <= U.offer_size ? (uint32_t)have : 0; /* from where it stopped */
    bool ok = true;
    if (!U.dl_off) { /* nothing, or not what this can be */
        hal_file_t *f = hal_fs_open(U.dl_path, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
        if (f) hal_fs_close(f);
        ok = f != NULL;
    }
    ink_fs_card_release();
    U.dl_buf = ok ? osal_malloc(DOWNLOAD_CHUNK, OSAL_MEM_EXTERNAL) : NULL;
    if (!U.dl_buf) return "the card cannot be written";
    U.dl_waiting = 0;
    U.dl_got = 0;
    U.dl_errors = 0;
    ink_job_t j = {.name = "update.download", .step = dl_step, .finish = dl_finish, .step_budget_ms = 50};
    U.job = ink_job_submit(&j);
    if (U.job < 0) {
        U.job = 0;
        osal_free(U.dl_buf);
        U.dl_buf = NULL;
        return "too much going on";
    }
    ink_power_hold(INK_NEED_NET);
    ink_probe_count("update.download_started");
    set_state("downloading");
    return NULL;
}

static void cap_download(ink_call_t *c) { /* exec.bg */
    if (c->src == INK_SRC_AI && !on_wifi()) { /* mobile data costs money: that is the person's to say */
        fail(c, INK_RC_REJECTED, "on mobile data a person has to start this");
        return;
    }
    const char *why = start_download();
    if (why) fail(c, INK_RC_FAILED, why);
    else snprintf(c->rc.say, sizeof c->rc.say, "开始下载，下好了告诉你");
}

static void cap_status(ink_call_t *c) {
    char out[240], st[16] = "";
    int64_t pct = 0;
    ink_state_get_str(U.h_state, st, sizeof st);
    ink_state_get_int(U.h_progress, &pct);
    ink_svc_lock();
    snprintf(out, sizeof out, "{\"state\":\"%s\",\"version\":\"%s\",\"code\":%u,\"progress\":%d,\"offered\":\"%s\"}", st, U.me.version, (unsigned)U.me.version_code, (int)pct, U.offer_version);
    ink_svc_unlock();
    ink_cap_result(c, out);
}

static void cap_upgrade(ink_call_t *c) { /* exec.bg: what was downloaded */
    int held = ink_fs_card_hold();
    if (held != INK_FS_OK) {
        fail(c, INK_RC_FAILED, held == INK_FS_ERR_USB ? "the card is lent to a computer" : "no card");
        return;
    }
    char *path = osal_malloc(PKG_PATH_MAX, OSAL_MEM_EXTERNAL);
    pkg_t *p = osal_malloc(sizeof *p, OSAL_MEM_EXTERNAL);
    if (path && p && newest_in(UPDATE_DIR, path, PKG_PATH_MAX, p)) install_cap(c, path);
    else fail(c, INK_RC_FAILED, "nothing downloaded");
    osal_free(p);
    osal_free(path);
    ink_fs_card_release();
}

static void cap_upgrade_card(ink_call_t *c) { /* exec.bg */
    int held = ink_fs_card_hold();
    if (held != INK_FS_OK) {
        fail(c, INK_RC_FAILED, held == INK_FS_ERR_USB ? "the card is lent to a computer" : "no card");
        return;
    }
    char *path = osal_malloc(PKG_PATH_MAX, OSAL_MEM_EXTERNAL);
    pkg_t *p = osal_malloc(sizeof *p, OSAL_MEM_EXTERNAL);
    char file[64] = "";
    if (path && p && ink_args_get_str(&c->args, "file", file, sizeof file) > 0) {
        if (strchr(file, '/') || file[0] == '.') fail(c, INK_RC_INVALID, "a file in the update folder");
        else {
            snprintf(path, PKG_PATH_MAX, "%s/%s", CARD_DIR, file);
            install_cap(c, path);
        }
    } else if (path && p && newest_in(CARD_DIR, path, PKG_PATH_MAX, p)) install_cap(c, path);
    else fail(c, INK_RC_FAILED, "no package on the card that can be installed");
    osal_free(p);
    osal_free(path);
    ink_fs_card_release();
}

/* ---- the new system proves itself ---- */

static void told_result(bool ok, const char *version, const char *why) {
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (a) {
        ink_args_init(a);
        ink_args_bool(a, "ok", ok);
        ink_args_str(a, "version", version);
        if (why) ink_args_str(a, "why", why);
        ink_cap_emit("update.result", a);
        osal_free(a);
    }
    char body[80];
    snprintf(body, sizeof body, ok ? "现在是 %s" : "%s 没装成，还是原来的系统", version);
    ink_notify_t n = {.channel = "system", .title = ok ? "系统升级好了" : "系统升级失败", .body = body, .tag = "update.result", .prio = ok ? INK_NOTIFY_NORMAL : INK_NOTIFY_HIGH, .src = INK_SRC_SYSTEM};
    ink_notify_post(&n, NULL);
}

/* What was not in the slot: fonts and resources, after the system has proved itself. From where a power cut left it. */
static bool write_resources(const char *path) { /* exec.bg, holding the card */
    pkg_t *p = osal_malloc(sizeof *p, OSAL_MEM_EXTERNAL);
    uint8_t *buf = osal_malloc(BLOCK, OSAL_MEM_EXTERNAL);
    hal_file_t *f = path[0] ? hal_fs_open(path, HAL_O_READ) : NULL;
    bool ok = !path[0]; /* a package with nothing more to write */
    if (p && buf && f && read_header(path, p) == PKG_OK) {
        ok = true;
        for (uint32_t i = U.st.res_done_section; i < p->nsec && ok; i++) {
            if (p->sec[i].kind != SEC_APP) {
                uint64_t done = 0;
                hal_fs_seek(f, (int64_t)p->sec[i].offset, 0);
                while (done < p->sec[i].size && ok) {
                    ssize_t n = hal_fs_read(f, buf, p->sec[i].size - done < BLOCK ? (size_t)(p->sec[i].size - done) : BLOCK);
                    if (n <= 0) ok = false;
                    else {
                        res_op_t o = {(int)p->sec[i].kind, (size_t)done, (size_t)n, buf};
                        ink_storage_run(res_write, &o, true);
                        done += (uint64_t)n;
                    }
                }
            }
            if (ok) {
                U.st.res_done_section = i + 1;
                save_state();
            }
        }
    }
    if (f) hal_fs_close(f);
    osal_free(buf);
    osal_free(p);
    return ok;
}

/* It is good: what is left. Again from the start of this after a power cut, what is done stays done. */
static void rest_job(void *arg) { /* exec.bg */
    (void)arg;
    if (ink_fs_card_hold() != INK_FS_OK) return; /* no card: the next boot, with the small built-in font until then */
    bool ok = write_resources(U.st.path);
    if (ok && !strncmp(U.st.path, UPDATE_DIR, strlen(UPDATE_DIR))) hal_fs_remove(U.st.path); /* a package the person put on the card is theirs to delete */
    ink_fs_card_release();
    if (!ok) {
        ink_probe_count("update.resources_failed");
        return;
    }
    U.st.state[0] = 0;
    U.st.path[0] = 0;
    save_state();
}

/* The services are up, the panel has drawn, the data partition can be written, and whoever has more to say about it. If it does
 * not hold: a restart, and the bootloader goes back to what it knows, since this one never said it was good. */
static void self_job(void *arg) { /* exec.bg */
    (void)arg;
    uint64_t end = osal_now_us() + SELF_CHECK_US;
    while ((!ink_display_commits() || ink_display_busy()) && osal_now_us() < end) osal_task_delay_us(100000);
    bool panel_ok = osal_now_us() < end;
    static const char PROBE[] = "self-check";
    char back[16] = "";
    ink_storage_write_atomic("/user/sys/update.probe", PROBE, sizeof PROBE - 1);
    while (ink_storage_pending()) osal_task_delay_us(50000);
    int n = ink_storage_read("/user/sys/update.probe", back, sizeof back - 1);
    ink_storage_remove("/user/sys/update.probe", NULL, NULL);
    bool ok = panel_ok && n == (int)sizeof PROBE - 1 && !memcmp(back, PROBE, sizeof PROBE - 1);
    for (int i = 0; i < U.nchecks && ok; i++) ok = U.checks[i]();
    if (!ok) {
        ink_probe_count("update.self_check_failed");
        hal_power_reboot();
        never_back();
    }
    ink_svc_cancel(&U.self_at);
    ink_storage_run(slot_valid, NULL, true);
    ink_probe_count("update.self_check_passed");
    if (U.st.state[0] && U.st.code == U.me.version_code) {
        snprintf(U.st.state, sizeof U.st.state, "resources");
        save_state();
    }
    set_state("idle");
    told_result(true, U.me.version, NULL);
    rest_job(NULL);
}

static void self_timeout(void *arg) { /* the timer task: a minute and it has not proved itself. No flash from here */
    (void)arg;
    ink_probe_count("update.self_check_timed_out");
    hal_power_reboot();
}

int ink_update_self_check_add(bool (*check)(void)) {
    if (U.nchecks >= MAX_SELF_CHECKS) return -1;
    U.checks[U.nchecks++] = check;
    return 0;
}

void ink_update_boot_check(void) { /* boot task */
    U.boot_done = true;
    if (U.me.pending_verify) {
        ink_svc_at(&U.self_at, osal_now_us() + SELF_CHECK_US, self_timeout, NULL);
        ink_exec_post(INK_EXEC_BG, &U.self_job, self_job, NULL);
        return;
    }
    bool trying = !strcmp(U.st.state, "trying"), writing = !strcmp(U.st.state, "writing");
    if ((trying || writing) && U.st.code != U.me.version_code && (trying || U.me.rolled_back)) { /* it went back */
        ink_probe_count("update.rolled_back");
        U.st.blocked_code = U.st.code;
        char version[24];
        snprintf(version, sizeof version, "%s", U.st.version);
        U.st.state[0] = 0;
        U.st.path[0] = 0;
        save_state();
        told_result(false, version, "it did not prove itself");
    } else if (writing && U.st.code != U.me.version_code) { /* the power went while the slot was being written: the package is still there */
        ink_probe_count("update.interrupted");
        U.st.state[0] = 0;
        save_state();
    } else if (U.st.state[0]) { /* it is good, a power cut came before all of the rest was done */
        ink_exec_post(INK_EXEC_BG, &U.rest_job, rest_job, NULL);
    }
    /* a package on the card? Only if the folder is there: this should not start the executor for nothing */
    if (ink_fs_card_hold() == INK_FS_OK) {
        hal_dir_t *d = hal_fs_opendir(CARD_DIR);
        if (d) {
            hal_fs_closedir(d);
            ink_exec_post(INK_EXEC_BG, &U.scan_job, scan_job, NULL);
        }
        ink_fs_card_release();
    }
}

/* ---- on the bus ---- */

static const ink_cap_t UPDATE_CAPS[] = {
    {.id = "update.status", .title = "系统版本和升级进度", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_status},
    {.id = "update.check", .title = "问中枢有没有新版本", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .qos = INK_QOS_NETWORK, .exec = INK_EXEC_BG, .run = cap_check},
    {.id = "update.download", .title = "把新版本下载到 TF 卡（不安装）", .effect = INK_FX_LOCAL_WRITE, .qos = INK_QOS_NETWORK, .exec = INK_EXEC_BG, .run = cap_download},
    /* a person installs: not even listed for the AI (foundation §6.2; the policy gate has the same rule by name) */
    {.id = "system.upgrade", .title = "安装下载好的新系统（安装时不能用设备，会重启）", .ask = "安装新系统，装完重启", .ask_by_hand = true, .effect = INK_FX_SYSTEM, .expose = INK_EXPOSE_OWNER, .timeout_ms = 600000, .exec = INK_EXEC_BG, .run = cap_upgrade},
    {.id = "system.upgrade_from_card", .title = "安装 TF 卡上的新系统（update 文件夹里，会重启）", .ask = "从 TF 卡安装新系统，装完重启", .ask_by_hand = true,
     .input = "{\"type\":\"object\",\"properties\":{\"file\":{\"type\":\"string\",\"maxLength\":63}}}", .effect = INK_FX_SYSTEM, .expose = INK_EXPOSE_OWNER, .timeout_ms = 600000, .exec = INK_EXEC_BG, .run = cap_upgrade_card},
    {.id = "update.available", .title = "发现了新版本", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
    {.id = "update.result", .title = "升级的结果", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

static const ink_state_def_t ST_STATE = {.id = "update.state", .type = INK_ST_STR, .title = "升级（idle、available、downloading、ready、verifying、installing、rebooting、failed）"};
static const ink_state_def_t ST_VERSION = {.id = "update.version", .type = INK_ST_STR, .title = "系统版本"};
static const ink_state_def_t ST_PROGRESS = {.id = "update.progress", .type = INK_ST_INT, .title = "升级进度（%）"};
static const ink_setting_def_t UPDATE_SETTINGS[] = {
    {.id = "update.auto_download", .title = "自动下载新版本（流量要钱，默认关）", .type = INK_SET_BOOL, .def = 0},
};

int ink_update_init(void) {
    hal_device_info_t di;
    hal_device_get_info(&di);
    U.product = di.profile_id;
    hal_ota_info(&U.me);
    load_state();
    U.h_state = ink_state_define(&ST_STATE);
    U.h_version = ink_state_define(&ST_VERSION);
    U.h_progress = ink_state_define(&ST_PROGRESS);
    ink_state_set_str(U.h_state, "idle");
    ink_state_set_str(U.h_version, U.me.version);
    ink_state_set_int(U.h_progress, 0);
    if (ink_settings_define(UPDATE_SETTINGS, 1) != 0) return -1;
    if (ink_cap_subscribe("usb.changed", on_card_changed, NULL, INK_EXEC_BG) != 0) return -1;
    if (ink_state_watch("storage.card", on_card_state, NULL, INK_EXEC_DIRECT) != 0 || ink_hub_on_online(on_online, NULL) != 0) return -1;
    return ink_cap_register(UPDATE_CAPS, (int)(sizeof UPDATE_CAPS / sizeof UPDATE_CAPS[0]));
}
