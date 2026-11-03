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
 * Decoding runs on the media executor into an 8-bit gray canvas in PSRAM;
 * the UI task marks the changed areas with the refresh hint they came with.
 * A tap inverts the touched spot at once (local feedback), the remote result
 * follows.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_jpeg.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/lz4.h"
#include "ink/sys/rt.h"
#include "ink/sys/ui.h"

#define DIRTY_MAX 32
#define FEEDBACK 24 /* half size of the local tap feedback square (px) */

static struct {
    int sid;
    int w, h;
    uint8_t *px; /* w * h, 8-bit gray (PSRAM) */
    uint8_t *tmp; /* decode scratch (PSRAM) */
    size_t tmp_cap;
    lv_image_dsc_t dsc;
    lv_obj_t *img;
    lv_timer_t *credit_timer;
    struct {
        lv_area_t a;
        uint8_t hint;
    } dirty[DIRTY_MAX];
    int ndirty;
    osal_mutex_t *lock;
    uint64_t opened_us, tap_us;
    bool first_shown, waiting_display, first_pull;
    unsigned updates;
} RS;

static const ink_refresh_t HINTS[] = {INK_REFRESH_FAST, INK_REFRESH_UI, INK_REFRESH_FULL};

/* ------------------------------------------------------------ down: regions */

typedef struct region_job {
    struct region_job *next;
    uint8_t flags;
    size_t len;
    uint8_t data[];
} region_job_t;

/* Regions waiting for the media executor: one job drains them all (an update can be dozens of
 * regions; one closure each would overflow the executor's queue). */
static region_job_t *g_head, *g_tail;
static bool g_draining;

static void send_credit(int n) {
    uint8_t c[5] = {(uint8_t)n, 0, 0, 0x10, 0}; /* up to 1 MiB per update */
    ink_rt_send(RS.sid, INK_RT_CREDIT, c, sizeof c, 0);
}

static void credit_check(lv_timer_t *t) { /* UI task: pull the first screen once the session is up, then one
                                           * more each time the panel has shown the last update */
    if (RS.first_pull) {
        if (!ink_rt_ready(RS.sid)) return;
        RS.first_pull = false;
        lv_timer_pause(t);
        send_credit(2);
        return;
    }
    if (ink_display_busy() || ink_ui_busy()) return;
    lv_timer_pause(t);
    RS.waiting_display = false;
    send_credit(1);
}

static void show_update(void *arg) { /* UI task */
    (void)arg;
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    int n = RS.ndirty;
    typeof(RS.dirty[0]) d[DIRTY_MAX];
    memcpy(d, RS.dirty, sizeof d[0] * (size_t)n);
    RS.ndirty = 0;
    osal_mutex_unlock(RS.lock);
    if (!RS.img) return;
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
    RS.waiting_display = true;
    lv_timer_resume(RS.credit_timer);
}

static void decode_region(region_job_t *j) { /* exec.media */
    const uint8_t *p = j->data;
    if (j->len < 10 || !RS.px) goto out;
    int x = p[0] | p[1] << 8, y = p[2] | p[3] << 8, w = p[4] | p[5] << 8, h = p[6] | p[7] << 8, enc = p[8], hint = p[9];
    if (w <= 0 || h <= 0 || x + w > RS.w || y + h > RS.h) {
        ink_probe_count("remote.bad_regions");
        goto out;
    }
    size_t px = (size_t)w * (size_t)h, packed = (px + 1) / 2;
    if (RS.tmp_cap < px) {
        osal_free(RS.tmp);
        RS.tmp = osal_malloc(px, OSAL_MEM_EXTERNAL);
        RS.tmp_cap = RS.tmp ? px : 0;
        if (!RS.tmp) goto out;
    }
    bool ok = false;
    if (enc == 2) ok = hal_jpeg_decode_gray(p + 10, j->len - 10, RS.tmp, w, h) == 0; /* 8-bit gray */
    else {
        const uint8_t *nib = p + 10;
        if (enc == 1) { /* into the back half of the scratch, then widened from the front */
            nib = RS.tmp + px - packed;
            ok = ink_lz4_decode(p + 10, j->len - 10, RS.tmp + px - packed, packed) == (int)packed;
        } else ok = j->len - 10 >= packed;
        for (size_t i = 0; ok && i < px; i++) /* 4-bit -> 8-bit, in place from the front */
            RS.tmp[i] = (uint8_t)(((nib[i / 2] >> (i & 1 ? 0 : 4)) & 15) * 17);
    }
    if (!ok) {
        ink_probe_count("remote.bad_regions");
        goto out;
    }
    for (int r = 0; r < h; r++) memcpy(RS.px + (size_t)(y + r) * (size_t)RS.w + (size_t)x, RS.tmp + (size_t)r * (size_t)w, (size_t)w);
    ink_probe_value("remote.region_bytes", (double)j->len);
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    if (RS.ndirty < DIRTY_MAX) {
        RS.dirty[RS.ndirty].a = (lv_area_t){x, y, x + w - 1, y + h - 1};
        RS.dirty[RS.ndirty].hint = (uint8_t)hint;
        RS.ndirty++;
    } else RS.dirty[DIRTY_MAX - 1].a = (lv_area_t){0, 0, RS.w - 1, RS.h - 1}; /* too many: redraw all */
    osal_mutex_unlock(RS.lock);
out:
    if (j->flags & INK_RT_F_LAST) ink_exec_post(INK_EXEC_UI, show_update, NULL);
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
        } else g_draining = false;
        osal_mutex_unlock(RS.lock);
        if (!j) return;
        decode_region(j);
    }
}

