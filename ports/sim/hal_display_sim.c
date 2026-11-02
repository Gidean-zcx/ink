/*
 * Display model: an EPDiy-style e-ink panel (or an LCD) with realistic timing,
 * CPU load and memory footprint. Defaults follow the upstream Plus pipeline.
 *
 * E-ink update pipeline (all numbers from the device profile):
 *   submit   caller converts+copies the area into the driver framebuffer
 *            (CPU + PSRAM bandwidth charged to the caller), request queued
 *   power    if the HV rails are off: PMIC power-up (~140 ms on the Plus;
 *            holds the shared I2C bus for part of it -> touch reads wait)
 *   diff     upstream epd_hl_update_area builds a difference image over the
 *            full-width rows of the area (PSRAM in, PSRAM out) — driver task
 *   play     phases x frame_time; EVERY update scans the whole panel
 *            (full_frame_scan). Two max-priority feeder tasks with PSRAM
 *            stacks keep the prepared-line queue topped up and spin until the
 *            last line of each phase is queued (derived duty); the LCD
 *            bounce-buffer interrupt adds load on core 0. A full-screen
 *            flashing GC16 uses the complete (48-phase) table.
 *   copy     back-buffer update of the dirty full-width rows (PSRAM->PSRAM)
 *   done     pixels stable at the end of the waveform -> done callback (from
 *            the driver task, after the copy); rails off right away unless
 *            hal_display_hold_power() / hal_display_wake() asked otherwise.
 *   wake     hal_display_wake() queues a power-up ahead of any update (the
 *            input service calls it on touch/key edges), so the ~140 ms
 *            overlaps rendering; unused wakes cost rail current only.
 *
 * Underruns: while a flash operation freezes the feeders the LCD DMA keeps
 * draining the line queue (one line per line time); between stalls the
 * feeders refill it at their net production rate. If it runs dry the panel
 * shows garbage: upstream then recovers with epd_clear + a complete GC16
 * (about 1.9 s), which is charged here too and counted in display.underruns.
 *
 * The panel state is modelled for screenshots: the physical image plus a
 * ghosting residual that grows with every non-flashing update and is cleared
 * by flashing GC16 updates.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "ink/hal_display.h"
#include "ink/rotation.h"
#include "sim.h"
#include "stb_image_write.h"

#define QUEUE_LEN 16
#define FEEDER_STACK 3072
#define DRIVER_STACK 4096
#define DRIVER_PRIO 20

typedef struct {
    uint32_t id;
    hal_rect_t area;
    hal_waveform_t wf;
    uint32_t flags;
    hal_display_done_fn done;
    void *ctx;
    uint64_t queued_ns;
    uint32_t wake_ms; /* != 0: not an update, power up and keep the rails for this long */
} req_t;

static const char *WFN[HAL_WF_COUNT] = {"GC16", "GL16", "DU", "A2"};

static struct {
    hal_panel_info_t info;
    int W, H;
    uint8_t *target;   /* driver framebuffer: latest submitted content (host copy) */
    uint8_t *screen;   /* physical panel image */
    float *residual;   /* ghosting residual added to the visible image */
    uint8_t *applying; /* snapshot of the area being driven */
    osal_queue_t *q;
    osal_task_t *task;
    uint32_t next_id;
    int pending;
    bool rails_on;
    uint32_t hold_ms;
    uint64_t last_done_ns;  /* end of the last update (hold_ms counts from here) */
    uint64_t wake_until_ns; /* rails requested up until (0 = no pending wake) */
    int temperature;
    /* feeders */
    vtk_waitq_t feed_wq;
    bool playing;
    uint64_t play_start, play_frames, frame_ns;
    int frame_no;
    uint64_t worst_stall_in_play; /* longest flash stall that overlapped the current waveform */
    double q_level;               /* prepared lines in the queue at q_t */
    uint64_t q_t;
    double q_min;                 /* lowest level during the current waveform */
    double refill_per_ns;         /* net queue refill rate while the feeders run */
    bool underrun;
    uint64_t underrun_at;
    /* stats */
    uint64_t busy_ns;
    unsigned wf_fallback_warned;
} D;

