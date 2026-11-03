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
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "board/metalio_board.h"
#include "board/metalio_epd_power.h"
#include "ink/hal_display.h"
#include "ink/hal_i2c.h"
#include "ink/rotation.h"
#include "pqbn_v4.h"
#include "sim.h"
#include "stb_image_write.h"
#include "tps65185/tps65185.h"

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
    uint32_t version;      /* bumped whenever the visible image may have changed (desktop simulator) */
    uint32_t phase_seq;    /* bumped for every phase the feeders work on */
    struct plan *plan;     /* the current phase (feeders read it) */
    bool stall_dirty;      /* a flash stall was reported while playing: re-plan */
    uint64_t underrun_at;
    uint64_t wf_prep_ns;      /* the waveform being played: its own line cost (0 = display-wide) */
    uint32_t wf_phase_min_us; /* ... and its own minimum phase time (0 = display-wide) */
    const uint32_t *line_cost; /* per-line feeder cost (ns) for the phase being planned (PQ-BN), else NULL */
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

static void png_on_host(void *path) {
    int lw, lh;
    uint8_t *img = sim_host_malloc((size_t)D.W * D.H);
    if (!img) return;
    visible_image(img, &lw, &lh);
    if (!stbi_write_png(path, lw, lh, 1, img, lw)) sim_warn("cannot write %s", (const char *)path);
    sim_host_free(img);
}

/* Host work (--frames, snapshots): not the calling task's stack. */
static void write_png(const char *path) { vtk_host_call(png_on_host, (void *)path); }

void sim_display_snapshot(const char *name) {
    if (!g_sim.out_dir || !D.screen) return;
    char path[512];
    snprintf(path, sizeof path, "%s/snap-%s.png", g_sim.out_dir, name);
    write_png(path);
    sim_note("snapshot %s", path);
}

uint32_t sim_display_version(void) { return D.version; }
uint32_t sim_display_phase_seq(void) { return D.phase_seq; }

bool sim_display_busy(void) { return D.playing; }