static void on_frame(void *ctx, int sid, uint8_t kind, const uint8_t *payload, size_t len, uint16_t seq, uint32_t t_ms, uint8_t flags) {
    (void)ctx;
    (void)seq;
    (void)t_ms;
    if (sid != RS.sid || kind != INK_RT_SCREEN) return; /* hub task */
    region_job_t *j = osal_malloc(sizeof *j + len, OSAL_MEM_EXTERNAL);
    if (!j) return;
    j->next = NULL;
    j->flags = flags;
    j->len = len;
    memcpy(j->data, payload, len);
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    if (g_tail) g_tail->next = j;
    else g_head = j;
    g_tail = j;
    bool post = !g_draining;
    g_draining = true;
    osal_mutex_unlock(RS.lock);
    if (post && ink_exec_post(INK_EXEC_MEDIA, drain, NULL) != 0) {
        ink_probe_count("remote.regions_dropped");
        osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
        g_draining = false;
        osal_mutex_unlock(RS.lock);
    }
}

/* ------------------------------------------------------------ up: input */

static void tapped(lv_event_t *e) { /* UI task */
    (void)e;
    lv_point_t pt;
    lv_indev_get_point(lv_indev_active(), &pt);
    RS.tap_us = osal_now_us();
    lv_area_t a = {pt.x - FEEDBACK, pt.y - FEEDBACK, pt.x + FEEDBACK, pt.y + FEEDBACK};
    if (a.x1 < 0) a.x1 = 0;
    if (a.y1 < 0) a.y1 = 0;
    if (a.x2 >= RS.w) a.x2 = RS.w - 1;
    if (a.y2 >= RS.h) a.y2 = RS.h - 1;
    for (int y = a.y1; y <= a.y2; y++) /* local feedback: invert the spot now, the hub's answer follows */
        for (int x = a.x1; x <= a.x2; x++) RS.px[(size_t)y * (size_t)RS.w + (size_t)x] ^= 0xFF;
    ink_ui_hint_area(&a, INK_REFRESH_A2); /* black and white, the fastest waveform */
    lv_obj_invalidate_area(RS.img, &a);
    ink_ui_cause(RS.tap_us, "remote.tap_feedback_visible_ms");
    uint8_t in[5] = {1, (uint8_t)pt.x, (uint8_t)(pt.x >> 8), (uint8_t)pt.y, (uint8_t)(pt.y >> 8)};
    ink_rt_send(RS.sid, INK_RT_INPUT, in, sizeof in, 0);
    ink_probe_count("remote.taps");
}

/* ------------------------------------------------------------ the activity */

static const ink_rt_handler_t RT = {.on_frame = on_frame};

static void remote_create(lv_obj_t *scr, void *arg) {
    const char *url = arg ? arg : "about:home";
    if (!RS.lock) RS.lock = osal_mutex_create();
    RS.w = lv_display_get_horizontal_resolution(NULL);
    RS.h = lv_display_get_vertical_resolution(NULL);
    if (!RS.px) RS.px = osal_malloc((size_t)RS.w * (size_t)RS.h, OSAL_MEM_EXTERNAL);
    if (!RS.px) return;
    memset(RS.px, 0xFF, (size_t)RS.w * (size_t)RS.h);
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
    RS.credit_timer = lv_timer_create(credit_check, 40, NULL);
    RS.first_shown = RS.waiting_display = false;
    RS.first_pull = true;
    RS.ndirty = 0;
    RS.tap_us = 0;
    RS.opened_us = osal_now_us();
    char params[160];
    snprintf(params, sizeof params, "{\"w\":%d,\"h\":%d,\"gray\":16,\"url\":\"%s\"}", RS.w, RS.h, url);
    RS.sid = ink_rt_open("browser.session", params, &RT); /* the first pull goes out once the hub confirmed it */
}

static void remote_destroy(void) {
    if (RS.sid > 0) ink_rt_close(RS.sid);
    RS.sid = 0;
    if (RS.credit_timer) lv_timer_delete(RS.credit_timer);
    RS.credit_timer = NULL;
    RS.img = NULL;
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