/* ------------------------------------------------------------------ helpers */

static bool clip(hal_rect_t *r) {
    if (r->x < 0) { r->w += r->x; r->x = 0; }
    if (r->y < 0) { r->h += r->y; r->y = 0; }
    if (r->x + r->w > D.W) r->w = D.W - r->x;
    if (r->y + r->h > D.H) r->h = D.H - r->y;
    return r->w > 0 && r->h > 0;
}

static hal_waveform_t resolve_wf(hal_waveform_t wf) {
    if (D.info.tech == HAL_PANEL_LCD) return wf;
    hal_waveform_t chain[HAL_WF_COUNT][4] = {
        [HAL_WF_GC16] = {HAL_WF_GC16, HAL_WF_GC16, HAL_WF_GC16, HAL_WF_GC16},
        [HAL_WF_GL16] = {HAL_WF_GL16, HAL_WF_GC16, HAL_WF_GC16, HAL_WF_GC16},
        [HAL_WF_DU] = {HAL_WF_DU, HAL_WF_GL16, HAL_WF_GC16, HAL_WF_GC16},
        [HAL_WF_A2] = {HAL_WF_A2, HAL_WF_DU, HAL_WF_GL16, HAL_WF_GC16},
    };
    for (int i = 0; i < 4; i++) {
        hal_waveform_t c = chain[wf][i];
        if (g_profile.wf[c].supported) {
            if (c != wf && !(D.wf_fallback_warned & (1u << wf))) {
                D.wf_fallback_warned |= 1u << wf;
                sim_note("display: waveform %s not available on this panel, using %s", WFN[wf], WFN[c]);
            }
            return c;
        }
    }
    return HAL_WF_GC16;
}

static void visible_image(uint8_t *out_logical, int *lw, int *lh) {
    int rot = D.info.rotation_deg;
    ink_logical_size(D.W, D.H, rot, lw, lh);
    for (int ny = 0; ny < D.H; ny++)
        for (int nx = 0; nx < D.W; nx++) {
            size_t i = (size_t)ny * D.W + nx;
            float v = (float)D.screen[i] + D.residual[i];
            if (D.info.tech == HAL_PANEL_EPD && D.info.gray_levels > 1 && D.info.gray_levels < 256) {
                /* the panel only shows gray_levels levels */
                float step = 255.0f / (float)(D.info.gray_levels - 1);
                v = (float)(int)(v / step + 0.5f) * step;
            }
            uint8_t px = v < 0 ? 0 : v > 255 ? 255 : (uint8_t)v;
            int lx, ly;
            ink_native_to_logical(D.W, D.H, rot, nx, ny, &lx, &ly);
            out_logical[(size_t)ly * (size_t)*lw + (size_t)lx] = px;
        }
}

static void write_png(const char *path) {
    int lw, lh;
    uint8_t *img = malloc((size_t)D.W * D.H);
    if (!img) return;
    visible_image(img, &lw, &lh);
    if (!stbi_write_png(path, lw, lh, 1, img, lw)) sim_warn("cannot write %s", path);
    free(img);
}

void sim_display_snapshot(const char *name) {
    if (!g_sim.out_dir || !D.screen) return;
    char path[512];
    snprintf(path, sizeof path, "%s/snap-%s.png", g_sim.out_dir, name);
    write_png(path);
    sim_note("snapshot %s", path);
}

double sim_display_ghost_level(void) {
    if (!D.residual) return 0;
    double sum = 0;
    size_t n = (size_t)D.W * D.H;
    for (size_t i = 0; i < n; i++) sum += D.residual[i] < 0 ? -D.residual[i] : D.residual[i];
    return sum / (double)n;
}

