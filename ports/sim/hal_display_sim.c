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
 *   play     every phase scans the whole panel (full_frame_scan). Two
 *            max-priority feeder tasks (PSRAM stacks) convert the lines into
 *            per-feeder queues — fixed odd/even ownership (upstream) or the
 *            next free feeder — and the LCD bounce-buffer interrupt pulls them
 *            (on core 0, which slows feeder 0). The scan starts once the
 *            prefill lines are ready; each phase is planned line by line
 *            (line cost: full line, untouched line, or only the area's
 *            columns), so a feeder slower than the DMA, a too-small prefill
 *            or a flash stall shows up as a late line. Feeders hold their
 *            core until their last line is queued (spinning on a full queue,
 *            as upstream) or only while computing ("block"). The next phase
 *            starts after the frame-done overhead, padded to phase_min. A
 *            full-screen flashing GC16 uses the complete (48-phase) table.
 *   copy     back-buffer update of the dirty full-width rows (PSRAM->PSRAM)
 *   done     pixels stable at the end of the waveform -> done callback (from
 *            the driver task, after the copy); rails off right away unless
 *            hal_display_hold_power() / hal_display_wake() asked otherwise.
 *   wake     hal_display_wake() queues a power-up ahead of any update (the
 *            input service calls it on touch/key edges), so the ~140 ms
 *            overlaps rendering; unused wakes cost rail current only.
 *
 * Underruns: a line not ready when the interrupt needs it means garbage on
 * the panel: upstream aborts the waveform and recovers with epd_clear + a
 * complete GC16 (about 1.9 s), charged here too and counted in
 * display.underruns. A flash operation freezes both feeders; the refill
 * interrupt keeps pulling lines only if it is IRAM-safe (port decision),
 * otherwise it is masked and anything longer than one bounce buffer is
 * garbage. Flash stalls are taken by virtual time, whenever they were
 * reported.
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
    uint32_t phase_seq;    /* bumped for every phase the feeders work on */
    struct plan *plan;     /* the current phase (feeders read it) */
    bool stall_dirty;      /* a flash stall was reported while playing: re-plan */
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

#define MAX_STALLS 32
#define MAX_LINES 4096

typedef struct {
    uint64_t a, b;
} span_t;

static struct { /* recent flash stalls by virtual time (kernel lock) */
    span_t s[MAX_STALLS];
    int next;
} ST_RING;

typedef struct plan {
    uint64_t start;       /* phase start (feeders notified) */
    uint64_t scan_start;  /* the LCD DMA starts (prefill ready) */
    uint64_t scan_end;
    uint64_t next_start;  /* the next phase can start */
    uint64_t busy_end[2]; /* feeder f holds its core until (spin) */
    uint64_t cpu_ns[2];   /* feeder f computes this long */
    bool late;            /* a line was not ready in time: garbage */
    int late_line;
    uint64_t late_at;
    double min_slack_lines; /* (needed - ready) / line time, minimum over the phase */
} plan_t;

static plan_t PLAN; /* kernel lock */

/* Flash stall hook (kernel lock held): remember it; a playing phase is re-planned. */
static void display_stall_hook(uint64_t from, uint64_t until) {
    if (D.info.tech != HAL_PANEL_EPD || !g_profile.feeders_cpu) return;
    ST_RING.s[ST_RING.next] = (span_t){from, until};
    ST_RING.next = (ST_RING.next + 1) % MAX_STALLS;
    if (D.playing) D.stall_dirty = true;
}

static int span_cmp(const void *x, const void *y) {
    const span_t *a = x, *b = y;
    return a->a < b->a ? -1 : a->a > b->a;
}

/* Stalls that end after t0, sorted and merged. */
static int collect_stalls(uint64_t t0, span_t *out) {
    int n = 0;
    for (int i = 0; i < MAX_STALLS; i++)
        if (ST_RING.s[i].b > t0 && ST_RING.s[i].b > ST_RING.s[i].a) out[n++] = ST_RING.s[i];
    qsort(out, (size_t)n, sizeof *out, span_cmp);
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (m && out[i].a <= out[m - 1].b) {
            if (out[i].b > out[m - 1].b) out[m - 1].b = out[i].b;
        } else {
            out[m++] = out[i];
        }
    }
    return m;
}

/* `work` ns of computing from t: frozen inside stalls. */
static uint64_t run_through(uint64_t t, uint64_t work, const span_t *st, int n) {
    for (int i = 0; i < n; i++) {
        if (st[i].b <= t) continue;
        if (st[i].a > t) {
            if (work <= st[i].a - t) return t + work;
            work -= st[i].a - t;
        }
        t = st[i].b;
    }
    return t + work;
}