bool sim_display_visible(uint8_t *out, size_t cap, int *lw, int *lh) {
    if (!D.screen || cap < (size_t)D.W * D.H) return false;
    visible_image(out, lw, lh);
    return true;
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

/* Self-test hook: a flash stall of this length starting just after the next phase's scan begins
 * (inside the waveform, more lines left than the queues can bridge). */
static uint64_t g_test_stall_ns;
static void test_stall_cb(void *arg) { /* ISR: kernel lock held */
    (void)arg;
    vtk_stall_begin_locked(vtk_isr_now() + g_test_stall_ns);
    g_test_stall_ns = 0;
}
void sim_display_stall_next_scan(uint64_t ns) { g_test_stall_ns = ns; }

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
    const uint64_t full = D.wf_prep_ns ? D.wf_prep_ns : g_profile.line_prep_ns, skip = g_profile.line_skip_ns;
    const uint64_t area_cost = g_profile.feeder_area_columns ? skip + (full - skip) * (uint64_t)a->w / (uint64_t)D.W : full;
    span_t st[MAX_STALLS];
    int ns = collect_stalls(t0, st);
    uint64_t tf[2] = {t0, t0}, cpu[2] = {0, 0}, scan = UINT64_MAX;
    int cnt[2] = {0, 0};
    memset(p, 0, sizeof *p);
    p->start = t0;
    for (int l = 0; l < L; l++) {
        uint64_t c = D.line_cost ? D.line_cost[l] : (l >= a->y && l < a->y + a->h && l < D.H) ? area_cost : skip;
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
    uint64_t pad = t0 + SIM_US(D.wf_phase_min_us ? D.wf_phase_min_us : g_profile.phase_min_us);
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

/* Panel power with display.pmic = "tps65185": the board driver (drivers/board) runs against the chip
 * models in dev_epd_power.c, so the sequence and its timing are the driver's own. Otherwise a fixed
 * power_up_us stands in for it. */
static epd_power_t g_epd;
static bool g_pmic_warned;

static void gate_output(void *ctx, bool on) {
    (void)ctx;
    sim_tps65185_gate(on); /* XOE is a GPIO; only the PMIC model's order check needs it */
}

static void board_init(void) {
    /* the expander is the board's (metalio_board.c, set up at boot); the PMIC answers from here on */
    sim_epd_chips_init();
    if (!metalio_board_ready()) sim_warn("display: board IO expander not ready");
    g_epd = (epd_power_t){.ioexp = metalio_ioexp(),
                          .io_lock = metalio_board_lock,
                          .io_unlock = metalio_board_unlock,
                          .pmic = hal_i2c_device(0, TPS65185_ADDR, "pmic"),
                          .vcom_mv = METALIO_EPD_VCOM_MV,
                          .gate_output = gate_output};
}

/* At waveform start: the panel must see good rails and VCOM (checked by the PMIC model). */
static void check_panel_power(void) {
    if (!g_profile.pmic_tps65185 || sim_tps65185_panel_ready()) return;
    sim_metric_add("display.unpowered_waveforms", 1);
    if (!g_pmic_warned) sim_warn("display: waveform started without panel power (rails or VCOM not good)");
    g_pmic_warned = true;
}

static void power_up(void) {
    if (D.rails_on) return;
    uint64_t t0 = vtk_now();
    if (g_profile.pmic_tps65185) {
        /* first use, in the driver task that runs the sequence anyway (keeps I2C off the boot stack) */
        if (!g_epd.ioexp) board_init();
        epd_power_result_t r = epd_power_on(&g_epd);
        sim_metric_sample("display.power_up_ms", (double)(vtk_now() - t0) / 1e6);
        if (r != EPD_POWER_OK) {
            sim_metric_add("display.power_failures", 1);
            sim_warn("display: panel power-up failed (%s) after %d retries; the device would enter safe mode",
                     epd_power_result_name(r), EPD_PWR_RETRIES);
        }
    } else {
        uint64_t pause = SIM_US(g_profile.touch_pause_us);
        uint64_t total = SIM_US(g_profile.power_up_us);
        if (pause > total) pause = total;
        if (pause) sim_i2c_hold("pmic", pause); /* PMIC register writes + PG polling hold the shared bus */
        uint64_t now = vtk_now();
        if (now < t0 + total) vtk_sleep_ns(t0 + total - now);
    }
    D.rails_on = true;
    sim_power_consumer("display.rails", true, 0);
    sim_trace_span("display", "power-up", t0, vtk_now(), NULL);
    sim_metric_add("display.power_ups", 1);
}

static void power_down(void) {
    if (!D.rails_on) return;
    uint64_t t0 = vtk_now();
    if (g_profile.pmic_tps65185)
        epd_power_off(&g_epd);
    else
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
    D.version++;
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
        if (g_test_stall_ns && k > 0) vtk_event_at(p.scan_start + SIM_US(200), test_stall_cb, NULL);
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

typedef struct {
    const req_t *r;
    hal_waveform_t wf;
    uint64_t t_pick, started, done;
} finished_t;

static void record_finished(void *p) {
    const finished_t *f = p;
    const req_t *r = f->r;
    char key[80], args[200];
    snprintf(key, sizeof key, "display.updates.%s", WFN[f->wf]);
    sim_metric_add(key, 1);
    snprintf(key, sizeof key, "display.update_ms.%s", WFN[f->wf]);
    sim_metric_sample(key, (f->done - r->queued_ns) / 1e6);
    sim_metric_sample("display.queue_wait_ms", (f->t_pick - r->queued_ns) / 1e6);
    double ghost = sim_display_ghost_level();
    sim_metric_max("display.ghost_level_max", ghost);
    snprintf(args, sizeof args, "{\"id\":%u,\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,\"ghost\":%.2f}", r->id, r->area.x,
             r->area.y, r->area.w, r->area.h, ghost);
    if (f->t_pick > r->queued_ns) sim_trace_span("display", "queued", r->queued_ns, f->t_pick, NULL);
    sim_trace_span("display", WFN[f->wf], f->started, f->done, args);
    if (g_sim.frames && g_sim.out_dir) {
        char path[512];
        snprintf(path, sizeof path, "%s/frames/%05u_%08.1fms_%s.png", g_sim.out_dir, r->id, f->done / 1e6, WFN[f->wf]);
        write_png(path);
    }
}

static void execute(req_t *r) {
    hal_display_result_t res = {.id = r->id, .ok = true, .queued_us = r->queued_ns / 1000};
    if (!g_boost) g_boost = hal_cpu_boost_create("epd");
    hal_cpu_boost_acquire(g_boost);
    hal_waveform_t wf = resolve_wf(r->wf);
    res.waveform = wf;
    D.wf_prep_ns = g_profile.wf[wf].line_prep_ns; /* per-waveform scan cost / phase time, if the profile sets them */
    D.wf_phase_min_us = g_profile.wf[wf].phase_min_us;
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
        check_panel_power();
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
    finished_t f = {r, wf, t_pick, started, done};
    vtk_host_call(record_finished, &f); /* metrics, trace, --frames: not the driver task's stack */
    D.pending--;
    hal_cpu_boost_release(g_boost);
    if (r->done) r->done(r->ctx, &res);
}

/* ------------------------------------------------------------------ PQ-BN direct drive
 *
 * display.driver = "pqbn": the research's direct-drive kernel (drivers/epd/pqbn_v4.c) replaces the
 * waveform tables. No global lock: an update converts its area into the kernel's 4 bpp target and
 * marks the 32x4 tiles; the driver runs drive phases back to back while anything moves. Per phase:
 *   - the scan: two executor tasks (the feeder tasks, blocking on a full queue) produce each drive
 *     line with pqbn_v4_exec_row(); a row costs row_idle (its tile band has no running plan) or
 *     row_band + row_tile x tiles still pushing (ISS fit, tools/iss/bench); the feeder planner above
 *     decides whether every row is ready in time (else an underrun, as with EPDiy);
 *   - the planner (its own task, just below the executors) computes the next phase's plans while
 *     this phase scans; planner cycles = plan_base + plan_px x pixels visited + ... (ISS fit). If it
 *     is not done when the scan ends, the next phase waits for it (counted: plan_late);
 *   - the panel model applies the drive codes: each push moves the pixel by 1/full_swing of the
 *     swing, less after a rest (stiction, the model the planner assumes), times a per-pixel mobility.
 * An update is done when no tile of its area has a running, pending or capped plan (and none is in
 * motion mode). HAL_WF_GC16 is a flashing clean: black, white, then the content.
 * hal_display_pause() stops at the next phase boundary (flash operations run between phases). */

#define PQ_MAX_UPD 64
#define PQ_PLAN_STACK 3072

typedef struct {
    req_t r;
    int tx0, ty0, tx1, ty1;   /* tiles covered, inclusive */
    uint64_t started_ns;      /* first push in the area */
    bool after_clean;         /* overlaps the area of a clean in progress: done only after it */
} pq_upd_t;

static struct {
    pqbn_v4_ctx C;
    int ntx, nty;
    uint8_t *line;            /* host scratch: one drive line */
    uint32_t now;             /* the kernel's phase clock */
    bool planned;             /* kernel state holds the plan for phase now + 1 */
    uint16_t *opt;            /* panel model: optical state, 8.8 fixed point (host only) */
    uint32_t *last_push;      /* phase of the pixel's last push (host only) */
    uint8_t *mob;             /* per-pixel push size, x/128 (host only) */
    float st_tab[1024];       /* stiction factor by rest length (phases) */
    pq_upd_t u[PQ_MAX_UPD];
    int nu;
    int clean_stage;          /* 0 none; 1 driving black, 2 white, 3 the content */
    int clean_idx;            /* the GC16 update being cleaned */
    hal_rect_t clean_area;
    bool pause_req, paused;
    vtk_waitq_t pause_wq;     /* hal_display_pause() waiters and the paused driver */
    osal_sem_t *plan_go, *plan_done;
    double plan_cyc;
    uint32_t *line_cost;      /* per line, ns */
    uint64_t phases, plan_late, cleans;
    uint64_t drive_ns;        /* time spent driving (phases) */
    uint64_t idle_since;      /* end of the last drive (the phase clock is advanced across the rest) */
} PQ;

static bool pq_mode(void) { return g_profile.display_driver == 1 && D.info.tech == HAL_PANEL_EPD; }

static void pq_plan_task(void *arg) { /* the planner's CPU, concurrently with the scan */
    (void)arg;
    for (;;) {
        osal_sem_take(PQ.plan_go, OSAL_WAIT_FOREVER);
        sim_charge_ns_always(sim_cycles_ns(PQ.plan_cyc)); /* ISS cycles already include its cache misses */
        osal_sem_give(PQ.plan_done);
    }
}

static void pq_init(void) {
    const int W = D.W, H = D.H;
    if (W % PQBN_TPX || H % PQBN_TPY) osal_panic("pqbn: the panel (%dx%d) must be a multiple of the %dx%d tiles", W, H, PQBN_TPX, PQBN_TPY);
    PQ.ntx = W / PQBN_TPX;
    PQ.nty = H / PQBN_TPY;
    const size_t nt = (size_t)PQ.ntx * PQ.nty, npx = (size_t)W * H;
    /* device memory, from the simulated heaps: per-pixel state and the framebuffer in PSRAM; the
     * planner's tables in PSRAM too (they stay in the D-cache: +1.5% planner time on the ISS); what
     * the executor reads for every row (its tables, tile state) in internal RAM */
    struct { void **p; size_t n; uint32_t caps; const char *what; } parts[] = {
        {(void **)&PQ.C.fb4, npx / 2, OSAL_MEM_EXTERNAL, "4 bpp target"},
        {(void **)&PQ.C.hot, npx / 2, OSAL_MEM_EXTERNAL, "plan nibbles"},
        {(void **)&PQ.C.cold, npx * 2, OSAL_MEM_EXTERNAL, "pixel state"},
        {(void **)&PQ.C.dec, 16384, OSAL_MEM_EXTERNAL, "decision table"},
        {(void **)&PQ.C.tau, 4096, OSAL_MEM_EXTERNAL, "blue-noise thresholds"},
        {(void **)&PQ.C.tau_nib, 2048, OSAL_MEM_EXTERNAL, "motion thresholds"},
        {(void **)&PQ.C.exk, 6 * 256, OSAL_MEM_INTERNAL, "executor table"},
        {(void **)&PQ.C.exk_m, 7 * 256, OSAL_MEM_INTERNAL, "motion executor table"},
        {(void **)&PQ.C.tile_dirty, nt, OSAL_MEM_INTERNAL, "tile marks"},
        {(void **)&PQ.C.tile_nmax, nt, OSAL_MEM_INTERNAL, "tile state"},
        {(void **)&PQ.C.tile_plan, nt * 2, OSAL_MEM_INTERNAL, "tile plan phases"},
        {(void **)&PQ.C.band, (size_t)PQ.nty * 4, OSAL_MEM_INTERNAL, "tile bands"},
        {(void **)&PQ.C.row_active, (size_t)H, OSAL_MEM_INTERNAL, "row flags"},
    };
    for (size_t i = 0; i < sizeof parts / sizeof parts[0]; i++) {
        *parts[i].p = osal_malloc(parts[i].n, parts[i].caps);
        if (!*parts[i].p) osal_panic("pqbn init failed: no memory for its %s (%zu bytes)", parts[i].what, parts[i].n);
        memset(*parts[i].p, 0, parts[i].n);
    }
    size_t line_bytes = ((size_t)W / 4 + 15) / 16 * 16;
    size_t queue = (size_t)g_profile.line_queue_lines * line_bytes; /* per executor */
    size_t bounce = 2 * (size_t)g_profile.bounce_lines * line_bytes;
    if (!osal_malloc(queue, OSAL_MEM_INTERNAL) || !osal_malloc(queue, OSAL_MEM_INTERNAL))
        osal_panic("pqbn init failed: no memory for the executors' line queues (2 x %zu bytes)", queue);
    if (!osal_malloc(bounce, OSAL_MEM_DMA)) osal_panic("pqbn init failed: no memory for the LCD bounce buffers");
    /* tables (build-time work, not charged) */
    uint8_t *dec = (uint8_t *)PQ.C.dec, *exk = (uint8_t *)PQ.C.exk, *exk_m = (uint8_t *)PQ.C.exk_m;
    uint8_t *tau = (uint8_t *)PQ.C.tau, *tau_nib = (uint8_t *)PQ.C.tau_nib;
    if (pqbn_v4_build(dec, exk, PQ.C.step, (float)g_profile.pqbn.full_swing, (float)g_profile.pqbn.stiction,
                      (float)g_profile.pqbn.stiction_tau))
        osal_panic("pqbn: the decision table cannot be built for full_swing %.2f", g_profile.pqbn.full_swing);
    for (int i = 0; i < 4096; i++) tau[i] = (uint8_t)((((i & 63) * 37) + ((i >> 6) * 101)) & 15); /* as the research's benches */
    pqbn_v4_build_motion(exk_m, tau_nib, tau);
    PQ.C.W = W;
    PQ.C.H = H;
    PQ.C.ntx = PQ.ntx;
    PQ.C.nty = PQ.nty;
    PQ.C.marg = 2;
    PQ.C.plan_budget = g_profile.pqbn.plan_budget;
    PQ.C.auto_motion = g_profile.pqbn.auto_motion;
    PQ.C.line_mode = 1;
    PQ.C.drive = NULL;
    /* the panel starts white and settled */
    memset((uint8_t *)PQ.C.fb4, 0xff, npx / 2);
    pqbn_v4_init(&PQ.C, 0);
    PQ.line = sim_host_malloc(line_bytes);
    PQ.opt = sim_host_malloc(npx * sizeof *PQ.opt);
    PQ.last_push = sim_host_calloc(npx, sizeof *PQ.last_push);
    PQ.mob = sim_host_malloc(npx);
    PQ.line_cost = sim_host_malloc(sizeof *PQ.line_cost * (size_t)(g_profile.line_count > H ? g_profile.line_count : H));
    if (!PQ.line || !PQ.opt || !PQ.last_push || !PQ.mob || !PQ.line_cost) abort();
    for (size_t i = 0; i < npx; i++) {
        PQ.opt[i] = 255 << 8;
        uint32_t h = (uint32_t)i * 0x9E3779B1u;
        h ^= h >> 15;
        h *= 0x2C1B3C6Du;
        h ^= h >> 12;
        /* deterministic spread ~ N(1, sd): sum of two uniforms */
        double u = ((h & 0xffff) + (h >> 16)) / 65536.0 - 1.0; /* -1..1, triangular */
        double k = 1.0 + u * g_profile.pqbn.mobility_sd * 2.449;
        PQ.mob[i] = (uint8_t)(k * 128.0 + 0.5);
    }
    const double s0 = g_profile.pqbn.stiction, tau_d = g_profile.pqbn.stiction_tau;
    for (int d = 0; d < 1024; d++) PQ.st_tab[d] = d == 0 ? 1.0f : (float)(1.0 - s0 * (1.0 - exp(-(double)d / tau_d)));
    PQ.plan_go = osal_sem_create(0, 1);
    PQ.plan_done = osal_sem_create(0, 1);
    if (!PQ.plan_go || !PQ.plan_done) osal_panic("pqbn init failed: no semaphores");
    osal_task_cfg_t pc = {.name = "epd_plan", .fn = pq_plan_task, .stack_bytes = PQ_PLAN_STACK,
                          .priority = OSAL_PRIO_MAX - 1, .core = -1};
    if (osal_task_create(&pc, NULL) != OSAL_OK) osal_panic("pqbn init failed: cannot create the planner task");
}

static void pq_tiles_of(const hal_rect_t *a, int *tx0, int *ty0, int *tx1, int *ty1) {
    *tx0 = a->x / PQBN_TPX;
    *ty0 = a->y / PQBN_TPY;
    *tx1 = (a->x + a->w - 1) / PQBN_TPX;
    *ty1 = (a->y + a->h - 1) / PQBN_TPY;
}

static void pq_mark(const hal_rect_t *a) {
    int tx0, ty0, tx1, ty1;
    pq_tiles_of(a, &tx0, &ty0, &tx1, &ty1);
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++) PQ.C.tile_dirty[ty * PQ.ntx + tx] = 1;
}

static bool pq_in_clean(int x, int y) {
    if (PQ.clean_stage != 1 && PQ.clean_stage != 2) return false;
    const hal_rect_t *c = &PQ.clean_area;
    return x >= c->x && x < c->x + c->w && y >= c->y && y < c->y + c->h;
}

/* set the kernel's target for area a: from the framebuffer (level < 0) or one level */
static void pq_target(const hal_rect_t *a, int level, bool respect_clean) {
    uint8_t *fb4 = (uint8_t *)PQ.C.fb4;
    for (int y = a->y; y < a->y + a->h; y++)
        for (int x = a->x; x < a->x + a->w; x++) {
            if (respect_clean && pq_in_clean(x, y)) continue;
            int t4 = level >= 0 ? level : (D.target[(size_t)y * D.W + x] * 15 + 127) / 255;
            uint8_t *b = fb4 + (size_t)y * (D.W / 2) + x / 2;
            *b = (x & 1) ? (uint8_t)((*b & 0x0f) | (t4 << 4)) : (uint8_t)((*b & 0xf0) | t4);
        }
    pq_mark(a);
}

static bool rect_overlap(const hal_rect_t *a, const hal_rect_t *b) {
    return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

static void pq_ingest(const req_t *r) {
    if (PQ.nu == PQ_MAX_UPD) osal_panic("pqbn: more than %d updates in flight", PQ_MAX_UPD);
    pq_upd_t *u = &PQ.u[PQ.nu++];
    memset(u, 0, sizeof *u);
    u->r = *r;
    pq_tiles_of(&r->area, &u->tx0, &u->ty0, &u->tx1, &u->ty1);
    if (r->wf == HAL_WF_GC16 && PQ.clean_stage == 0) { /* flashing clean: black first */
        PQ.clean_stage = 1;
        PQ.clean_idx = PQ.nu - 1;
        PQ.clean_area = r->area;
        PQ.cleans++;
        pq_target(&r->area, 0, false);
        return;
    }
    u->after_clean = PQ.clean_stage && rect_overlap(&r->area, &PQ.clean_area);
    pq_target(&r->area, -1, true);
}

/* no further push in tile ti from phase `from` on */
static bool pq_tile_still(int ti, uint32_t from) {
    const int tn = PQ.C.tile_nmax[ti];
    if (PQ.C.tile_dirty[ti] || (tn & (T_PEND | T_CAP | T_MOT))) return false;
    return (int)(uint16_t)(from - PQ.C.tile_plan[ti]) >= (tn & T_NMAX);
}

static bool pq_area_still(int tx0, int ty0, int tx1, int ty1, uint32_t from) {
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++)
            if (!pq_tile_still(ty * PQ.ntx + tx, from)) return false;
    return true;
}

static bool pq_work_after(uint32_t now) { /* anything for phase now + 1 */
    if (PQ.C.backlog || PQ.C.n_motion) return true;
    const int nt = PQ.ntx * PQ.nty;
    for (int i = 0; i < nt; i++)
        if (PQ.C.tile_dirty[i] || (PQ.C.tile_nmax[i] & (T_PEND | T_CAP))) return true;
    for (int ty = 0; ty < PQ.nty; ty++)
        if ((int16_t)(PQ.C.band[2 * ty] - (uint16_t)(now + 1)) > 0) return true;
    return false;
}

/* plan phase `now` (host) and return the planner's cycles on the device */
static double pq_plan(uint32_t now) {
    const int nt = PQ.ntx * PQ.nty;
    int dirty = 0, mdirty = 0;
    for (int i = 0; i < nt; i++)
        if (PQ.C.tile_dirty[i]) {
            dirty++;
            mdirty += (PQ.C.tile_nmax[i] & T_MOT) != 0;
        }
    const int motion0 = PQ.C.n_motion;
    const uint32_t planned0 = PQ.C.stat_planned;
    pqbn_v4_phase(&PQ.C, now);
    const uint32_t planned = PQ.C.stat_planned - planned0;
    const int d = PQ.C.n_motion - motion0;
    sim_metric_max("display.pqbn.motion_tiles_max", PQ.C.n_motion);
    sim_metric_add("display.pqbn.planned_px", planned);
    if (PQ.C.n_motion > motion0) {
        sim_metric_add("display.pqbn.motion_enter_tiles", PQ.C.n_motion - motion0);
        char a[64];
        snprintf(a, sizeof a, "{\"tiles\":%d,\"backlog\":%d}", PQ.C.n_motion - motion0, PQ.C.backlog);
        sim_trace_instant("display", "pqbn motion enter", vtk_now(), a);
    }
    double cyc = (!planned && !dirty && !PQ.C.n_motion && !d)
                     ? g_profile.pqbn.plan_idle
                     : g_profile.pqbn.plan_base + g_profile.pqbn.plan_px * planned + g_profile.pqbn.plan_motion_tile * mdirty +
                           g_profile.pqbn.plan_exit_tile * (d < 0 ? -d : 0);
    sim_metric_sample("display.pqbn.plan_ms", sim_cycles_ns(cyc) / 1e6);
    return cyc;
}

/* The kernel counts rests in phases. Across a rest without drive phases the clock must still move
 * (first-push stiction after a long rest); the jump is capped at 255 phases so the kernel's 10-bit
 * settle timestamps (ages kept <= 256 + one 512-phase sweep) stay unambiguous, and the tile-age clamp
 * the kernel runs on multiples of 4096 phases is applied if the jump skips one. */
static void pq_advance_idle(void) {
    uint64_t phase_ns = (uint64_t)g_profile.line_count * g_profile.line_ns + SIM_US(g_profile.frame_overhead_us);
    if (phase_ns < SIM_US(g_profile.phase_min_us)) phase_ns = SIM_US(g_profile.phase_min_us);
    uint64_t j = (vtk_now() - PQ.idle_since) / phase_ns;
    if (j > 255) j = 255;
    if (!j) return;
    sim_metric_add("display.pqbn.idle_advance_phases", (double)j); /* budgets check the clock moves across rests */
    const uint32_t from = PQ.now, to = PQ.now + (uint32_t)j;
    if ((from >> 12) != (to >> 12)) {
        const uint32_t m = to & ~4095u;
        const int nt = PQ.ntx * PQ.nty;
        for (int i = 0; i < nt; i++)
            if ((uint16_t)(m - PQ.C.tile_plan[i]) > 8192) PQ.C.tile_plan[i] = (uint16_t)(m - 8192);
        for (int ty = 0; ty < PQ.nty; ty++)
            if ((int16_t)(PQ.C.band[2 * ty] - (uint16_t)m) <= 0) PQ.C.band[2 * ty] = (uint16_t)m;
    }
    PQ.now = to;
}

/* feeder cost of every scan line for phase `now`; returns the rows with pushes, and the first one */
static int pq_row_costs(uint32_t now, int *first) {
    int rows = 0;
    *first = -1;
    const double ns_per_cyc = 1000.0 / g_sim_mhz;
    for (int ty = 0; ty < PQ.nty; ty++) {
        int n = -1;
        if ((int16_t)(PQ.C.band[2 * ty] - (uint16_t)now) > 0) {
            n = 0;
            for (int tx = 0; tx < PQ.ntx; tx++) {
                const int ti = ty * PQ.ntx + tx;
                n += (int)(uint16_t)(now - PQ.C.tile_plan[ti]) < (PQ.C.tile_nmax[ti] & T_NMAX);
            }
        }
        const double cyc = n < 0 ? g_profile.pqbn.row_idle : g_profile.pqbn.row_band + g_profile.pqbn.row_tile * n;
        for (int k = 0; k < PQBN_TPY; k++) PQ.line_cost[ty * PQBN_TPY + k] = (uint32_t)(cyc * ns_per_cyc);
        if (n > 0) {
            rows += PQBN_TPY;
            if (*first < 0) *first = ty * PQBN_TPY;
        }
    }
    for (int l = D.H; l < g_profile.line_count; l++) PQ.line_cost[l] = (uint32_t)(g_profile.pqbn.row_idle * ns_per_cyc);
    return rows;
}

/* the panel model takes this phase's drive codes */
static void pq_apply(uint32_t now) {
    const double full = 255.0 * 256.0 / g_profile.pqbn.full_swing;
    for (int y = 0; y < D.H; y++) {
        if (!pqbn_v4_exec_row(&PQ.C, now, y, PQ.line)) continue;
        for (int x = 0; x < D.W; x++) {
            const int code = (PQ.line[x >> 2] >> (6 - 2 * (x & 3))) & 3;
            if (!code) continue;
            const size_t i = (size_t)y * D.W + x;
            uint32_t rest = now - PQ.last_push[i] - 1;
            if (PQ.last_push[i] == 0 && now > 1) rest = 1023;
            const double step = full * PQ.st_tab[rest > 1023 ? 1023 : rest] * PQ.mob[i] / 128.0;
            double v = PQ.opt[i] + (code == 2 ? step : -step);
            if (v < 0) v = 0;
            if (v > 65535) v = 65535;
            PQ.opt[i] = (uint16_t)v;
            PQ.last_push[i] = now;
            D.screen[i] = (uint8_t)(PQ.opt[i] >> 8);
        }
    }
    D.version++;
}

typedef struct {
    const pq_upd_t *u;
    uint64_t done_ns;
} pq_finished_t;

static void pq_record_finished(void *p) {
    const pq_finished_t *f = p;
    const pq_upd_t *u = f->u;
    char key[80], args[160];
    snprintf(key, sizeof key, "display.updates.%s", WFN[u->r.wf]);
    sim_metric_add(key, 1);
    snprintf(key, sizeof key, "display.update_ms.%s", WFN[u->r.wf]);
    sim_metric_sample(key, (f->done_ns - u->r.queued_ns) / 1e6);
    snprintf(args, sizeof args, "{\"id\":%u,\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}", u->r.id, u->r.area.x, u->r.area.y,
             u->r.area.w, u->r.area.h);
    sim_trace_span("display", WFN[u->r.wf], u->started_ns ? u->started_ns : f->done_ns, f->done_ns, args);
    if (g_sim.frames && g_sim.out_dir) {
        char path[512];
        snprintf(path, sizeof path, "%s/frames/%05u_%08.1fms_%s.png", g_sim.out_dir, u->r.id, f->done_ns / 1e6,
                 WFN[u->r.wf]);
        write_png(path);
    }
}

static void pq_finish(pq_upd_t *u, uint64_t done_ns) {
    hal_display_result_t res = {.id = u->r.id, .ok = true, .waveform = u->r.wf, .queued_us = u->r.queued_ns / 1000};
    res.started_us = (u->started_ns ? u->started_ns : done_ns) / 1000;
    res.done_us = done_ns / 1000;
    pq_finished_t f = {u, done_ns};
    vtk_host_call(pq_record_finished, &f); /* metrics, trace, --frames: not the driver task's stack */
    D.pending--;
    if (u->r.done) u->r.done(u->r.ctx, &res);
}

/* phase `now` is planned and about to scan (from scan_start): updates whose area pushes now start */
static void pq_note_started(uint32_t now, uint64_t scan_start) {
    for (int i = 0; i < PQ.nu; i++) {
        pq_upd_t *u = &PQ.u[i];
        if (u->started_ns) continue;
        for (int ty = u->ty0; ty <= u->ty1 && !u->started_ns; ty++)
            for (int tx = u->tx0; tx <= u->tx1; tx++) {
                const int ti = ty * PQ.ntx + tx;
                if ((int)(uint16_t)(now - PQ.C.tile_plan[ti]) < (PQ.C.tile_nmax[ti] & T_NMAX)) {
                    u->started_ns = scan_start + (uint64_t)(ty * PQBN_TPY) * g_profile.line_ns;
                    break;
                }
            }
    }
}

/* after phase `now` (its scan ended at scan_end; the next phase may be planned): finished updates,
 * clean stages */
static void pq_after_phase(uint32_t now, uint64_t scan_end) {
    if (PQ.clean_stage) { /* the clean advances once its area is still */
        pq_upd_t *c = &PQ.u[PQ.clean_idx];
        if (pq_area_still(c->tx0, c->ty0, c->tx1, c->ty1, now + 1)) {
            if (PQ.clean_stage == 1) {
                PQ.clean_stage = 2;
                pq_target(&PQ.clean_area, 15, false);
            } else if (PQ.clean_stage == 2) {
                PQ.clean_stage = 3;
                pq_target(&PQ.clean_area, -1, false); /* the newest content, other updates included */
            } else {
                PQ.clean_stage = 0;
            }
        }
    }
    for (int i = 0; i < PQ.nu;) {
        pq_upd_t *u = &PQ.u[i];
        bool cleaning = PQ.clean_stage && i == PQ.clean_idx;
        bool waiting = u->after_clean && PQ.clean_stage;
        if (!cleaning && !waiting && pq_area_still(u->tx0, u->ty0, u->tx1, u->ty1, now + 1)) {
            pq_finish(u, u->started_ns ? scan_end : vtk_now());
            PQ.u[i] = PQ.u[--PQ.nu]; /* swap-remove: only the last entry moves (to i) */
            if (PQ.clean_stage && PQ.clean_idx == PQ.nu) PQ.clean_idx = i;
            continue;
        }
        i++;
    }
}

static void pq_drain_requests(void) {
    req_t r;
    while (osal_queue_recv(D.q, &r, 0) == OSAL_OK) {
        if (r.wake_ms) {
            uint64_t until = r.queued_ns + (uint64_t)r.wake_ms * 1000000ull;
            if (until > D.wake_until_ns) D.wake_until_ns = until;
            continue;
        }
        pq_ingest(&r);
    }
}

/* drive phases while anything moves */
static void pq_drive(void) {
    if (!g_boost) g_boost = hal_cpu_boost_create("epd");
    hal_cpu_boost_acquire(g_boost);
    power_up();
    check_panel_power();
    sim_i2c_transfer("io-expander", 3);
    sim_power_consumer("display.refresh", true, g_profile.ma_display_refresh);
    D.playing = true;
    if (!PQ.planned) pq_advance_idle();
    uint64_t t_start = vtk_now(), t0 = vtk_now(), last_scan = 0;
    const hal_rect_t all = {0, 0, D.W, D.H};
    double worst = 1e9;
    bool plan_running = false;
    for (;;) {
        pq_drain_requests();
        if (PQ.pause_req) { /* a flash operation: hold still between phases */
            if (plan_running) {
                osal_sem_take(PQ.plan_done, OSAL_WAIT_FOREVER);
                plan_running = false;
            }
            uint64_t tp = vtk_now();
            vtk_lock();
            PQ.paused = true;
            D.playing = false; /* no scan: flash operations do not meet a waveform */
            vtk_wake_all_locked(&PQ.pause_wq);
            while (PQ.pause_req) vtk_block_locked(&PQ.pause_wq, VTK_TIME_INF);
            PQ.paused = false;
            D.playing = true;
            vtk_unlock();
            sim_metric_add("display.pqbn.pauses_while_driving", 1);
            sim_metric_sample("display.pqbn.pause_ms", (vtk_now() - tp) / 1e6);
            sim_trace_span("display", "pqbn paused (flash)", tp, vtk_now(), NULL);
            t0 = vtk_now();
            continue;
        }
        if (!PQ.planned) { /* first phase after a rest: plan before the scan (adds latency) */
            PQ.plan_cyc = pq_plan(PQ.now + 1);
            osal_sem_give(PQ.plan_go);
            osal_sem_take(PQ.plan_done, OSAL_WAIT_FOREVER);
            PQ.planned = true;
            t0 = vtk_now();
        }
        const uint32_t now = ++PQ.now;
        int first;
        const int rows = pq_row_costs(now, &first);
        sim_metric_sample("display.pqbn.rows_pushing", rows);
        sim_assets_cache_pollute(); /* the executors stream the plan nibbles through the shared D-cache */
        plan_t p;
        vtk_lock();
        D.line_cost = PQ.line_cost;
        D.stall_dirty = false;
        plan_phase(&all, t0, &p);
        PLAN = p;
        D.phase_seq++;
        vtk_wake_all_locked(&D.feed_wq);
        vtk_unlock();
        if (last_scan) sim_metric_sample("display.pqbn.cycle_ms", (p.scan_start - last_scan) / 1e6);
        last_scan = p.scan_start;
        pq_note_started(now, p.scan_start);
        pq_apply(now);
        PQ.planned = false;
        plan_running = false;
        /* plan_during_scan: the planner prepares the next phase while this one scans (the port must
         * then plan only bands the scan has passed, or double-buffer the tile state: the kernel's
         * re-plans rewrite tiles still pushing in this phase). Otherwise the next phase is planned after
         * this scan (the kernel as written): its time adds to the phase cycle. */
        if (g_profile.pqbn.plan_during_scan && pq_work_after(now)) {
            PQ.plan_cyc = pq_plan(now + 1);
            osal_sem_give(PQ.plan_go);
            plan_running = true;
            PQ.planned = true;
        }
        for (;;) { /* until the scan ends; a flash stall reported meanwhile re-plans it */
            uint64_t tn = vtk_now();
            if (tn < p.scan_end) vtk_sleep_ns(p.scan_end - tn);
            vtk_lock();
            bool dirty = D.stall_dirty;
            if (dirty) {
                D.stall_dirty = false;
                plan_phase(&all, t0, &p);
                PLAN = p;
            }
            vtk_unlock();
            if (!dirty || vtk_now() >= p.scan_end) break;
        }
        D.line_cost = NULL;
        PQ.phases++;
        if (p.min_slack_lines < worst) worst = p.min_slack_lines;
        if (p.late) {
            sim_metric_add("display.underruns", 1);
            sim_metric_add("display.late_lines_phases", 1);
            sim_warn("display: pqbn drive line %d was not ready at %.3f ms (executors slower than the scan, or a flash "
                     "operation froze them) -> stray pushes; recovering with a full clean", p.late_line, p.late_at / 1e6);
            if (PQ.clean_stage == 0) {
                req_t rc = {.id = ++D.next_id, .area = all, .wf = HAL_WF_GC16, .queued_ns = vtk_now()};
                D.pending++;
                pq_ingest(&rc);
            }
        }
        sim_metric_sample("display.phase_ms", (p.next_start - t0) / 1e6);
        sim_metric_sample("display.prefill_ms", (p.scan_start - t0) / 1e6);
        pq_after_phase(now, p.scan_end);
        uint64_t tn = vtk_now();
        if (tn < p.next_start) vtk_sleep_ns(p.next_start - tn);
        if (plan_running) {
            if (osal_sem_take(PQ.plan_done, 0) != OSAL_OK) { /* the next phase waits for its plan */
                PQ.plan_late++;
                uint64_t tw = vtk_now();
                osal_sem_take(PQ.plan_done, OSAL_WAIT_FOREVER);
                sim_metric_sample("display.pqbn.plan_wait_ms", (vtk_now() - tw) / 1e6);
            }
            plan_running = false;
        }
        t0 = vtk_now();
        pq_drain_requests();
        if (!PQ.planned && !pq_work_after(now)) {
            /* nothing moves: anything still listed is finished (e.g. content that did not change) */
            pq_after_phase(now, p.scan_end);
            if (!PQ.nu) break;
        }
    }
    vtk_lock();
    D.playing = false;
    vtk_wake_all_locked(&PQ.pause_wq); /* a pause asked for during the last phase: the panel is still now */
    vtk_unlock();
    PQ.idle_since = vtk_now();
    sim_power_consumer("display.refresh", false, g_profile.ma_display_refresh);
    sim_i2c_transfer("io-expander", 3);
    sim_metric_min("display.feeder_slack_lines_min", worst);
    PQ.drive_ns += vtk_now() - t_start;
    D.busy_ns += vtk_now() - t_start;
    sim_trace_span("display", "pqbn drive", t_start, vtk_now(), NULL);
    hal_cpu_boost_release(g_boost);
}

static void pq_driver_loop(void) {
    req_t r;
    for (;;) {
        osal_err_t e;
        if (D.rails_on) {
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
            if (e != OSAL_OK) continue;
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
        while (PQ.pause_req) { /* paused while idle: updates wait (no flash access needed for them) */
            vtk_lock();
            PQ.paused = true;
            vtk_wake_all_locked(&PQ.pause_wq);
            while (PQ.pause_req) vtk_block_locked(&PQ.pause_wq, VTK_TIME_INF);
            PQ.paused = false;
            vtk_unlock();
        }
        pq_ingest(&r);
        pq_drive();
        D.last_done_ns = vtk_now();
        D.wake_until_ns = 0;
    }
}

void hal_display_pause(bool pause) {
    if (!pq_mode()) return;
    if (vtk_in_isr() || !vtk_self()) osal_panic("hal_display_pause must be called from a task");
    vtk_lock();
    PQ.pause_req = pause;
    if (!pause) {
        vtk_wake_all_locked(&PQ.pause_wq);
    } else {
        while (D.playing && !PQ.paused) vtk_block_locked(&PQ.pause_wq, VTK_TIME_INF);
    }
    vtk_unlock();
}

static void driver_task(void *arg) {
    (void)arg;
    if (pq_mode()) pq_driver_loop(); /* never returns */
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
    D.target = sim_host_malloc(n);
    D.screen = sim_host_malloc(n);
    D.applying = sim_host_malloc(n);
    D.residual = sim_host_calloc(n, sizeof(float));
    if (!D.target || !D.screen || !D.applying || !D.residual) abort();
    memset(D.target, 255, n);
    memset(D.screen, 255, n);
    D.version++;
    D.temperature = 25;

    /* The real driver's buffers come out of the device heaps, as separate blocks (a block can
     * never exceed the largest internal heap region): per feeder a prepared-line queue, the
     * waveform LUT, the LCD bounce buffers (DMA-capable) and the rest of its state. */
    const char *prev = osal_mem_tag("display-driver");
    if (pq_mode()) { /* direct drive: its own buffers instead of EPDiy's */
        pq_init();
        goto tasks;
    }
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
tasks:
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
    if (g_profile.pmic_tps65185) {
        sim_metric_set("display.pmic.attempts", g_epd.attempts);
        sim_metric_set("display.pmic.retries", g_epd.retries);
        sim_metric_set("display.pmic.ioexp_errors", g_epd.ioexp_errors);
        sim_tps65185_report();
    }
    if (pq_mode() && PQ.opt) {
        sim_metric_set("display.pqbn.phases", (double)PQ.phases);
        sim_metric_set("display.pqbn.plan_late_phases", (double)PQ.plan_late);
        sim_metric_set("display.pqbn.cleans", (double)PQ.cleans);
        sim_metric_set("display.pqbn.drive_ms", PQ.drive_ns / 1e6);
        /* how close the panel model ended to the framebuffer (gray levels of 255) */
        double sum = 0, mx = 0;
        const size_t n = (size_t)D.W * D.H;
        for (size_t i = 0; i < n; i++) {
            double e = fabs((double)PQ.opt[i] / 256.0 - (double)D.target[i]);
            sum += e;
            if (e > mx) mx = e;
        }
        sim_metric_set("display.pqbn.final_error_mean", sum / (double)n);
        sim_metric_set("display.pqbn.final_error_max", mx);
    }
    sim_metric_set("display.ghost_level_final", sim_display_ghost_level());
    if (g_sim.out_dir && D.screen) {
        char path[512];
        snprintf(path, sizeof path, "%s/final.png", g_sim.out_dir);
        write_png(path);
    }
}