/* ------------------------------------------------------------------ feeders */

static void feeder_task(void *arg) {
    int core = (int)(intptr_t)arg;
    for (;;) {
        vtk_lock();
        while (!D.playing) vtk_block_locked(&D.feed_wq, VTK_TIME_INF);
        uint64_t start = D.play_start, frames = D.play_frames, frame = D.frame_ns;
        vtk_unlock();
        double duty = g_profile.prep_cpu_duty + (core == 0 ? g_profile.lcd_isr_duty : 0.0);
        if (duty > 1.0) duty = 1.0;
        for (uint64_t f = 0; f < frames; f++) {
            uint64_t fs = start + f * frame, fe = fs + frame;
            uint64_t now = vtk_now();
            if (now >= fe) continue; /* frozen through this phase: the DMA scanned it without us */
            if (now < fs) vtk_sleep_ns(fs - now);
            /* prepare + push this phase's lines: busy (spinning on the line queue at max priority) */
            now = vtk_now();
            uint64_t busy = (uint64_t)(duty * (double)frame);
            if (now + busy > fe) busy = fe > now ? fe - now : 0;
            sim_charge_ns_always(busy);
            vtk_sched_point();
            now = vtk_now();
            if (now < fe) vtk_sleep_ns(fe - now);
        }
        vtk_lock();
        /* wait for the driver to clear `playing` (it does at the end of the waveform) */
        while (D.playing && D.play_start == start) vtk_block_locked(&D.feed_wq, VTK_TIME_INF);
        vtk_unlock();
    }
}

/* Flash stall hook (kernel lock held): the feeders are frozen for [from, until) while the LCD DMA
 * keeps consuming prepared lines. */
static void display_stall_hook(uint64_t from, uint64_t until) {
    if (!D.playing || D.info.tech != HAL_PANEL_EPD) return;
    uint64_t ps = D.play_start, pe = D.play_start + D.play_frames * D.frame_ns;
    uint64_t a = from > ps ? from : ps, b = until < pe ? until : pe;
    if (b <= a) return;
    double Q = (double)g_profile.line_queue_lines;
    double lvl = D.q_level;
    if (a > D.q_t) lvl += (double)(a - D.q_t) * D.refill_per_ns;
    if (lvl > Q) lvl = Q;
    double drain = (double)(b - a) / (double)(g_profile.line_ns ? g_profile.line_ns : 17000);
    if (drain >= lvl) {
        if (!D.underrun) {
            D.underrun = true;
            D.underrun_at = a + (uint64_t)(lvl * (double)g_profile.line_ns);
        }
        lvl = 0;
    } else {
        lvl -= drain;
    }
    D.q_level = lvl;
    D.q_t = b;
    if (lvl < D.q_min) D.q_min = lvl;
    if (b - a > D.worst_stall_in_play) D.worst_stall_in_play = b - a;
}

/* ------------------------------------------------------------------ driver */

static void power_up(void) {
    if (D.rails_on) return;
    uint64_t t0 = vtk_now();
    uint64_t pause = SIM_US(g_profile.touch_pause_us);
    uint64_t total = SIM_US(g_profile.power_up_us);
    if (pause > total) pause = total;
    if (pause) sim_i2c_hold("pmic", pause); /* PMIC register writes + PG polling hold the shared bus */
    uint64_t now = vtk_now();
    if (now < t0 + total) vtk_sleep_ns(t0 + total - now);
    D.rails_on = true;
    sim_power_consumer("display.rails", true, 0);
    sim_trace_span("display", "power-up", t0, vtk_now(), NULL);
    sim_metric_add("display.power_ups", 1);
}

static void power_down(void) {
    if (!D.rails_on) return;
    uint64_t t0 = vtk_now();
    vtk_sleep_ns(SIM_US(g_profile.power_down_us));
    D.rails_on = false;
    sim_power_consumer("display.rails", false, 0);
    sim_trace_span("display", "power-down", t0, vtk_now(), NULL);
}