/* Plan one phase of an update of native area `a` starting at t0 (kernel lock held). */
static void plan_phase(const hal_rect_t *a, uint64_t t0, plan_t *p) {
    static uint64_t ready[MAX_LINES];
    static int lines_of[2][MAX_LINES];
    const int L = g_profile.line_count < MAX_LINES ? g_profile.line_count : MAX_LINES;
    const uint64_t ln = g_profile.line_ns;
    const int Q = g_profile.line_queue_lines, F = g_profile.prefill_lines < L ? g_profile.prefill_lines : L;
    const int B = g_profile.bounce_lines, lead = 2 * B; /* both bounce buffers are filled at the scan start */
    const double isr = g_profile.lcd_isr_duty;
    const uint64_t full = g_profile.line_prep_ns, skip = g_profile.line_skip_ns;
    const uint64_t area_cost = g_profile.feeder_area_columns ? skip + (full - skip) * (uint64_t)a->w / (uint64_t)D.W : full;
    span_t st[MAX_STALLS];
    int ns = collect_stalls(t0, st);
    uint64_t tf[2] = {t0, t0}, cpu[2] = {0, 0}, scan = UINT64_MAX;
    int cnt[2] = {0, 0};
    memset(p, 0, sizeof *p);
    p->start = t0;
    for (int l = 0; l < L; l++) {
        uint64_t c = (l >= a->y && l < a->y + a->h && l < D.H) ? area_cost : skip;
        uint64_t s[2];
        for (int f = 0; f < 2; f++) {
            s[f] = tf[f];
            if (cnt[f] >= Q) { /* a queue slot frees when the interrupt pulls the feeder's (k-Q)-th line */
                int old = lines_of[f][cnt[f] - Q];
                if (scan == UINT64_MAX) s[f] = UINT64_MAX;
                else {
                    uint64_t pull = scan + (old > lead ? (uint64_t)(old - lead) * ln : 0);
                    if (pull > s[f]) s[f] = pull;
                }
            }
            if (l == F && scan != UINT64_MAX && s[f] < scan) s[f] = scan; /* the trigger line's owner starts the scan first */
        }
        int f = g_profile.feeder_dynamic ? (s[0] <= s[1] ? 0 : 1) : (l & 1);
        if (s[f] == UINT64_MAX) {
            osal_panic("display profile: the prefill (%d lines) does not fit the line queues (%d per feeder)", F, Q);
        }
        double speed = (f == 0 && scan != UINT64_MAX && s[f] >= scan) ? 1.0 - isr : 1.0;
        uint64_t work = (uint64_t)((double)c / speed);
        uint64_t e = run_through(s[f], work, st, ns);
        ready[l] = e;
        tf[f] = e;
        cpu[f] += work;
        lines_of[f][cnt[f]++] = l;
        if (scan == UINT64_MAX && l == F - 1) { /* the whole prefix is ready: the scan starts (not inside a stall) */
            uint64_t m = 0;
            for (int i = 0; i < F; i++)
                if (ready[i] > m) m = ready[i];
            scan = run_through(m, 0, st, ns);
        }
    }
    p->scan_start = scan;
    p->scan_end = scan + (uint64_t)L * ln;
    p->min_slack_lines = 1e9;
    p->late_line = -1;
    for (int l = 0; l < L; l++) {
        uint64_t need = scan + (l > lead ? (uint64_t)(l - lead) * ln : 0);
        double slack = ((double)need - (double)ready[l]) / (double)ln;
        if (l >= F && slack < p->min_slack_lines) p->min_slack_lines = slack; /* the prefix is ready by construction */
        if (ready[l] > need && !p->late) {
            p->late = true;
            p->late_line = l;
            p->late_at = need;
        }
    }
    if (!g_profile.lcd_isr_iram_safe) /* the refill interrupt is masked during flash operations */
        for (int i = 0; i < ns; i++) {
            uint64_t x = st[i].a > scan ? st[i].a : scan, y = st[i].b < p->scan_end ? st[i].b : p->scan_end;
            if (y > x && y - x > (uint64_t)B * ln && (!p->late || x + (uint64_t)B * ln < p->late_at)) {
                p->late = true;
                p->late_line = (int)((x - scan) / ln);
                p->late_at = x + (uint64_t)B * ln;
                p->min_slack_lines = -1;
            }
        }
    for (int f = 0; f < 2; f++) {
        p->busy_end[f] = tf[f];
        p->cpu_ns[f] = cpu[f];
    }
    uint64_t next = p->scan_end + SIM_US(g_profile.frame_overhead_us);
    uint64_t pad = t0 + SIM_US(g_profile.phase_min_us);
    p->next_start = next > pad ? next : pad;
}

