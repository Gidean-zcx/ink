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
 *         input [1 tap][x u16][y u16]
 *
 * Two canvases in PSRAM, 8-bit gray. Regions are decoded on the media executor
 * into `back`, the picture as the hub knows it; when an update is complete the
 * UI task copies what changed to `px`, the only one LVGL reads, and marks those
 * areas with the refresh hint they came with. A generation number follows the
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
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/font.h"
#include "ink/sys/json.h"
#include "ink/sys/lz4.h"
#include "ink/sys/rt.h"
#include "ink/sys/ui.h"

#define DIRTY_MAX 32
#define FEEDBACK 24 /* half size of the local tap feedback square (px) */
#define FEEDBACK_MS 2000 /* a tap the hub does not answer with a new picture: the spot goes back */

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
    uint32_t complete_gen; /* the generation of the update just completed: what show_update may show (under the lock) */
    struct {
        lv_area_t a;
        uint8_t hint;
    } dirty[DIRTY_MAX];
    int ndirty;
    int ready_sid; /* the session the hub just confirmed (under the lock) */
    ink_exec_job_t ready_job, show_job, drain_job, release_job, closed_job;
    /* UI task */
    lv_image_dsc_t dsc;
    lv_obj_t *img, *note;
    lv_timer_t *credit_timer, *spot_timer;
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

/* The picture under the tap comes back: the hub's next update, or nothing came. UI task. */
static void spot_restore(bool mark) {
    if (!RS.spot_on) return;
    RS.spot_on = false;
    lv_timer_pause(RS.spot_timer);
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    copy_rect(RS.px, RS.back, &RS.spot);
    osal_mutex_unlock(RS.lock);
    if (mark) {
        ink_ui_hint_area(&RS.spot, INK_REFRESH_UI);
        lv_obj_invalidate_area(RS.img, &RS.spot);
        ink_probe_count("remote.feedback_restored");
    }
}

static void spot_expired(lv_timer_t *t) { /* UI task */
    (void)t;
    spot_restore(true);
}

static void show_update(void *arg) { /* UI task */
    (void)arg;
    if (!RS.img) return;
    typeof(RS.dirty[0]) d[DIRTY_MAX];
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    bool complete = RS.complete_gen == RS.gen; /* not an update of an older connection, come too late */
    RS.complete_gen = 0;
    osal_mutex_unlock(RS.lock);
    if (!complete) return;
    spot_restore(true);
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    int n = RS.ndirty;
    memcpy(d, RS.dirty, sizeof d[0] * (size_t)n);
    RS.ndirty = 0;
    bool all = RS.repaint;
    RS.repaint = false;
    if (all) memcpy(RS.px, RS.back, (size_t)RS.w * (size_t)RS.h);
    else
        for (int i = 0; i < n; i++) copy_rect(RS.px, RS.back, &d[i].a);
    osal_mutex_unlock(RS.lock);
    if (all) { /* after a new connection: nothing of the old picture stays */
        lv_area_t whole = {0, 0, RS.w - 1, RS.h - 1};
        ink_ui_hint_area(&whole, INK_REFRESH_FULL);
        lv_obj_invalidate_area(RS.img, &whole);
    } else
        for (int i = 0; i < n; i++) {
            ink_ui_hint_area(&d[i].a, HINTS[d[i].hint < 3 ? d[i].hint : 1]);
            lv_obj_invalidate_area(RS.img, &d[i].a);
        }
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
        if (show) RS.complete_gen = j->gen;
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
        region_job_t *j = g_head;
        if (j) {
            g_head = j->next;
            if (!g_head) g_tail = NULL;
        }
        osal_mutex_unlock(RS.lock);
        if (!j) return;
        decode_region(j);
    }
}