static void apply_area(const req_t *r, hal_waveform_t wf) {
    const profile_waveform_t *pw = &g_profile.wf[wf];
    for (int y = r->area.y; y < r->area.y + r->area.h; y++)
        for (int x = r->area.x; x < r->area.x + r->area.w; x++) {
            size_t i = (size_t)y * D.W + x;
            uint8_t old = D.screen[i];
            uint8_t nv = D.applying[(size_t)(y - r->area.y) * r->area.w + (x - r->area.x)];
            if (D.info.tech == HAL_PANEL_EPD && (wf == HAL_WF_DU || wf == HAL_WF_A2)) nv = nv < 128 ? 0 : 255; /* 2-level */
            if (D.info.tech == HAL_PANEL_LCD || pw->flashing) {
                D.residual[i] = 0;
            } else if (old != nv) {
                D.residual[i] += (float)pw->ghost * (float)((int)old - (int)nv);
                if (D.residual[i] > 64) D.residual[i] = 64;
                if (D.residual[i] < -64) D.residual[i] = -64;
            }
            D.screen[i] = nv;
        }
}

/* Play `phases` frames, DMA-paced from now. Returns true if the line queue ran dry. */
static bool play(int phases) {
    uint64_t started = vtk_now();
    D.frame_ns = SIM_US(g_profile.frame_time_us);
    vtk_lock();
    D.play_start = started;
    D.play_frames = (uint64_t)phases;
    D.worst_stall_in_play = 0;
    D.q_level = (double)g_profile.line_queue_lines;
    D.q_t = started;
    D.q_min = D.q_level;
    D.underrun = false;
    D.playing = true;
    if (g_profile.prep_cpu_duty > 0) vtk_wake_all_locked(&D.feed_wq);
    vtk_unlock();
    /* DMA-paced scan: hardware time runs from `started` even if the feeders preempt this task */
    uint64_t end = started + (uint64_t)phases * D.frame_ns;
    uint64_t now = vtk_now();
    if (now < end) vtk_sleep_ns(end - now);
    vtk_lock();
    D.playing = false;
    bool underrun = D.underrun;
    if (g_profile.prep_cpu_duty > 0) vtk_wake_all_locked(&D.feed_wq);
    vtk_unlock();
    sim_metric_max("display.line_queue_min_lines_worst", (double)g_profile.line_queue_lines - D.q_min);
    return underrun;
}

/* Upstream difference image (before) and back-buffer copy (after): full-width rows of the area. */
static void diff_pass(const hal_rect_t *a) {
    if (!g_profile.diff_pass) return;
    double px = (double)a->h * (double)(g_profile.diff_pass == 2 ? D.W : a->w);
    uint64_t t0 = vtk_now();
    /* compute + PSRAM traffic: read front and back (4 bpp each), write the 1-byte-per-pixel difference */
    sim_charge_ns_always(sim_cycles_ns(px * (profile_cost("epd.diff_px") + profile_cost("psram_to_internal.copy_byte") +
                                             profile_cost("internal_to_psram.copy_byte"))));
    vtk_sched_point();
    sim_trace_span("display", "diff", t0, vtk_now(), NULL);
}

static void back_copy(const hal_rect_t *a) {
    if (!g_profile.diff_pass) return;
    double bytes = (double)a->h * (double)(g_profile.diff_pass == 2 ? D.W : a->w) / 2.0;
    uint64_t t0 = vtk_now();
    sim_charge_ns_always(sim_cycles_ns(bytes * profile_cost("psram.copy_byte")));
    vtk_sched_point();
    sim_trace_span("display", "backcopy", t0, vtk_now(), NULL);
}