static void feeder_task(void *arg) {
    int f = (int)(intptr_t)arg;
    uint32_t seen = 0;
    for (;;) {
        vtk_lock();
        while (D.phase_seq == seen) vtk_block_locked(&D.feed_wq, VTK_TIME_INF);
        seen = D.phase_seq;
        plan_t p = PLAN;
        vtk_unlock();
        uint64_t now = vtk_now();
        if (now >= p.scan_end) continue; /* frozen through this phase: the DMA scanned it without us */
        if (now < p.start) vtk_sleep_ns(p.start - now);
        if (!g_profile.feeder_block) {
            /* convert, and spin on a full queue, until the last line is queued: the core is ours */
            now = vtk_now();
            if (p.busy_end[f] > now) sim_charge_ns_always(p.busy_end[f] - now);
        } else {
            /* compute in slices, blocked while the queue is full: others get the gaps */
            uint64_t span = p.busy_end[f] > p.start ? p.busy_end[f] - p.start : 0, left = p.cpu_ns[f];
            int slices = (int)(span / SIM_MS(1)) + 1;
            for (int i = 0; i < slices && left; i++) {
                uint64_t c = left / (uint64_t)(slices - i);
                sim_charge_ns_always(c);
                left -= c;
                uint64_t until = p.start + span * (uint64_t)(i + 1) / (uint64_t)slices;
                now = vtk_now();
                if (until > now) vtk_sleep_ns(until - now);
            }
        }
        if (f == 0 && g_profile.lcd_isr_duty > 0) {
            /* the refill interrupt keeps taking its share of core 0 for the rest of the scan */
            for (;;) {
                now = vtk_now();
                if (now < p.scan_start) vtk_sleep_ns(p.scan_start - now);
                now = vtk_now();
                if (now >= p.scan_end) break;
                uint64_t slice = p.scan_end - now < SIM_MS(1) ? p.scan_end - now : SIM_MS(1);
                sim_charge_ns_always((uint64_t)((double)slice * g_profile.lcd_isr_duty));
                now = vtk_now();
                uint64_t until = now + (uint64_t)((double)slice * (1.0 - g_profile.lcd_isr_duty));
                if (until > p.scan_end) until = p.scan_end;
                if (until > now) vtk_sleep_ns(until - now);
            }
        }
    }
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

/* Play `phases` phases of an update of native area `a`. Returns false if a line was late (upstream
 * aborts the waveform at that phase). */
static bool play_cpu(int phases, const hal_rect_t *a) {
    uint64_t t0 = vtk_now();
    double worst = 1e9;
    bool ok = true;
    D.playing = true;
    for (int k = 0; k < phases && ok; k++) {
        sim_assets_cache_pollute(); /* the feeders stream PSRAM through the shared D-cache every phase */
        plan_t p;
        vtk_lock();
        D.stall_dirty = false;
        plan_phase(a, t0, &p);
        PLAN = p;
        D.phase_seq++;
        vtk_wake_all_locked(&D.feed_wq);
        vtk_unlock();
        for (;;) { /* until the scan ends; a flash stall reported meanwhile re-plans the phase */
            uint64_t now = vtk_now();
            if (now < p.scan_end) vtk_sleep_ns(p.scan_end - now);
            vtk_lock();
            bool dirty = D.stall_dirty;
            if (dirty) {
                D.stall_dirty = false;
                plan_phase(a, t0, &p);
                PLAN = p;
            }
            vtk_unlock();
            if (!dirty || vtk_now() >= p.scan_end) break;
        }
        if (p.min_slack_lines < worst) worst = p.min_slack_lines;
        if (p.late) {
            ok = false;
            D.underrun_at = p.late_at;
            sim_metric_add("display.late_lines_phases", 1);
        }
        sim_metric_sample("display.phase_ms", (p.next_start - t0) / 1e6);
        sim_metric_sample("display.prefill_ms", (p.scan_start - t0) / 1e6);
        t0 = p.next_start;
        uint64_t now = vtk_now();
        if (now < t0) vtk_sleep_ns(t0 - now);
    }
    D.playing = false;
    sim_metric_min("display.feeder_slack_lines_min", worst);
    return ok;
}

/* Hardware EPD controller: fixed phase time, no CPU feeders. */
static bool play_fixed(int phases) {
    D.playing = true;
    vtk_sleep_ns((uint64_t)phases * SIM_US(g_profile.frame_time_us));
    D.playing = false;
    return true;
}

static bool play(int phases, const hal_rect_t *a) { return g_profile.feeders_cpu ? play_cpu(phases, a) : play_fixed(phases); }

/* Clear frames (epd_push_pixels_lcd): the interrupt copies one static line per line, no feeders, no
 * prefill, no phase padding. */
static void play_clear(int frames) {
    if (!g_profile.feeders_cpu) {
        (void)play_fixed(frames);
        return;
    }
    D.playing = true;
    uint64_t frame = (uint64_t)g_profile.line_count * g_profile.line_ns + SIM_US(g_profile.frame_overhead_us);
    vtk_sleep_ns((uint64_t)frames * frame);
    D.playing = false;
}

/* Upstream difference image (before) and back-buffer copy (after): full-width rows of the area. */
static void diff_pass(const hal_rect_t *a) {
    sim_assets_cache_pollute(); /* the waveform streams PSRAM framebuffers through the shared D-cache */
    if (!g_profile.diff_pass) return;
    double px = g_profile.diff_pass == 3 ? (double)D.W * D.H : (double)a->h * (double)(g_profile.diff_pass == 2 ? D.W : a->w);
    uint64_t t0 = vtk_now();
    /* compute + PSRAM traffic: read front and back (4 bpp each), write the 1-byte-per-pixel difference */
    sim_charge_ns_always(sim_cycles_ns(px * (profile_cost("epd.diff_px") + profile_cost("psram_to_internal.copy_byte") +
                                             profile_cost("internal_to_psram.copy_byte"))));
    vtk_sched_point();
    sim_trace_span("display", "diff", t0, vtk_now(), NULL);
}

static void back_copy(const hal_rect_t *a) {
    if (!g_profile.diff_pass) return;
    double bytes = (double)a->h * (double)(g_profile.diff_pass >= 2 ? D.W : a->w) / 2.0;
    uint64_t t0 = vtk_now();
    sim_charge_ns_always(sim_cycles_ns(bytes * profile_cost("psram.copy_byte")));
    vtk_sched_point();
    sim_trace_span("display", "backcopy", t0, vtk_now(), NULL);
}

static hal_cpu_boost_t *g_boost; /* the driver keeps the CPU at full speed while it refreshes */

static void execute(req_t *r) {
    hal_display_result_t res = {.id = r->id, .ok = true, .queued_us = r->queued_ns / 1000};
    if (!g_boost) g_boost = hal_cpu_boost_create("epd");
    hal_cpu_boost_acquire(g_boost);
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
        bool full_gc16 = wf == HAL_WF_GC16 && full_area;
        if (full_gc16 && g_profile.gc16_full_phases > phases) phases = g_profile.gc16_full_phases;
        started = vtk_now();
        sim_power_consumer("display.refresh", true, g_profile.ma_display_refresh);
        if (full_gc16 && g_profile.gc16_full_clear_frames > 0) play_clear(g_profile.gc16_full_clear_frames);
        bool underrun = !play(phases, &r->area);
        if (underrun) {
            sim_metric_add("display.underruns", 1);
            sim_warn("display: a prepared line was not ready at %.3f ms during a %s waveform (feeders slower than the "
                     "scan, too small a prefill, or a flash operation froze them) -> garbage on the panel; recovering "
                     "with clear + full GC16",
                     D.underrun_at / 1e6, WFN[wf]);
            uint64_t t_rec = vtk_now();
            hal_rect_t all = {0, 0, D.W, D.H};
            play_clear(g_profile.clear_frames);
            (void)play(g_profile.gc16_full_phases, &all);
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
    hal_cpu_boost_release(g_boost);
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

    /* The real driver's buffers come out of the device heaps, as separate blocks (a block can
     * never exceed the largest internal heap region): per feeder a prepared-line queue, the
     * waveform LUT, the LCD bounce buffers (DMA-capable) and the rest of its state. */
    const char *prev = osal_mem_tag("display-driver");
    void *ext = g_profile.panel_buffers_external ? osal_malloc(g_profile.panel_buffers_external, OSAL_MEM_EXTERNAL) : (void *)1;
    if (!ext) osal_panic("display driver init failed: cannot reserve %zu KiB of external RAM", g_profile.panel_buffers_external / 1024);
    if (D.info.tech == HAL_PANEL_EPD && g_profile.panel_buffers_internal) {
        size_t line_bytes = ((size_t)D.W / 4 + 15) / 16 * 16; /* 2 bits per pixel on the panel bus, 16-byte aligned */
        size_t queue = (size_t)g_profile.line_queue_lines * line_bytes; /* per feeder */
        size_t bounce = 2 * (size_t)g_profile.bounce_lines * line_bytes; /* refilled by the LCD interrupt */
        size_t lut = g_profile.driver_lut_bytes;
        size_t used = 2 * queue + lut + bounce;
        size_t state = g_profile.panel_buffers_internal > used ? g_profile.panel_buffers_internal - used : 0;
        struct { const char *what; size_t n; uint32_t caps; } parts[] = {
            {"waveform LUT", lut, OSAL_MEM_INTERNAL},           {"line queue (feeder 0)", queue, OSAL_MEM_INTERNAL},
            {"line queue (feeder 1)", queue, OSAL_MEM_INTERNAL}, {"LCD bounce buffers", bounce, OSAL_MEM_DMA},
            {"driver state", state, OSAL_MEM_INTERNAL},
        };
        for (size_t i = 0; i < sizeof parts / sizeof parts[0]; i++)
            if (parts[i].n && !osal_malloc(parts[i].n, parts[i].caps))
                osal_panic("display driver init failed: no memory for its %s (%zu bytes; internal heap blocks are limited "
                           "by the largest region)", parts[i].what, parts[i].n);
    } else if (g_profile.panel_buffers_internal && !osal_malloc(g_profile.panel_buffers_internal, OSAL_MEM_INTERNAL)) {
        osal_panic("display driver init failed: cannot reserve %zu KiB internal", g_profile.panel_buffers_internal / 1024);
    }
    vtk_set_stall_hook(display_stall_hook);
    D.q = osal_queue_create(sizeof(req_t), QUEUE_LEN);
    if (!D.q) osal_panic("display driver init failed: no memory for its request queue");
    osal_task_cfg_t cfg = {.name = "epd", .fn = driver_task, .stack_bytes = DRIVER_STACK, .priority = DRIVER_PRIO, .core = -1};
    if (osal_task_create(&cfg, &D.task) != OSAL_OK) osal_panic("display driver init failed: cannot create its task");
    if (D.info.tech == HAL_PANEL_EPD && g_profile.feeders_cpu) {
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

/* Timing of one phase of an update of `area` (self-test expectations; no stalls ahead). */
void sim_display_plan_info(const hal_rect_t *area, uint64_t *phase_ns, uint64_t *busy0_ns, uint64_t *busy1_ns,
                           double *slack_lines) {
    if (!g_profile.feeders_cpu || D.info.tech != HAL_PANEL_EPD) {
        *phase_ns = SIM_US(g_profile.frame_time_us);
        *busy0_ns = *busy1_ns = 0;
        *slack_lines = 1e9;
        return;
    }
    plan_t p;
    vtk_lock();
    uint64_t t0 = vtk_now();
    plan_phase(area, t0, &p);
    vtk_unlock();
    *phase_ns = p.next_start - t0;
    *busy0_ns = p.busy_end[0] - t0;
    *busy1_ns = p.busy_end[1] - t0;
    *slack_lines = p.min_slack_lines;
}

bool sim_display_playing(void) { return D.playing; }

/* A flash operation starting now (IDF 6.1 cache_utils.c): the caller suspends its own scheduler,
 * posts a non-blocking IPC call to the other core and spins until the IPC task (priority
 * configMAX_PRIORITIES - 1, esp_ipc.c) runs there. A feeder computing on that core has the same
 * priority and is not preempted: the IPC task gets the core when the feeder is done with this
 * phase's lines (or yields on a full queue) or at the next tick, whichever comes first. Modelled
 * pessimistically: both cores count as frozen for the wait (in reality the other core's feeder
 * keeps producing). Task context. */
uint64_t sim_display_ipc_wait_ns(uint64_t now) {
    if (!D.playing || !g_profile.feeders_cpu || g_profile.cores < 2) return 0;
    int c = vtk_current_core();
    int other = c == 0 ? 1 : 0;
    vtk_lock();
    plan_t p = PLAN;
    vtk_unlock();
    if (now < p.start || now >= p.busy_end[other]) return 0;
    uint64_t T = g_profile.tick_hz > 0 ? 1000000000ull / (uint64_t)g_profile.tick_hz : SIM_MS(10);
    uint64_t tick = (now / T + 1) * T, until = p.busy_end[other] < tick ? p.busy_end[other] : tick;
    return until - now;
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
