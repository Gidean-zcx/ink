/*
 * Remote screen host (architecture v1.0 §9.5, detailed design §8): a system
 * app that shows what a hub capability renders (a browser, a third-party
 * app) and sends back what the person does. One realtime session carries
 * the screen down and the input up:
 *
 *   down  region frames: [x u16][y u16][w u16][h u16][encoding u8][refresh u8] data
 *         (0 raw 4-bit gray, 1 4-bit gray + LZ4, 2 gray JPEG); the last region
 *         of an update has INK_RT_F_LAST: only then the update is shown
 *   up    credits [n u8][max bytes u32]: the device pulls; one is returned
 *         when the panel has shown the last update
 *         input [1 tap][x u16][y u16] (unchanged)
 *               [2 page][direction u8: 0 previous, 1 next]
 *               [3 long press][x u16][y u16]; [4 submit][UTF-8, no NUL]
 *   control rt.nav: blocked=true, or input="text"/"password" opens the local editor
 *
 * Two canvases in PSRAM, 8-bit gray. Regions are decoded on the media executor
 * into `back`, the picture as the hub knows it; when an update is complete the
 * UI task copies what changed to `px`, the only one LVGL reads, and marks those
 * areas with the refresh hint they came with. Regions of the next update wait
 * meanwhile: what is shown is always a whole update, one for each credit. A
 * generation number follows the
 * picture: closing the page, reopening it or a new connection (that hub starts
 * from a white screen) start a new one, and regions of an older one are
 * dropped, also when they are found mid-way.
 * A tap inverts the touched spot of `px` at once (local feedback); the hub's
 * next update, or a timer if none comes, puts the picture back.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/hal_jpeg.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/appreg.h"
#include "ink/sys/members.h"
#include "ink/sys/textfield.h"
#include "ink/ui_shell.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/font.h"
#include "ink/sys/json.h"
#include "ink/sys/lz4.h"
#include "ink/sys/rt.h"
#include "ink/sys/state.h"
#include "ink/sys/ui.h"

#define DIRTY_MAX 32
#define FEEDBACK 24 /* half size of the local tap feedback square (px) */
#define FEEDBACK_MS 2000 /* a tap the hub does not answer with a new picture: the spot goes back */

typedef enum { REMOTE, LOADING, OFFLINE, ADDRESS, TYPE, BLOCKED } remote_page_t;
static void page_show(remote_page_t page);
static void address_open(const char *url);
static struct {
    osal_mutex_t *lock;
    int sid;
    int w, h;
    uint8_t *px;   /* w * h: what LVGL shows. UI task only */
    uint8_t *back; /* w * h: the picture the hub knows. Under the lock */
    uint8_t *tmp;  /* decode scratch: media executor */
    size_t tmp_cap;
    uint32_t gen;  /* the picture's generation (under the lock) */
    bool active;   /* between create and destroy (under the lock): the canvases stay */
    bool repaint;  /* a new picture: the next update shows all of `back` (under the lock) */
    bool holding;  /* an update is complete and waits for the UI task: `back` stays as it is, the next one waits (under the lock) */
    struct {
        lv_area_t a;
        uint8_t hint;
    } dirty[DIRTY_MAX];
    int ndirty;
    int ready_sid; /* the session the hub just confirmed (under the lock) */
    ink_exec_job_t ready_job, show_job, drain_job, release_job, closed_job, restart_job;
    /* UI task */
    lv_image_dsc_t dsc;
    lv_obj_t *img, *note, *chrome, *field;
    remote_page_t page;
    int control_sid, control_kind;
    ink_exec_job_t control_job, focus_job;
    bool secret;
    uint64_t down_us;
    char url[180];
    ink_ui_page_spec_t semantics;
    ink_ui_field_spec_t fields[3];
    ink_ui_action_spec_t actions[8];
    int dpi;
    lv_timer_t *credit_timer, *spot_timer, *link_timer;
    char params[INK_RT_PARAMS]; /* what the session is opened with */
    bool suspended;  /* the device is locked: no session */
    bool ended;      /* the hub refused or ended it */
    bool down_shown; /* the note says the connection is gone */
    int down_ticks;
    lv_area_t spot;
    bool spot_on;
    int owed; /* credits the hub has not been given yet */
    uint64_t opened_us, tap_us;
    bool first_shown, waiting_display;
    bool pulled; /* the hub has been asked for a picture on some connection */
    unsigned updates;
} RS INK_PSRAM_BSS;

static const ink_refresh_t HINTS[] = {INK_REFRESH_FAST, INK_REFRESH_UI, INK_REFRESH_FULL};

/* ------------------------------------------------------------ down: regions */

typedef struct region_job {
    struct region_job *next;
    uint32_t gen; /* the picture it belongs to */
    uint8_t flags;
    size_t len;
    uint8_t data[];
} region_job_t;

/* Regions waiting for the media executor: one job drains them all. */
static region_job_t *g_head, *g_tail;

static void region_lost(void);
static void drain(void *arg);

static void copy_rect(uint8_t *dst, const uint8_t *src, const lv_area_t *a) { /* canvases of the same size */
    size_t w = (size_t)(a->x2 - a->x1 + 1);
    for (int y = a->y1; y <= a->y2; y++) memcpy(dst + (size_t)y * (size_t)RS.w + (size_t)a->x1, src + (size_t)y * (size_t)RS.w + (size_t)a->x1, w);
}