static void execute(req_t *r) {
    hal_display_result_t res = {.id = r->id, .ok = true, .queued_us = r->queued_ns / 1000};
    hal_waveform_t wf = resolve_wf(r->wf);
    res.waveform = wf;
    uint64_t t_pick = vtk_now();
    /* EPDiy drives from the framebuffer as it is when the update starts */
    for (int y = 0; y < r->area.h; y++)
        memcpy(D.applying + (size_t)y * r->area.w, D.target + (size_t)(r->area.y + y) * D.W + r->area.x, (size_t)r->area.w);

    bool full_area = r->area.x == 0 && r->area.y == 0 && r->area.w == D.W && r->area.h == D.H;
    uint64_t started, done;
    bool recovered = false;
    if (D.info.tech == HAL_PANEL_LCD) {
        started = vtk_now();
        vtk_sleep_ns(SIM_US(g_profile.lcd_frame_us));
        done = vtk_now();
    } else {
        power_up();
        sim_i2c_transfer("io-expander", 3); /* panel control lines on the TCA9555 at waveform start */
        diff_pass(&r->area);
        int phases = g_profile.wf[wf].phases;
        if (wf == HAL_WF_GC16 && full_area && g_profile.gc16_full_phases > phases) phases = g_profile.gc16_full_phases;
        started = vtk_now();
        sim_power_consumer("display.refresh", true, g_profile.ma_display_refresh);
        bool underrun = play(phases);
        if (underrun) {
            sim_metric_add("display.underruns", 1);
            sim_warn("display: the prepared-line queue ran dry at %.3f ms during a %s waveform (a flash operation froze the "
                     "feeders; queue %d lines = %.2f ms) -> garbage on the panel; recovering with clear + full GC16",
                     D.underrun_at / 1e6, WFN[wf], g_profile.line_queue_lines, g_profile.line_queue_us / 1e3);
            uint64_t t_rec = vtk_now();
            (void)play(g_profile.clear_frames);
            (void)play(g_profile.gc16_full_phases);
            sim_trace_span("display", "underrun-recovery", t_rec, vtk_now(), NULL);
            sim_metric_sample("display.underrun_recovery_ms", (vtk_now() - t_rec) / 1e6);
            recovered = true;
        }
        done = vtk_now();
        sim_power_consumer("display.refresh", false, g_profile.ma_display_refresh);
        back_copy(&r->area);
        sim_i2c_transfer("io-expander", 3);
    }
    if (recovered) {
        /* the whole panel now shows the framebuffer after a flashing GC16 */
        memcpy(D.applying, D.target, (size_t)D.W * D.H);
        req_t all = *r;
        all.area = (hal_rect_t){0, 0, D.W, D.H};
        apply_area(&all, HAL_WF_GC16);
    } else {
        apply_area(r, wf);
    }
    res.started_us = started / 1000;
    res.done_us = done / 1000;
    D.busy_ns += vtk_now() - t_pick;

    char key[80], args[200];
    snprintf(key, sizeof key, "display.updates.%s", WFN[wf]);
    sim_metric_add(key, 1);
    snprintf(key, sizeof key, "display.update_ms.%s", WFN[wf]);
    sim_metric_sample(key, (done - r->queued_ns) / 1e6);
    sim_metric_sample("display.queue_wait_ms", (t_pick - r->queued_ns) / 1e6);
    double ghost = sim_display_ghost_level();
    sim_metric_max("display.ghost_level_max", ghost);
    snprintf(args, sizeof args, "{\"id\":%u,\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"ghost\":%.2f}", r->id, r->area.x,
             r->area.y, r->area.w, r->area.h, ghost);
    if (t_pick > r->queued_ns) sim_trace_span("display", "queued", r->queued_ns, t_pick, NULL);
    sim_trace_span("display", WFN[wf], started, done, args);
    if (g_sim.frames && g_sim.out_dir) {
        char path[512];
        snprintf(path, sizeof path, "%s/frames/%05u_%08.1fms_%s.png", g_sim.out_dir, r->id, done / 1e6, WFN[wf]);
        write_png(path);
    }
    D.pending--;
    if (r->done) r->done(r->ctx, &res);
}