static void on_frame(void *ctx, int sid, uint8_t kind, const uint8_t *payload, size_t len, uint16_t seq, uint32_t t_ms, uint8_t flags) {
    (void)ctx;
    (void)seq;
    (void)t_ms;
    if (kind != INK_RT_SCREEN) return; /* hub task */
    region_job_t *j = osal_malloc(sizeof *j + len, OSAL_MEM_EXTERNAL);
    if (!j) {
        ink_probe_count("remote.regions_dropped");
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
    else if (ink_exec_post(INK_EXEC_MEDIA, &RS.drain_job, drain, NULL) != 0) ink_probe_count("remote.regions_dropped");
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
        RS.gen++;
        memset(RS.back, 0xFF, (size_t)RS.w * (size_t)RS.h);
        RS.ndirty = 0;
        RS.repaint = RS.pulled; /* some of an older picture may be on the panel */
    }
    osal_mutex_unlock(RS.lock);
    if (!ours) return;
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
    if (!RS.note) return;
    lv_obj_remove_flag(RS.note, LV_OBJ_FLAG_HIDDEN);
    ink_ui_hint_obj(RS.note, INK_REFRESH_UI);
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

/* ------------------------------------------------------------ up: input */

static void tapped(lv_event_t *e) { /* UI task */
    (void)e;
    lv_point_t pt;
    lv_indev_get_point(lv_indev_active(), &pt);
    RS.tap_us = osal_now_us();
    spot_restore(true);
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
    uint8_t in[5] = {1, (uint8_t)pt.x, (uint8_t)(pt.x >> 8), (uint8_t)pt.y, (uint8_t)(pt.y >> 8)};
    /* not resent: by the time the link is back it may mean something else on a new picture */
    if (ink_rt_send(RS.sid, INK_RT_INPUT, in, sizeof in, 0) != 0) {
        RS.tap_us = 0;
        ink_probe_count("remote.taps_lost");
    }
    ink_probe_count("remote.taps");
}

/* ------------------------------------------------------------ the activity */

static const ink_rt_handler_t RT = {.on_frame = on_frame, .on_ready = on_ready, .on_closed = on_closed};

static void remote_create(lv_obj_t *scr, void *arg) {
    const char *url = arg ? arg : "about:home";
    if (!RS.lock) RS.lock = osal_mutex_create();
    RS.w = lv_display_get_horizontal_resolution(NULL);
    RS.h = lv_display_get_vertical_resolution(NULL);
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
    RS.repaint = false;
    RS.complete_gen = 0;
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
    lv_obj_add_event_cb(RS.img, tapped, LV_EVENT_CLICKED, NULL);
    RS.note = lv_label_create(scr);
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    ink_font_t *cjk = ink_font_for_pt(8, pi.dpi); /* the UI fonts are Latin only */
    lv_obj_set_style_text_font(RS.note, cjk ? ink_font_lvgl(cjk) : ink_ui_font(2), 0);
    lv_obj_set_style_bg_color(RS.note, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(RS.note, LV_OPA_COVER, 0);
    lv_label_set_text(RS.note, cjk ? "远程画面已结束，按返回键退出" : "The remote screen ended - press back");
    lv_obj_align(RS.note, LV_ALIGN_BOTTOM_MID, 0, -ink_dp(16));
    lv_obj_add_flag(RS.note, LV_OBJ_FLAG_HIDDEN);
    RS.credit_timer = lv_timer_create(credit_check, 40, NULL);
    lv_timer_pause(RS.credit_timer); /* until the hub confirmed the session */
    RS.spot_timer = lv_timer_create(spot_expired, FEEDBACK_MS, NULL);
    lv_timer_pause(RS.spot_timer);
    RS.spot_on = false;
    RS.owed = 0;
    RS.first_shown = RS.waiting_display = RS.pulled = false;
    RS.tap_us = 0;
    RS.opened_us = osal_now_us();
    char params[INK_RT_PARAMS];
    ink_jw_t w;
    ink_jw_init(&w, params, sizeof params);
    char head[48];
    snprintf(head, sizeof head, "{\"w\":%d,\"h\":%d,\"gray\":16,\"url\":", RS.w, RS.h);
    ink_jw_raw(&w, head);
    ink_jw_str(&w, url);
    ink_jw_raw(&w, "}");
    if (w.overflow) snprintf(params, sizeof params, "{\"w\":%d,\"h\":%d,\"gray\":16}", RS.w, RS.h); /* too long: the hub's start page */
    int sid = ink_rt_open("browser.session", params, &RT); /* the first pull goes out once the hub confirmed it */
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    RS.sid = sid > 0 ? sid : 0;
    osal_mutex_unlock(RS.lock);
    if (sid <= 0) session_closed(NULL);
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
    RS.credit_timer = RS.spot_timer = NULL;
    RS.img = RS.note = NULL;
    if (!RS.lock) return;
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    int sid = RS.sid;
    RS.sid = 0;
    RS.gen++; /* what is queued no longer lands anywhere */
    RS.active = false;
    RS.ndirty = 0;
    osal_mutex_unlock(RS.lock);
    if (sid > 0) ink_rt_close(sid);
    if (ink_exec_post(INK_EXEC_MEDIA, &RS.release_job, release_canvas, NULL) != 0) ink_probe_count("remote.canvas_kept");
}

static bool remote_key(hal_key_t key, bool long_press) {
    (void)long_press;
    if (key == HAL_KEY_BACK || key == HAL_KEY_HOME) {
        ink_ui_pop();
        return true;
    }
    return false;
}

static int remote_snapshot(char *buf, size_t cap) { /* the hub has the page's text itself (R-039) */
    return snprintf(buf, cap, "远程画面（由中枢渲染，内容由中枢提供给 AI）");
}

const ink_activity_t ink_app_remote = {
    .name = "remote", .create = remote_create, .destroy = remote_destroy, .on_key = remote_key, .snapshot = remote_snapshot};

static char g_url[128];

static void cap_open(ink_call_t *c) { /* UI task */
    if (ink_args_get_str(&c->args, "url", g_url, sizeof g_url) < 0) snprintf(g_url, sizeof g_url, "about:home");
    ink_ui_push(&ink_app_remote, g_url);
    snprintf(c->rc.object, sizeof c->rc.object, "screen:remote");
    snprintf(c->rc.say, sizeof c->rc.say, "打开远程画面");
}

static const ink_cap_t REMOTE_CAPS[] = {
    {.id = "remote.open",
     .title = "打开网页（远程画面）",
     .input = "{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\",\"maxLength\":120}}}",
     .effect = INK_FX_NAVIGATE,
     .child_ok = true,
     .run = cap_open},
};

int ink_remote_init(void) { return ink_cap_register(REMOTE_CAPS, 1); }