static int send_credit(int n) {
    uint8_t c[5] = {(uint8_t)n, 0, 0, 0x10, 0}; /* up to 1 MiB per update */
    return ink_rt_send(RS.sid, INK_RT_CREDIT, c, sizeof c, 0);
}

/* UI task: credits owed go out when the session is up and, after an update, when the panel has shown
 * it; a send that fails is tried again. While the session is down it rests: the hub's confirmation
 * starts it again (session_ready). */
static void credit_check(lv_timer_t *t) {
    if (!RS.owed || !ink_rt_ready(RS.sid)) {
        lv_timer_pause(t);
        return;
    }
    if (RS.waiting_display && (ink_display_busy() || ink_ui_busy())) return;
    if (send_credit(RS.owed) != 0) return;
    RS.pulled = true;
    RS.owed = 0;
    RS.waiting_display = false;
    lv_timer_pause(t);
}

/* A word at the bottom of the page: the picture may be old or will not change any more. NULL: none. */
static void note_set(const char *zh, const char *en) {
    if (!RS.note) return;
    if (zh) {
        lv_label_set_text(RS.note, ink_font_for_pt(8, RS.dpi) ? zh : en);
        lv_obj_remove_flag(RS.note, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(RS.note, LV_OBJ_FLAG_HIDDEN);
    ink_ui_hint_obj(RS.note, INK_REFRESH_UI);
}

/* The picture under the tap comes back: the hub's next update, or nothing came. Inverted once more,
 * not copied from `back`: that may hold part of an update not complete yet. UI task. */
static void spot_restore(void) {
    if (!RS.spot_on) return;
    RS.spot_on = false;
    lv_timer_pause(RS.spot_timer);
    for (int y = RS.spot.y1; y <= RS.spot.y2; y++)
        for (int x = RS.spot.x1; x <= RS.spot.x2; x++) RS.px[(size_t)y * (size_t)RS.w + (size_t)x] ^= 0xFF;
    ink_ui_hint_area(&RS.spot, INK_REFRESH_UI);
    lv_obj_invalidate_area(RS.img, &RS.spot);
    ink_probe_count("remote.feedback_restored");
}

static void spot_expired(lv_timer_t *t) { /* UI task */
    (void)t;
    spot_restore();
}

static void show_update(void *arg) { /* UI task */
    (void)arg;
    typeof(RS.dirty[0]) d[DIRTY_MAX];
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    bool complete = RS.holding && RS.img; /* else it came too late: an older picture's, already given up */
    osal_mutex_unlock(RS.lock);
    if (!complete) return;
    spot_restore();
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    int n = RS.ndirty;
    memcpy(d, RS.dirty, sizeof d[0] * (size_t)n);
    RS.ndirty = 0;
    bool all = RS.repaint;
    RS.repaint = false;
    if (all) memcpy(RS.px, RS.back, (size_t)RS.w * (size_t)RS.h);
    else
        for (int i = 0; i < n; i++) copy_rect(RS.px, RS.back, &d[i].a);
    RS.holding = false;
    osal_mutex_unlock(RS.lock);
    ink_exec_post(INK_EXEC_MEDIA, &RS.drain_job, drain, NULL); /* the next update's regions */
    if (all) { /* after a new connection: nothing of the old picture stays */
        lv_area_t whole = {0, 0, RS.w - 1, RS.h - 1};
        ink_ui_hint_area(&whole, INK_REFRESH_FULL);
        lv_obj_invalidate_area(RS.img, &whole);
    } else
        for (int i = 0; i < n; i++) {
            ink_ui_hint_area(&d[i].a, HINTS[d[i].hint < 3 ? d[i].hint : 1]);
            lv_obj_invalidate_area(RS.img, &d[i].a);
        }
    if (RS.page == LOADING || RS.page == OFFLINE) page_show(REMOTE);
    RS.updates++;
    ink_probe_count("remote.updates");
    if (!RS.first_shown) {
        RS.first_shown = true;
        ink_ui_cause(RS.opened_us, "remote.open_to_first_visible_ms");
    } else if (RS.tap_us) {
        ink_ui_cause(RS.tap_us, "remote.tap_to_update_visible_ms");
        RS.tap_us = 0;
    }
    RS.owed++;
    RS.waiting_display = true;
    lv_timer_resume(RS.credit_timer);
}

static bool still_current(uint32_t gen) { /* under the lock */
    return gen == RS.gen && RS.active && RS.back;
}

/* One region into scratch memory, then, if its picture is still the current one, into `back`. */
static void decode_region(region_job_t *j) { /* exec.media */
    const uint8_t *p = j->data;
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    bool current = still_current(j->gen);
    osal_mutex_unlock(RS.lock);
    bool ok = false;
    int x = 0, y = 0, w = 0, h = 0, hint = 0;
    if (!current) ink_probe_count("remote.stale_regions");
    else if (j->len >= 10) {
        x = p[0] | p[1] << 8, y = p[2] | p[3] << 8, w = p[4] | p[5] << 8, h = p[6] | p[7] << 8, hint = p[9];
        int enc = p[8];
        size_t px = (size_t)w * (size_t)h, packed = (px + 1) / 2;
        if (w > 0 && h > 0 && x + w <= RS.w && y + h <= RS.h) {
            if (RS.tmp_cap < px) {
                osal_free(RS.tmp);
                RS.tmp = osal_malloc(px, OSAL_MEM_EXTERNAL);
                RS.tmp_cap = RS.tmp ? px : 0;
                if (!RS.tmp) region_lost();
            }
            if (RS.tmp && enc == 2) ok = hal_jpeg_decode_gray(p + 10, j->len - 10, RS.tmp, w, h) == 0; /* 8-bit gray */
            else if (RS.tmp) {
                const uint8_t *nib = p + 10;
                if (enc == 1) { /* into the back half of the scratch, then widened from the front */
                    nib = RS.tmp + px - packed;
                    ok = ink_lz4_decode(p + 10, j->len - 10, RS.tmp + px - packed, packed) == (int)packed;
                } else ok = j->len - 10 >= packed;
                for (size_t i = 0; ok && i < px; i++) /* 4-bit -> 8-bit, in place from the front */
                    RS.tmp[i] = (uint8_t)(((nib[i / 2] >> (i & 1 ? 0 : 4)) & 15) * 17);
            }
        }
        if (!ok) ink_probe_count("remote.bad_regions");
    }
    bool show = false;
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    if (still_current(j->gen)) { /* checked again: it may have become old while it was decoded */
        if (ok) {
            for (int r = 0; r < h; r++) memcpy(RS.back + (size_t)(y + r) * (size_t)RS.w + (size_t)x, RS.tmp + (size_t)r * (size_t)w, (size_t)w);
            if (RS.ndirty < DIRTY_MAX) {
                RS.dirty[RS.ndirty].a = (lv_area_t){x, y, x + w - 1, y + h - 1};
                RS.dirty[RS.ndirty].hint = (uint8_t)hint;
                RS.ndirty++;
            } else RS.repaint = true; /* too many: show all of it */
        }
        show = j->flags & INK_RT_F_LAST;
        if (show) RS.holding = true;
    }
    osal_mutex_unlock(RS.lock);
    if (ok) ink_probe_value("remote.region_bytes", (double)j->len);
    if (show) ink_exec_post(INK_EXEC_UI, &RS.show_job, show_update, NULL);
    osal_free(j);
}

static void drain(void *arg) { /* exec.media */
    (void)arg;
    for (;;) {
        osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
        region_job_t *j = RS.holding ? NULL : g_head; /* show_update starts this again */
        if (j) {
            g_head = j->next;
            if (!g_head) g_tail = NULL;
        }
        osal_mutex_unlock(RS.lock);
        if (!j) return;
        decode_region(j);
    }
}

static void session_restart(void *arg);

/* A region could not be kept: the hub believes it is shown, and if it ended an update no credit would
 * ever follow. The picture can only start over: a new session (hub's and device's start from white). */
static void region_lost(void) {
    ink_probe_count("remote.regions_dropped");
    ink_exec_post(INK_EXEC_UI, &RS.restart_job, session_restart, NULL);
}

static void on_frame(void *ctx, int sid, uint8_t kind, const uint8_t *payload, size_t len, uint16_t seq, uint32_t t_ms, uint8_t flags) {
    (void)ctx;
    (void)seq;
    (void)t_ms;
    if (kind != INK_RT_SCREEN) return; /* hub task */
    region_job_t *j = osal_malloc(sizeof *j + len, OSAL_MEM_EXTERNAL);
    if (!j) {
        region_lost();
        return;
    }
    j->next = NULL;
    j->flags = flags;
    j->len = len;
    memcpy(j->data, payload, len);
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    bool ours = sid == RS.sid;
    j->gen = RS.gen;
    if (ours) {
        if (g_tail) g_tail->next = j;
        else g_head = j;
        g_tail = j;
    }
    osal_mutex_unlock(RS.lock);
    if (!ours) osal_free(j);
    else if (ink_exec_post(INK_EXEC_MEDIA, &RS.drain_job, drain, NULL) != 0) region_lost();
}

static void new_picture(void) { /* under the lock: what is queued or waiting to be shown no longer counts */
    RS.gen++;
    RS.ndirty = 0;
    RS.holding = false;
}

/* UI task. The hub confirmed the session: this connection's hub knows nothing of what the device
 * shows and starts from a white screen. Regions still queued from before are dropped, what stays on
 * the panel until the first update comes is the old picture. */
static void session_ready(void *arg) {
    (void)arg;
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    bool ours = RS.img && RS.ready_sid == RS.sid;
    RS.ready_sid = 0;
    if (ours) {
        new_picture();
        memset(RS.back, 0xFF, (size_t)RS.w * (size_t)RS.h);
        RS.repaint = RS.pulled; /* some of an older picture may be on the panel */
    }
    osal_mutex_unlock(RS.lock);
    if (!ours) return;
    RS.down_us = 0;
    ink_exec_post(INK_EXEC_MEDIA, &RS.drain_job, drain, NULL); /* what was queued goes, as old */
    if (RS.pulled) ink_probe_count("remote.resynced");
    RS.owed = 2;
    RS.waiting_display = false;
    lv_timer_resume(RS.credit_timer);
}

static void on_ready(void *ctx, int sid) { /* hub task */
    (void)ctx;
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    RS.ready_sid = sid;
    osal_mutex_unlock(RS.lock);
    ink_exec_post(INK_EXEC_UI, &RS.ready_job, session_ready, NULL);
}

static void session_closed(void *arg) { /* UI task: the hub refused or ended it: this page will not change any more */
    (void)arg;
    if (!RS.img) return;
    RS.ended = true;
    page_show(OFFLINE);
    note_set("网页结束了，按返回键退出", "The remote screen ended - press back");
    ink_probe_count("remote.closed_by_hub");
}

static void on_closed(void *ctx, int sid, int reason) { /* hub task */
    (void)ctx;
    (void)reason;
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    bool ours = sid == RS.sid;
    if (ours) RS.sid = 0;
    osal_mutex_unlock(RS.lock);
    if (ours) ink_exec_post(INK_EXEC_UI, &RS.closed_job, session_closed, NULL);
}

static const ink_rt_handler_t RT;

static void session_open(void) { /* UI task */
    int sid = ink_rt_open("browser.session", RS.params, &RT); /* the first pull goes out once the hub confirmed it */
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    RS.sid = sid > 0 ? sid : 0;
    osal_mutex_unlock(RS.lock);
    if (sid <= 0 || !ink_rt_is_open(sid)) session_closed(NULL);
}

static void session_close(void) { /* UI task: what is queued no longer lands anywhere */
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    int sid = RS.sid;
    RS.sid = 0;
    new_picture();
    osal_mutex_unlock(RS.lock);
    if (sid > 0) ink_rt_close(sid);
    ink_exec_post(INK_EXEC_MEDIA, &RS.drain_job, drain, NULL);
}

static void session_restart(void *arg) { /* UI task */
    (void)arg;
    if (!RS.img || RS.suspended || RS.ended) return;
    session_close();
    session_open();
}

/* UI task: the realtime service reannounces; the device bounds the recovery window. */
static void link_check(lv_timer_t *t) {
    (void)t;
    ink_probe_count("remote.link_checks");
    if (RS.suspended || RS.ended || !RS.sid) return;
    uint64_t now = osal_now_us();
    int64_t online = 1;
    ink_state_get_int(ink_state_find("net.online"), &online);
    RS.down_ticks = ink_rt_ready(RS.sid) ? 0 : RS.down_ticks + 1;
    bool down = !online || (RS.first_shown && RS.down_ticks >= 6);
    if (down && !RS.down_us) RS.down_us = now;
    if (RS.down_us && now - RS.down_us >= 300000000ull) {
        session_close();
        RS.ended = true;
        page_show(OFFLINE);
        return;
    }
    if (down && !RS.down_shown) {
        RS.down_shown = true;
        ink_probe_count("remote.offline_shown");
        page_show(OFFLINE);
        note_set("连接断了，正在重连…", "Connection lost, reconnecting...");
    } else if (!down && RS.down_shown) {
        RS.down_shown = false;
        note_set(NULL, NULL);
    }
    if (!RS.first_shown && !RS.down_shown && RS.page != ADDRESS && RS.page != TYPE &&
        now - RS.opened_us >= 1500000ull && RS.page != LOADING) page_show(LOADING);
}

/* UI task. Locked, nobody looks at the page: no session keeps the link up and background work waiting.
 * Unlocked, a new session: the hub's picture replaces the one left on the panel. */
static void on_power_state(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)id;
    char s[16] = "";
    ink_state_get_str(h, s, sizeof s);
    bool locked = !strcmp(s, "locked");
    if (!RS.img || RS.ended || locked == RS.suspended) return;
    RS.suspended = locked;
    if (locked) {
        lv_timer_pause(RS.link_timer);
        session_close();
    } else {
        lv_timer_resume(RS.link_timer);
        session_open();
    }
}

/* ------------------------------------------------------------ up: input */

static void tapped(lv_event_t *e) { /* UI task */
    lv_point_t pt;
    lv_indev_get_point(lv_indev_active(), &pt);
    char args[64];
    snprintf(args, sizeof args, "{\"x\":%ld,\"y\":%ld,\"long\":%s}", (long)pt.x, (long)pt.y,
             lv_event_get_code(e) == LV_EVENT_LONG_PRESSED ? "true" : "false");
    ink_receipt_t rc;
    ink_cap_invoke_json("remote.pointer", args, INK_SRC_TOUCH, ink_ui_content_version(), &rc);
}

static void pointer_send(int tap_x, int tap_y, bool held, ink_receipt_t *rc) {
    lv_point_t pt = {tap_x, tap_y};
    RS.tap_us = osal_now_us();
    spot_restore();
    lv_area_t a = {pt.x - FEEDBACK, pt.y - FEEDBACK, pt.x + FEEDBACK, pt.y + FEEDBACK};
    if (a.x1 < 0) a.x1 = 0;
    if (a.y1 < 0) a.y1 = 0;
    if (a.x2 >= RS.w) a.x2 = RS.w - 1;
    if (a.y2 >= RS.h) a.y2 = RS.h - 1;
    for (int y = a.y1; y <= a.y2; y++) /* local feedback: invert the spot now, the hub's answer follows */
        for (int x = a.x1; x <= a.x2; x++) RS.px[(size_t)y * (size_t)RS.w + (size_t)x] ^= 0xFF;
    RS.spot = a;
    RS.spot_on = true;
    lv_timer_reset(RS.spot_timer);
    lv_timer_resume(RS.spot_timer);
    ink_ui_hint_area(&a, INK_REFRESH_A2); /* black and white, the fastest waveform */
    lv_obj_invalidate_area(RS.img, &a);
    ink_ui_cause(RS.tap_us, "remote.tap_feedback_visible_ms");
    uint8_t in[5] = {held ? 3 : 1, (uint8_t)pt.x, (uint8_t)(pt.x >> 8), (uint8_t)pt.y, (uint8_t)(pt.y >> 8)};
    /* not resent: by the time the link is back it may mean something else on a new picture */
    if (ink_rt_send(RS.sid, INK_RT_INPUT, in, sizeof in, 0) != 0) {
        RS.tap_us = 0;
        rc->status = INK_RC_FAILED;
        ink_probe_count("remote.taps_lost");
    }
    ink_probe_count(held ? "remote.long_presses" : "remote.taps");
}

/* ------------------------------------------------------------ the activity */

static void control_show(void *arg) {
    (void)arg;
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    bool ours = RS.control_sid == RS.sid && RS.img;
    int kind = RS.control_kind;
    osal_mutex_unlock(RS.lock);
    if (!ours) return;
    RS.secret = kind == 2;
    if (kind == 3) { session_close(); RS.ended = true; }
    page_show(kind == 3 ? BLOCKED : TYPE);
}
static void on_control(void *ctx, int sid, const char *method, const ink_json_t *j, int params) {
    (void)ctx;
    if (strcmp(method, "rt.nav")) return;
    bool blocked = false;
    char input[16] = "";
    blocked = ink_json_eq(j, ink_json_find(j, params, "blocked"), "true");
    ink_json_str(j, ink_json_find(j, params, "input"), input, sizeof input);
    int kind = blocked ? 3 : !strcmp(input, "password") ? 2 : !strcmp(input, "text") ? 1 : 0;
    if (!kind) return;
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    RS.control_sid = sid; RS.control_kind = kind;
    osal_mutex_unlock(RS.lock);
    ink_exec_post(INK_EXEC_UI, &RS.control_job, control_show, NULL);
}
static const ink_rt_handler_t RT = {.on_frame = on_frame, .on_ready = on_ready, .on_closed = on_closed, .on_control = on_control};

static void remote_create(lv_obj_t *scr, void *arg) {
    const char *url = arg ? arg : "";
    if (!RS.lock) RS.lock = osal_mutex_create();
    RS.w = lv_display_get_horizontal_resolution(NULL);
    RS.h = lv_display_get_vertical_resolution(NULL) - ink_ui_px(INK_PRIMARY + INK_GAP);
    size_t size = (size_t)RS.w * (size_t)RS.h;
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    RS.active = true; /* from here on the release job leaves the canvases alone */
    RS.gen++;
    if (!RS.px) RS.px = osal_malloc(size, OSAL_MEM_EXTERNAL);
    if (!RS.back) RS.back = osal_malloc(size, OSAL_MEM_EXTERNAL);
    bool ok = RS.px && RS.back;
    if (ok) {
        memset(RS.px, 0xFF, size);
        memset(RS.back, 0xFF, size);
    }
    RS.ndirty = 0;
    RS.holding = false;
    RS.repaint = false;
    RS.ready_sid = 0;
    osal_mutex_unlock(RS.lock);
    if (!ok) {
        ink_probe_count("remote.no_memory");
        return;
    }
    RS.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    RS.dsc.header.cf = LV_COLOR_FORMAT_L8;
    RS.dsc.header.w = (uint32_t)RS.w;
    RS.dsc.header.h = (uint32_t)RS.h;
    RS.dsc.header.stride = (uint32_t)RS.w;
    RS.dsc.data_size = (uint32_t)(RS.w * RS.h);
    RS.dsc.data = RS.px;
    RS.img = lv_image_create(scr);
    lv_image_set_src(RS.img, &RS.dsc);
    lv_obj_set_pos(RS.img, 0, 0);
    lv_obj_add_flag(RS.img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(RS.img, tapped, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(RS.img, tapped, LV_EVENT_LONG_PRESSED, NULL);
    RS.note = lv_label_create(scr);
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    RS.dpi = pi.dpi;
    ink_font_t *cjk = ink_font_for_pt(8, pi.dpi); /* the UI fonts are Latin only */
    lv_obj_set_style_text_font(RS.note, cjk ? ink_font_lvgl(cjk) : ink_ui_font(2), 0);
    lv_obj_set_style_bg_color(RS.note, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(RS.note, LV_OPA_COVER, 0);
    lv_obj_align(RS.note, LV_ALIGN_TOP_MID, 0, ink_ui_px(INK_STATUS_BAR + INK_CONTROL + INK_GAP)); /* at the top: the AI's answer sheet may cover the lower part */
    lv_obj_add_flag(RS.note, LV_OBJ_FLAG_HIDDEN);
    RS.credit_timer = lv_timer_create(credit_check, 40, NULL);
    lv_timer_pause(RS.credit_timer); /* until the hub confirmed the session */
    RS.spot_timer = lv_timer_create(spot_expired, FEEDBACK_MS, NULL);
    lv_timer_pause(RS.spot_timer);
    RS.link_timer = lv_timer_create(link_check, 500, NULL);
    RS.spot_on = false;
    RS.owed = 0;
    RS.ended = RS.down_shown = false;
    RS.down_ticks = 0;
    RS.first_shown = RS.waiting_display = RS.pulled = false;
    RS.tap_us = 0;
    RS.opened_us = osal_now_us();
    RS.chrome = NULL;
    RS.down_us = 0;
    RS.secret = false;
    RS.url[0] = 0;
    char ps[16] = "";
    ink_state_get_str(ink_state_find("power.state"), ps, sizeof ps);
    RS.suspended = !strcmp(ps, "locked");
    if (RS.suspended) lv_timer_pause(RS.link_timer);
    if (url[0]) address_open(url);
    else page_show(ADDRESS);
}

/* After the regions still queued (on the same executor): the canvases go back, unless the host was
 * opened again meanwhile. */
static void release_canvas(void *arg) { /* exec.media */
    (void)arg;
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    if (!RS.active) {
        osal_free(RS.px);
        osal_free(RS.back);
        osal_free(RS.tmp);
        RS.px = RS.back = RS.tmp = NULL;
        RS.tmp_cap = 0;
    }
    osal_mutex_unlock(RS.lock);
}

static void remote_destroy(void) {
    if (RS.credit_timer) lv_timer_delete(RS.credit_timer);
    if (RS.spot_timer) lv_timer_delete(RS.spot_timer);
    if (RS.link_timer) lv_timer_delete(RS.link_timer);
    RS.credit_timer = RS.spot_timer = RS.link_timer = NULL;
    RS.img = RS.note = RS.chrome = RS.field = NULL;
    if (!RS.lock) return;
    session_close();
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    RS.active = false;
    osal_mutex_unlock(RS.lock);
    if (ink_exec_post(INK_EXEC_MEDIA, &RS.release_job, release_canvas, NULL) != 0) ink_probe_count("remote.canvas_kept");
}

static bool remote_key(hal_key_t key, bool long_press) {
    (void)long_press;
    ink_receipt_t rc;
    if ((key == HAL_KEY_NEXT || key == HAL_KEY_PREV) && RS.page == REMOTE) {
        ink_cap_invoke(key == HAL_KEY_NEXT ? "remote.next" : "remote.prev", NULL, INK_SRC_KEY, ink_ui_content_version(), &rc);
        return true;
    }
    if (key == HAL_KEY_BACK || key == HAL_KEY_HOME) {
        ink_cap_invoke("remote.close", NULL, INK_SRC_KEY, ink_ui_content_version(), &rc);
        return true;
    }
    return false;
}

static int remote_snapshot(char *buf, size_t cap) {
    return RS.field ? ink_textfield_short_snapshot(RS.field, buf, cap) : ink_ui_semantics_snapshot(&RS.semantics, buf, cap);
}

const ink_activity_t ink_app_remote = {
    .name = "remote", .app = "remote", .create = remote_create, .destroy = remote_destroy, .on_key = remote_key, .snapshot = remote_snapshot};

static char g_url[180]; /* so that the session's parameters always hold it (RS.params) */

static void cap_open(ink_call_t *c) { /* UI task */
    if (ink_member_current()->role == INK_ROLE_CHILD) { c->rc.status = INK_RC_REJECTED; return; }
    char url[sizeof g_url];
    int n = ink_args_get_str(&c->args, "url", url, sizeof url);
    if (n >= (int)sizeof url) { /* opening something else than was asked for would be worse */
        c->rc.status = INK_RC_INVALID;
        snprintf(c->rc.detail, sizeof c->rc.detail, "the address is too long");
        return;
    }
    snprintf(g_url, sizeof g_url, "%s", n < 0 ? "" : url);
    if (!strcmp(ink_ui_page_name(), "remote") && RS.img) {
        if (g_url[0]) address_open(g_url);
        else page_show(ADDRESS);
    } else ink_ui_push(&ink_app_remote, g_url);
    snprintf(c->rc.object, sizeof c->rc.object, "screen:remote");
    snprintf(c->rc.say, sizeof c->rc.say, "打开网页");
}

static const char *const BOARDS[] = {"Remote", "RemoteLoading", "RemoteOffline", "RemoteAddress", "RemoteType", "RemoteBlocked"};
static const char *page_message(void) {
    if (RS.page == LOADING) return "正在打开";
    if (RS.page == OFFLINE) return RS.ended ? "网页已关闭，请重新打开" : "网页断开了";
    if (RS.page == BLOCKED) return "这个网页打不开";
    if (RS.page == TYPE) return "打完字点“完成”，字才发给网页。";
    return "家庭服务器渲染，内容由家庭服务器提供给 AI";
}
static void address_open(const char *url) {
    char value[sizeof RS.url];
    snprintf(value, sizeof value, "%s", url);
    session_close();
    snprintf(RS.url, sizeof RS.url, "%s", value);
    ink_jw_t w;
    ink_jw_init(&w, RS.params, sizeof RS.params);
    char head[64];
    snprintf(head, sizeof head, "{\"w\":%d,\"h\":%d,\"gray\":16,\"url\":", RS.w, RS.h);
    ink_jw_raw(&w, head); ink_jw_str(&w, value); ink_jw_raw(&w, "}");
    RS.ended = RS.down_shown = RS.first_shown = false;
    RS.down_us = 0; RS.opened_us = osal_now_us();
    page_show(LOADING);
    if (w.overflow) { RS.ended = true; page_show(OFFLINE); }
    else if (!RS.suspended) session_open();
}
static void entered(void *ctx, const char *value) {
    (void)ctx; (void)value;
    ink_receipt_t rc;
    ink_cap_invoke("remote.submit", NULL, INK_SRC_TOUCH, ink_ui_content_version(), &rc);
}
static void action(size_t i, const char *id, const char *label, const char *cap) {
    RS.actions[i] = (ink_ui_action_spec_t){.id=id, .label=label, .capability=cap, .args_json="{}", .enabled=true};
}
static void focus_input(void *ctx) { (void)ctx; if (RS.field) ink_textfield_short_focus(RS.field); }
static void page_show(remote_page_t page) {
    if (!RS.img) return;
    RS.page = page;
    if (RS.chrome) lv_obj_delete(RS.chrome);
    RS.field = NULL;
    int w = INK_CANVAS_W - 2 * INK_MARGIN;
    int y = INK_STATUS_BAR + INK_CONTROL + INK_GAP;
    RS.chrome = ink_ui_box(lv_obj_get_parent(RS.img), 0, page == REMOTE ? INK_CANVAS_H - INK_PRIMARY - INK_GAP : 0,
        INK_CANVAS_W, page == REMOTE ? INK_PRIMARY + INK_GAP : INK_CANVAS_H, 0, INK_SURFACE_WHITE, false);
    memset(RS.actions, 0, sizeof RS.actions);
    RS.fields[0] = (ink_ui_field_spec_t){.id="board", .role="text", .label="页面", .value=BOARDS[page]};
    RS.fields[1] = (ink_ui_field_spec_t){.id="status", .role="text", .label="状态", .value=page_message()};
    RS.semantics = (ink_ui_page_spec_t){.id="remote", .title="网页", .object={"remote", RS.gen},
        .fields=RS.fields, .field_count=2, .actions=RS.actions, .action_count=(page == REMOTE || page == ADDRESS || page == TYPE) ? 4 : page == LOADING ? 2 : 3};
    action(0, "close", "关闭", "remote.close");
    action(1, "address", "换一个地址", "remote.address");
    if (page == REMOTE) {
        action(2, "prev", "上一页", "remote.prev"); action(3, "next", "下一页", "remote.next");
        ink_shell_bind(ink_ui_circle(RS.chrome, "back", INK_MARGIN, INK_GAP_S, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL), "remote.prev", "{}");
        ink_shell_bind(ink_ui_circle(RS.chrome, "next", INK_MARGIN + INK_CONTROL + INK_GAP_M, INK_GAP_S, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_WHITE, NULL, NULL), "remote.next", "{}");
        ink_shell_bind(ink_ui_pill(RS.chrome, "网址", INK_CANVAS_W / 2, INK_GAP_S, w / 2, INK_CONTROL, false, NULL, NULL), "remote.address", "{}");
    } else {
        ink_shell_statusbar(RS.chrome, true);
        ink_shell_bind(ink_ui_circle(RS.chrome, "close", INK_MARGIN, INK_STATUS_BAR + INK_GAP_S, INK_CONTROL, INK_ICON_MEDIUM, INK_SURFACE_STATIC, NULL, NULL), "remote.close", "{}");
        ink_shell_bind(ink_ui_pill(RS.chrome, RS.url[0] ? RS.url : "打开网页", INK_MARGIN + INK_CONTROL + INK_GAP_M, INK_STATUS_BAR + INK_GAP_S,
            w - INK_CONTROL - INK_GAP_M, INK_CONTROL, false, NULL, NULL), "remote.address", "{}");
        if (page == ADDRESS || page == TYPE) {
            ink_text_cfg_t cfg = {.type=page == ADDRESS ? INK_TEXT_URL : RS.secret ? INK_TEXT_PASSWORD : INK_TEXT_PLAIN,
                .initial=page == ADDRESS ? RS.url : "", .hint=page == ADDRESS ? "输入网址" : "输入文字", .max=page == ADDRESS ? sizeof RS.url - 1 : 255, .ai_hidden=page == TYPE && RS.secret};
            action(2, "focus", "输入", "remote.focus"); action(3, "submit", page == ADDRESS ? "打开" : "完成", "remote.submit");
            RS.actions[3].enabled = page == ADDRESS || !RS.secret;
            RS.field = ink_textfield_create(RS.chrome, &cfg, entered, NULL, false);
            lv_obj_set_pos(RS.field, ink_ui_px(INK_MARGIN), ink_ui_px(y));
            lv_obj_set_width(RS.field, ink_ui_px(w));
            RS.semantics.field_count = 3;
            RS.fields[2] = (ink_ui_field_spec_t){.id="input", .role="textbox", .label=cfg.hint, .secret=cfg.ai_hidden};
            ink_textfield_bind_short(RS.field, &RS.semantics, &RS.fields[2], &RS.actions[2]);
            ink_exec_post(INK_EXEC_UI, &RS.focus_job, focus_input, NULL);
            if (page == TYPE) ink_ui_label(RS.chrome, page_message(), INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, y + INK_CONTROL + INK_GAP, w);
            if (page == ADDRESS) ink_ui_notice(RS.chrome, "输入网址后点“完成”打开", y + INK_CONTROL + INK_GAP);
            /* The common text service masks credentials in every input snapshot. */
        } else {
            const char *icon = page == BLOCKED ? "warn" : page == OFFLINE ? "wifioff" : "globe";
            ink_ui_circle(RS.chrome, icon, (INK_CANVAS_W - INK_CIRCLE_LARGE) / 2, INK_CANVAS_H / 2 - INK_CIRCLE_LARGE, INK_CIRCLE_LARGE, INK_ICON_STATE, INK_SURFACE_STATIC, NULL, NULL);
            lv_obj_t *label = ink_ui_label(RS.chrome, page_message(), INK_FONT_SUBTITLE, INK_WEIGHT_BOLD, INK_MARGIN, INK_CANVAS_H / 2 + INK_GAP, w);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            if (page != LOADING) {
                const char *detail = page == BLOCKED ? "家庭服务器不允许打开这个网站。" : RS.ended ? "请重新打开，刚才的网页可能无法恢复。" : "5 分钟内连回来，还在刚才那一页。";
                label = ink_ui_label(RS.chrome, detail, INK_FONT_NOTE, INK_WEIGHT_REGULAR, INK_MARGIN, INK_CANVAS_H / 2 + INK_LINE_SUBTITLE + INK_GAP_L, w);
                lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
                action(2, "retry", page == BLOCKED ? "换一个地址" : "重新连接", page == BLOCKED ? "remote.address" : "remote.retry");
                ink_shell_bind(ink_ui_pill(RS.chrome, RS.actions[2].label, INK_MARGIN, INK_CANVAS_H - INK_MARGIN - INK_PRIMARY, w, INK_PRIMARY, true, NULL, NULL), RS.actions[2].capability, "{}");
            }
        }
    }
    lv_obj_move_foreground(RS.note);
    ink_ui_hint_frame(INK_REFRESH_FULL);
    ink_ui_content_changed();
}
static void cap_input(ink_call_t *c) {
    if (ink_member_current()->role == INK_ROLE_CHILD || !RS.img) { c->rc.status = INK_RC_REJECTED; return; }
    const char *id = c->cap->id;
    if (!strcmp(id, "remote.close")) { ink_ui_pop(); return; }
    if (!strcmp(id, "remote.address")) { page_show(ADDRESS); return; }
    if (!strcmp(id, "remote.focus")) { if (!RS.field || ink_textfield_short_focus(RS.field) < 0) c->rc.status = INK_RC_REJECTED; return; }
    if (!strcmp(id, "remote.retry")) { address_open(RS.url); return; }
    if (!strcmp(id, "remote.submit")) {
        if (!RS.field) { c->rc.status = INK_RC_REJECTED; return; }
        const char *text = ink_textfield_text(RS.field);
        if (RS.page == ADDRESS) { if (text[0]) address_open(text); else c->rc.status = INK_RC_INVALID; }
        else if (RS.page == TYPE && (!RS.secret || c->src == INK_SRC_TOUCH || c->src == INK_SRC_KEY)) {
            uint8_t input[256] = {4}; size_t len = strlen(text);
            memcpy(input + 1, text, len);
            if (ink_rt_send(RS.sid, INK_RT_INPUT, input, len + 1, 0)) c->rc.status = INK_RC_FAILED;
            else page_show(REMOTE);
            memset(input, 0, sizeof input);
        } else c->rc.status = INK_RC_REJECTED;
        return;
    }
    if (RS.page != REMOTE || !ink_rt_ready(RS.sid)) { c->rc.status = INK_RC_REJECTED; return; }
    if (!strcmp(id, "remote.pointer")) {
        int64_t x = -1, y = -1; bool held = false;
        ink_args_get_int(&c->args, "x", &x); ink_args_get_int(&c->args, "y", &y); ink_args_get_bool(&c->args, "long", &held);
        if (x < 0 || y < 0 || x >= RS.w || y >= RS.h) c->rc.status = INK_RC_INVALID;
        else pointer_send((int)x, (int)y, held, &c->rc);
    } else {
        uint8_t input[2] = {2, !strcmp(id, "remote.next")};
        if (ink_rt_send(RS.sid, INK_RT_INPUT, input, sizeof input, 0)) c->rc.status = INK_RC_FAILED;
    }
}

static const ink_cap_t REMOTE_CAPS[] = {
    {.id = "remote.open",
     .title = "打开网页",
     .input = "{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\",\"maxLength\":120}}}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = false,
     .run = cap_open},
    {.id="remote.address", .title="输入网址", .effect=INK_FX_NAVIGATE, .activity="remote", .run=cap_input},
    {.id="remote.focus", .title="输入网页文字", .effect=INK_FX_NAVIGATE, .activity="remote", .run=cap_input},
    {.id="remote.submit", .title="完成网页输入", .effect=INK_FX_NAVIGATE, .activity="remote", .run=cap_input},
    {.id="remote.retry", .title="重新打开网页", .effect=INK_FX_NAVIGATE, .activity="remote", .run=cap_input},
    {.id="remote.close", .title="关闭网页", .effect=INK_FX_NAVIGATE, .activity="remote", .run=cap_input},
    {.id="remote.prev", .title="网页上一页", .effect=INK_FX_NAVIGATE, .activity="remote", .run=cap_input},
    {.id="remote.next", .title="网页下一页", .effect=INK_FX_NAVIGATE, .activity="remote", .run=cap_input},
    {.id="remote.pointer", .title="点按或长按网页", .effect=INK_FX_NAVIGATE, .activity="remote",
     .input="{\"type\":\"object\",\"properties\":{\"x\":{\"type\":\"integer\"},\"y\":{\"type\":\"integer\"},\"long\":{\"type\":\"boolean\"}},\"required\":[\"x\",\"y\"]}", .run=cap_input},
};

int ink_remote_init(void) {
    ink_state_watch("power.state", on_power_state, NULL, INK_EXEC_UI);
    return ink_cap_register(REMOTE_CAPS, sizeof REMOTE_CAPS / sizeof *REMOTE_CAPS);
}

int ink_remote_app_init(void) {
    static const ink_app_def_t app = {.id="remote", .title="网页", .icon=INK_UI_ICON(globe), .entry=&ink_app_remote, .system_app=true};
    return ink_app_register(&app);
}