static void driver_task(void *arg) {
    (void)arg;
    req_t r;
    for (;;) {
        osal_err_t e;
        if (D.rails_on) {
            /* rails stay up for hold_ms after the last update and until a pending wake expires
             * (exact: DMA/timer driven), then go down */
            uint64_t until = D.hold_ms ? D.last_done_ns + (uint64_t)D.hold_ms * 1000000ull : 0;
            if (D.wake_until_ns > until) until = D.wake_until_ns;
            uint64_t now = vtk_now();
            if (until <= now) {
                if (D.wake_until_ns && D.wake_until_ns <= now) sim_metric_add("display.wakes_unused", 1);
                D.wake_until_ns = 0;
                power_down();
                continue;
            }
            e = osal_queue_recv(D.q, &r, (int64_t)((until - now + 999) / 1000));
            if (e != OSAL_OK) continue; /* deadline reached: re-evaluate */
        } else {
            e = osal_queue_recv(D.q, &r, OSAL_WAIT_FOREVER);
            if (e != OSAL_OK) continue;
        }
        if (r.wake_ms) {
            uint64_t until = r.queued_ns + (uint64_t)r.wake_ms * 1000000ull;
            if (until > D.wake_until_ns) D.wake_until_ns = until;
            if (!D.rails_on) {
                sim_metric_add("display.wakes", 1);
                power_up();
            }
            continue;
        }
        execute(&r);
        D.last_done_ns = vtk_now();
        D.wake_until_ns = 0; /* the wake served its purpose; hold_ms decides from here */
    }
}

/* -------------------------------------------------------------------- API */

void hal_display_get_info(hal_panel_info_t *out) { *out = D.info; }

uint32_t hal_display_update(const hal_rect_t *area, const uint8_t *gray8, int stride, hal_waveform_t wf, uint32_t flags,
                            hal_display_done_fn done, void *ctx) {
    if (vtk_in_isr() || !vtk_self()) osal_panic("hal_display_update must be called from a task");
    if (!area || !gray8 || wf >= HAL_WF_COUNT) return 0;
    hal_rect_t r = *area;
    int dx = r.x < 0 ? -r.x : 0, dy = r.y < 0 ? -r.y : 0;
    if (!clip(&r)) return 0;
    const uint8_t *src = gray8 + (size_t)dy * stride + dx;

    /* convert + copy into the framebuffer: compute charged as CPU work, PSRAM traffic always charged */
    uint64_t px = (uint64_t)r.w * (uint64_t)r.h;
    sim_charge_cycles((double)px * profile_cost("epd.convert_px"));
    sim_charge_ns_always(sim_cycles_ns((double)px * 0.75 * profile_cost("psram.copy_byte")));
    for (int y = 0; y < r.h; y++) memcpy(D.target + (size_t)(r.y + y) * D.W + r.x, src + (size_t)y * stride, (size_t)r.w);

    req_t q = {.id = ++D.next_id, .area = r, .wf = wf, .flags = flags, .done = done, .ctx = ctx, .queued_ns = vtk_now()};
    D.pending++;
    osal_err_t e = (flags & HAL_UPD_FLAG_PRIORITY) ? osal_queue_send_front(D.q, &q, OSAL_WAIT_FOREVER)
                                                   : osal_queue_send(D.q, &q, OSAL_WAIT_FOREVER);
    if (e != OSAL_OK) {
        D.pending--;
        return 0;
    }
    return q.id;
}

int hal_display_pending(void) { return D.pending; }

void hal_display_set_temperature(int celsius) { D.temperature = celsius; }
void hal_display_hold_power(uint32_t ms) { D.hold_ms = D.info.tech == HAL_PANEL_EPD ? ms : 0; }

void hal_display_wake(uint32_t ms) {
    if (D.info.tech != HAL_PANEL_EPD || !D.q || !ms) return;
    if (vtk_in_isr() || !vtk_self()) osal_panic("hal_display_wake must be called from a task or timer callback");
    req_t q = {.wake_ms = ms, .queued_ns = vtk_now()};
    /* never blocks: with a full queue an update is already waiting and will power up anyway */
    (void)osal_queue_send_front(D.q, &q, 0);
}

void sim_display_init(void) {
    D.info = g_profile.panel;
    D.W = D.info.width;
    D.H = D.info.height;
    size_t n = (size_t)D.W * D.H;
    D.target = malloc(n);
    D.screen = malloc(n);
    D.applying = malloc(n);
    D.residual = calloc(n, sizeof(float));
    if (!D.target || !D.screen || !D.applying || !D.residual) abort();
    memset(D.target, 255, n);
    memset(D.screen, 255, n);
    D.temperature = 25;

    /* the real driver's buffers (framebuffers, LUTs, line queues) come out of the device heaps */
    const char *prev = osal_mem_tag("display-driver");
    void *ext = g_profile.panel_buffers_external ? osal_malloc(g_profile.panel_buffers_external, OSAL_MEM_EXTERNAL) : (void *)1;
    void *in = g_profile.panel_buffers_internal ? osal_malloc(g_profile.panel_buffers_internal, OSAL_MEM_INTERNAL) : (void *)1;
    if (!ext || !in) osal_panic("display driver init failed: cannot reserve its buffers (%zu KiB external, %zu KiB internal)",
                                g_profile.panel_buffers_external / 1024, g_profile.panel_buffers_internal / 1024);
    {
        double prod = g_profile.line_prep_ns ? 2.0 / (double)g_profile.line_prep_ns : 0;
        double cons = g_profile.line_ns ? 1.0 / (double)g_profile.line_ns : 0;
        D.refill_per_ns = prod > cons ? prod - cons : cons * 0.01;
        vtk_set_stall_hook(display_stall_hook);
    }
    D.q = osal_queue_create(sizeof(req_t), QUEUE_LEN);
    if (!D.q) osal_panic("display driver init failed: no memory for its request queue");
    osal_task_cfg_t cfg = {.name = "epd", .fn = driver_task, .stack_bytes = DRIVER_STACK, .priority = DRIVER_PRIO, .core = -1};
    if (osal_task_create(&cfg, &D.task) != OSAL_OK) osal_panic("display driver init failed: cannot create its task");
    if (D.info.tech == HAL_PANEL_EPD && g_profile.prep_cpu_duty > 0) {
        for (int c = 0; c < g_profile.cores && c < 2; c++) {
            char name[16];
            snprintf(name, sizeof name, "epd_prep%d", c);
            osal_task_cfg_t fc = {.name = name, .fn = feeder_task, .arg = (void *)(intptr_t)c, .stack_bytes = FEEDER_STACK,
                                  .priority = OSAL_PRIO_MAX, .core = c, .flags = OSAL_TASK_STACK_EXTERNAL};
            if (osal_task_create(&fc, NULL) != OSAL_OK) osal_panic("display driver init failed: cannot create %s", name);
        }
    }
    osal_mem_tag(prev);
    if (g_sim.frames && g_sim.out_dir) {
        char dir[512];
        snprintf(dir, sizeof dir, "%s/frames", g_sim.out_dir);
        mkdir(dir, 0755);
    }
}

void sim_display_report(void) {
    sim_metric_set("display.busy_ms", D.busy_ns / 1e6);
    sim_metric_set("display.ghost_level_final", sim_display_ghost_level());
    if (g_sim.out_dir && D.screen) {
        char path[512];
        snprintf(path, sizeof path, "%s/final.png", g_sim.out_dir);
        write_png(path);
    }
}
