/*
 * Our own refresh on the real panel, the first time (SP-13, D-40). Included by bench.c.
 *
 * What it does, in order, all with the rails and VCOM the factory driver sets:
 *  1. The guard proves itself: rails on, nobody scanning, and it must turn them off.
 *  2. A rehearsal: full scans of empty lines, which push nothing. The first push comes only after these.
 *  3. A wedge: patches pushed 1..24 times toward black from white and toward white from black, one
 *     push per scan, next to the factory's 16 gray levels. A photo of it says how many pushes a
 *     full swing takes at this phase length, which is what the kernel's tables are built from.
 *  4. Trials: the PQ-BN kernel (drivers/display/pqbn) drives real screens: a page change, a large
 *     black area, reading pages one after the other, a small area toggling. Each reports its time,
 *     the planner's and the executors' cycles, and missed lines.
 *  5. A page of grays through the factory driver, to choose from with the eye.
 * A factory clean ends every trial, so nothing stays on the panel. The kernel is used as it is: the
 * planner runs after each scan, which makes a busy phase longer (integration report, problem 1).
 *
 * Who runs when. The scan tasks never block while they wait for room or for each other, they only
 * yield, so nothing below them runs on their core until they are done. The third run hung on exactly
 * this: the driving task woke the scan task on its own core first, was never run again, and its
 * timeouts never fired, with the rails on. So the driving task sits above the scan tasks while it
 * drives, and the other core is woken first, as the factory driver does. Above everything sits a
 * guard on the other core that needs nothing from any of them.
 *
 * A line that is not ready goes out empty, which is no push at all, and is dropped when it comes, so
 * the rows after it stay on their own rows: never stray drive.
 */
#include "esp_attr.h"
#include "esp_cpu.h"
#include "freertos/semphr.h"
#include "epd_board.h"
#include "lcd_driver.h"
#include "line_queue.h"
#include "tps65185.h"
#include "pqbn_v4.h"

#define PANEL_W 1216
#define PANEL_H 684
#define LINE_BYTES (PANEL_W / 4)
#define QUEUE_LINES 128   /* the factory driver's numbers: nothing here should fail for a reason it has already solved */
#define PREFILL_LINES 192
#define MIN_PHASE_US 12000
#define SCAN_TIMEOUT_MS 200 /* a scan takes 11 ms */
#define GUARD_MS 2000       /* above the worst case of the driving task's own timeouts: three of them and a stop */
#define FACTORY_GUARD_MS 5000 /* a factory clean and picture take about 2 s */
#define MAX_DRIVE_US (30 * 1000000) /* no scene takes this long, and it is the most the rails stay on for one */

static void hold(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

static struct {
    LineQueue_t queue[2];
    TaskHandle_t scanner[2];
    SemaphoreHandle_t scan_done[2], frame_done, lock;
    void (*fill)(int row, uint8_t *line);
    uint8_t ready[PANEL_H];
    volatile int consumed;
    volatile int owed[2]; /* lines of each queue that missed their turn */
    volatile bool stop, failed;
    volatile uint32_t underruns;
    uint32_t row_cycles_max[2];
    uint64_t row_cycles_sum[2], rows_timed[2];
    UBaseType_t priority;
    int stuck_stage;
    bool started;
} L;

static struct {
    volatile bool armed, testing, tripped;
    volatile int64_t beat, limit_us;
} G;

static bool IRAM_ATTR line_from_queue(void *unused, uint8_t *buf) {
    (void) unused;
    const int row = L.consumed;
    if (row >= PANEL_H) { /* the driver scans a few more lines than the panel has */
        memset(buf, 0, LINE_BYTES);
        return false;
    }
    LineQueue_t *q = &L.queue[row & 1];
    volatile int *owed = &L.owed[row & 1];
    while (*owed && lq_read(q, buf) == 0) --*owed;
    if (*owed || lq_read(q, buf) != 0) {
        ++*owed;
        L.underruns++;
        memset(buf, 0, LINE_BYTES);
    }
    L.consumed = row + 1;
    return false;
}

static void IRAM_ATTR frame_finished(void *unused) {
    (void) unused;
    epd_lcd_frame_done_cb(NULL, NULL);
    epd_lcd_line_source_cb(NULL, NULL);
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(L.frame_done, &woken);
    portYIELD_FROM_ISR(woken);
}

/* The scan starts from a scan task, once the first lines are all there. */
static bool scan_start(void) {
    for (int r = 0; r < PREFILL_LINES; r++)
        while (!__atomic_load_n(&L.ready[r], __ATOMIC_ACQUIRE)) {
            if (L.stop) return false;
            vTaskDelay(0);
        }
    xSemaphoreTake(L.lock, portMAX_DELAY);
    const bool go = !L.stop;
    if (go) {
        epd_lcd_line_source_cb(line_from_queue, NULL);
        epd_lcd_start_frame();
    }
    xSemaphoreGive(L.lock);
    return go;
}

static void scan_cancel(void) {
    L.stop = true;
    const bool locked = xSemaphoreTake(L.lock, pdMS_TO_TICKS(500)) == pdTRUE;
    epd_lcd_line_source_cb(NULL, NULL);
    epd_lcd_frame_done_cb(NULL, NULL);
    epd_lcd_abort_frame();
    if (locked) xSemaphoreGive(L.lock);
}

static void scanner_task(void *arg) {
    const int id = (int) (intptr_t) arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (int row = id; row < PANEL_H && !L.stop; row += 2) {
            if (row == PREFILL_LINES && !scan_start()) break;
            uint8_t *slot;
            while (!(slot = lq_current(&L.queue[id])) && !L.stop) vTaskDelay(0);
            if (!slot) break;
            const uint32_t t = esp_cpu_get_cycle_count();
            L.fill(row, slot);
            const uint32_t c = esp_cpu_get_cycle_count() - t;
            if (c > L.row_cycles_max[id]) L.row_cycles_max[id] = c;
            L.row_cycles_sum[id] += c;
            L.rows_timed[id]++;
            lq_commit(&L.queue[id]);
            __atomic_store_n(&L.ready[row], 1, __ATOMIC_RELEASE);
        }
        xSemaphoreGive(L.scan_done[id]);
    }
}

/* ------------------------------------------------------------------- the guard */

static void guard_arm(int ms) {
    G.limit_us = (int64_t) ms * 1000;
    G.beat = now_us();
    G.armed = true;
}

/* The rails on with nobody scanning: whatever else has gone wrong, they go off, and the next boot leaves the panel alone. */
static void guard_task(void *unused) {
    (void) unused;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (!G.armed || now_us() - G.beat < G.limit_us) continue;
        G.armed = false;
        if (!G.testing) scan_cancel(); /* the test runs before anything has started the LCD */
        epd_set_mode(0);
        epd_poweroff();
        G.tripped = true;
        if (G.testing) continue;
        R("\"test\":\"guard\",\"tripped\":true,\"consumed\":%d", L.consumed);
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
}

/* The power chip answers only while awake (PWR_GOOD proves nothing: it floats high while the chip sleeps, see the first bench's TCA inputs). */
static bool rails_up(void) { return tps_probe(I2C_NUM_1) == ESP_OK && (tps_read_register(I2C_NUM_1, TPS_REG_PG) & 0xFA) == 0xFA; }
static bool rails_asleep(void) { return tps_probe(I2C_NUM_1) != ESP_OK; }

static bool guard_self_test(void) {
    epd_poweron();
    const bool up = rails_up();
    G.testing = true;
    G.tripped = false;
    const int64_t t0 = now_us();
    guard_arm(GUARD_MS);
    while (!G.tripped && now_us() - t0 < (int64_t) (GUARD_MS + 1000) * 1000) hold(20);
    const bool tripped = G.tripped;
    const int64_t ms = (now_us() - t0) / 1000;
    G.armed = false;
    G.testing = false;
    if (!tripped) {
        epd_set_mode(0);
        epd_poweroff();
    }
    bool asleep = false;
    for (int i = 0; i < 100 && !asleep; i++) {
        asleep = rails_asleep();
        if (!asleep) hold(20);
    }
    R("\"test\":\"guard_self_test\",\"rails_up\":%s,\"tripped\":%s,\"after_ms\":%lld,\"rails_asleep\":%s", up ? "true" : "false",
      tripped ? "true" : "false", ms, asleep ? "true" : "false");
    return up && tripped && asleep;
}

static void scan_start_tasks(void) {
    if (L.started) return;
    L.frame_done = xSemaphoreCreateBinary();
    L.lock = xSemaphoreCreateMutex();
    for (int i = 0; i < 2; i++) {
        L.queue[i] = lq_init(QUEUE_LINES, LINE_BYTES);
        L.scan_done[i] = xSemaphoreCreateBinary();
        xTaskCreatePinnedToCore(scanner_task, "scan", 4096, (void *) (intptr_t) i, configMAX_PRIORITIES - 2, &L.scanner[i], i);
    }
    xTaskCreatePinnedToCore(guard_task, "guard", 4096, NULL, configMAX_PRIORITIES - 1, NULL, 1);
    L.started = true;
}

/* Rails on, and from here the driving task runs whenever it wants to: its timeouts have to be able to fire. */
static void drive_begin(void) {
    epd_poweron();
    epd_set_mode(1);
    guard_arm(GUARD_MS);
    L.priority = uxTaskPriorityGet(NULL);
    vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
}

static void drive_end(void) {
    G.beat = now_us();
    epd_set_mode(0);
    epd_poweroff();
    G.armed = false;
    vTaskPrioritySet(NULL, L.priority);
    if (L.failed) R("\"test\":\"scan_stuck\",\"stage\":%d,\"consumed\":%d,\"underruns\":%u", L.stuck_stage, L.consumed, (unsigned) L.underruns);
}

/* One scan of the whole panel. Returns false when it did not finish: nothing of ours drives the panel after that. */
static bool scan_once(void (*fill)(int row, uint8_t *line)) {
    const int64_t start = now_us();
    L.fill = fill;
    L.consumed = 0;
    L.owed[0] = L.owed[1] = 0;
    L.stop = false;
    memset(L.ready, 0, sizeof L.ready);
    epd_lcd_frame_done_cb(frame_finished, NULL);
    const int here = xPortGetCoreID();
    xTaskNotifyGive(L.scanner[!here]);
    xTaskNotifyGive(L.scanner[here]);
    L.stuck_stage = 0;
    if (xSemaphoreTake(L.frame_done, pdMS_TO_TICKS(SCAN_TIMEOUT_MS)) != pdTRUE) goto stuck;
    for (int i = 0; i < 2; i++) {
        L.stuck_stage = i + 1;
        if (xSemaphoreTake(L.scan_done[i], pdMS_TO_TICKS(SCAN_TIMEOUT_MS)) != pdTRUE) goto stuck;
    }
    for (int i = 0; i < 2; i++) lq_reset(&L.queue[i]); /* what came too late stays out of the next scan */
    G.beat = now_us();
    while (now_us() - start < MIN_PHASE_US) taskYIELD();
    return true;
stuck:
    L.failed = true;
    scan_cancel();
    vTaskDelay(pdMS_TO_TICKS(50));
    for (int i = 0; i < 2; i++) {
        lq_reset(&L.queue[i]);
        xSemaphoreTake(L.scan_done[i], 0);
    }
    xSemaphoreTake(L.frame_done, 0);
    return false;
}

static void reset_row_stats(void) {
    L.underruns = 0;
    for (int i = 0; i < 2; i++) L.row_cycles_max[i] = 0, L.row_cycles_sum[i] = 0, L.rows_timed[i] = 0;
}

/* ------------------------------------------------------------------- the rehearsal */

static void fill_empty(int row, uint8_t *line) {
    (void) row;
    memset(line, 0, LINE_BYTES);
}

static bool rehearse(void) {
    reset_row_stats();
    drive_begin();
    const int64_t t0 = now_us();
    int64_t worst = 0, last = t0;
    bool ok = true;
    for (int i = 0; i < 20 && ok; i++) {
        ok = scan_once(fill_empty);
        const int64_t t = now_us();
        if (t - last > worst) worst = t - last;
        last = t;
    }
    const int64_t ms = (now_us() - t0) / 1000;
    drive_end();
    R("\"test\":\"rehearsal\",\"ok\":%s,\"scans\":20,\"ms\":%lld,\"phase_us_max\":%lld,\"underruns\":%u", ok ? "true" : "false", ms, worst,
      (unsigned) L.underruns);
    return ok;
}

/* ------------------------------------------------------------------- the wedge */

static uint8_t g_wedge_line[WEDGE_COLS][LINE_BYTES];

static void fill_wedge(int row, uint8_t *line) {
    const int lx = PANEL_H - 1 - row, off = lx - WEDGE_X0, col = off / WEDGE_PITCH_X;
    if (off < 0 || col >= WEDGE_COLS || off % WEDGE_PITCH_X >= WEDGE_PATCH) memset(line, 0, LINE_BYTES);
    else memcpy(line, g_wedge_line[col], LINE_BYTES);
}

/* Which patches push at step k. The patch at (col, row) gets n = row % 4 * cols + col + 1 pushes in all. */
static void wedge_lines_for_step(int k) {
    memset(g_wedge_line, 0, sizeof g_wedge_line);
    for (int c = 0; c < WEDGE_COLS; c++)
        for (int r = 0; r < WEDGE_ROWS; r++) {
            const int n = (r % (WEDGE_ROWS / 2)) * WEDGE_COLS + c + 1;
            if (n < k) continue;
            const int code = r < WEDGE_ROWS / 2 ? 1 : 2, y0 = WEDGE_Y0 + r * WEDGE_PITCH_Y;
            for (int x = y0; x < y0 + WEDGE_PATCH; x++) g_wedge_line[c][x >> 2] |= (uint8_t) (code << (6 - 2 * (x & 3)));
        }
}

static void run_wedge(void) {
    reset_row_stats();
    drive_begin();
    const int64_t t0 = now_us();
    int64_t worst = 0, last = t0;
    bool ok = true;
    for (int k = 1; k <= WEDGE_COLS * WEDGE_ROWS / 2 && ok; k++) {
        wedge_lines_for_step(k);
        ok = scan_once(fill_wedge);
        const int64_t t = now_us();
        if (t - last > worst) worst = t - last;
        last = t;
    }
    const int64_t ms = (now_us() - t0) / 1000;
    drive_end();
    R("\"test\":\"wedge\",\"ok\":%s,\"ms\":%lld,\"phase_us_max\":%lld,\"underruns\":%u", ok ? "true" : "false", ms, worst, (unsigned) L.underruns);
}

/* ------------------------------------------------------------------- the kernel */

static struct {
    pqbn_v4_ctx C;
    uint32_t now;
    int frames_per_push;
    int64_t last_drive_us;
    bool allocated;
} K;

static void fill_kernel(int row, uint8_t *line) {
    if (!pqbn_v4_exec_row(&K.C, K.now, row, line)) memset(line, 0, LINE_BYTES);
}

static void *lab_alloc(size_t n, bool internal) {
    void *p = heap_caps_calloc(1, n, internal ? MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT : MALLOC_CAP_SPIRAM);
    if (!p) R("\"test\":\"lab_alloc\",\"bytes\":%u,\"internal\":%d,\"error\":\"no memory\"", (unsigned) n, internal);
    return p;
}

static bool kernel_alloc(void) {
    if (K.allocated) return true;
    pqbn_v4_ctx *C = &K.C;
    const size_t npx = (size_t) PANEL_W * PANEL_H, nt = (size_t) (PANEL_W / PQBN_TPX) * (PANEL_H / PQBN_TPY);
    C->W = PANEL_W;
    C->H = PANEL_H;
    C->ntx = PANEL_W / PQBN_TPX;
    C->nty = PANEL_H / PQBN_TPY;
    C->marg = 2;
    C->plan_budget = 20000; /* for a screen of new content: at 8000 it took 140 phases on the real pictures (host run) */
    C->auto_motion = 0;
    C->line_mode = 1;
    C->drive = NULL;
    C->fb4 = lab_alloc(npx / 2, false);
    C->hot = lab_alloc(npx / 2, false);
    C->cold = lab_alloc(npx * 2, false);
    C->dec = lab_alloc(16384, false);
    C->tau = lab_alloc(4096, false);
    C->tau_nib = lab_alloc(2048, false);
    C->exk = lab_alloc(6 * 256, true);
    C->exk_m = lab_alloc(7 * 256, true);
    C->tile_dirty = lab_alloc(nt, true);
    C->tile_nmax = lab_alloc(nt, true);
    C->tile_plan = lab_alloc(nt * 2, true);
    C->band = lab_alloc((size_t) C->nty * 4, true);
    C->row_active = lab_alloc(PANEL_H, true);
    if (!C->fb4 || !C->hot || !C->cold || !C->dec || !C->tau || !C->tau_nib || !C->exk || !C->exk_m || !C->tile_dirty ||
        !C->tile_nmax || !C->tile_plan || !C->band || !C->row_active)
        return false;
    uint8_t *tau = (uint8_t *) C->tau;
    for (int i = 0; i < 4096; i++) tau[i] = (uint8_t) ((((i & 63) * 37) + ((i >> 6) * 101)) & 15); /* as the research's benches */
    pqbn_v4_build_motion((uint8_t *) C->exk_m, (uint8_t *) C->tau_nib, C->tau);
    K.allocated = true;
    return true;
}

static bool kernel_work_after(uint32_t now) {
    const pqbn_v4_ctx *C = &K.C;
    if (C->backlog || C->n_motion) return true;
    const int nt = C->ntx * C->nty;
    for (int i = 0; i < nt; i++)
        if (C->tile_dirty[i] || (C->tile_nmax[i] & (T_PEND | T_CAP))) return true;
    for (int ty = 0; ty < C->nty; ty++)
        if ((int16_t) (C->band[2 * ty] - (uint16_t) (now + 1)) > 0) return true;
    return false;
}

/* The phase clock crosses a rest, at most 255 phases: the kernel's dwell (first-push stiction) counts in phases. */
static void kernel_rest(void) {
    if (!K.last_drive_us) return;
    int64_t phases = (now_us() - K.last_drive_us) / MIN_PHASE_US;
    if (phases > 255) phases = 255;
    if (phases <= 0) return;
    pqbn_v4_skip(&K.C, K.now, K.now + (uint32_t) phases);
    K.now += (uint32_t) phases;
}

/* Drive phases until nothing moves. */
static bool kernel_drive(const char *what) {
    pqbn_v4_ctx *C = &K.C;
    kernel_rest();
    reset_row_stats();
    const uint32_t planned0 = C->stat_planned;
    const int64_t t0 = now_us();
    drive_begin();
    const int64_t rails_us = now_us() - t0;
    int phases = 0, first_ms = -1;
    int64_t phase_max = 0, plan_sum = 0, plan_max = 0, phase_start = now_us();
    bool ok = true;
    for (;;) {
        const int64_t p0 = now_us();
        pqbn_v4_phase(C, K.now + 1);
        const int64_t plan = now_us() - p0;
        plan_sum += plan;
        if (plan > plan_max) plan_max = plan;
        K.now++;
        if (first_ms < 0)
            for (int ty = 0; ty < C->nty; ty++)
                if ((int16_t) (C->band[2 * ty] - (uint16_t) K.now) > 0) {
                    first_ms = (int) ((now_us() - t0) / 1000);
                    break;
                }
        for (int f = 0; f < K.frames_per_push && ok; f++) ok = scan_once(fill_kernel);
        phases++;
        const int64_t t = now_us();
        if (t - phase_start > phase_max) phase_max = t - phase_start;
        phase_start = t;
        if (!ok || t - t0 > MAX_DRIVE_US || !kernel_work_after(K.now)) break;
    }
    const int64_t total = now_us() - t0;
    const bool settled = ok && !kernel_work_after(K.now);
    drive_end();
    K.last_drive_us = now_us();
    const uint64_t rows = L.rows_timed[0] + L.rows_timed[1];
    R("\"test\":\"pqbn\",\"what\":\"%s\",\"ok\":%s,\"ms\":%lld,\"first_ms\":%d,\"phases\":%d,\"phase_ms_avg\":%.1f,\"phase_ms_max\":%.1f,"
      "\"plan_ms_avg\":%.2f,\"plan_ms_max\":%.2f,\"planned_px\":%u,\"row_cycles_avg\":%u,\"row_cycles_max\":%u,\"underruns\":%u,\"rails_ms\":%lld",
      what, settled ? "true" : "false", total / 1000, first_ms, phases, (double) total / 1000.0 / phases,
      (double) phase_max / 1000.0, (double) plan_sum / 1000.0 / phases, (double) plan_max / 1000.0,
      (unsigned) (C->stat_planned - planned0), rows ? (unsigned) ((L.row_cycles_sum[0] + L.row_cycles_sum[1]) / rows) : 0,
      (unsigned) (L.row_cycles_max[0] > L.row_cycles_max[1] ? L.row_cycles_max[0] : L.row_cycles_max[1]), (unsigned) L.underruns,
      rails_us / 1000);
    return settled;
}

/* The whole panel becomes this picture. */
static bool kernel_show(const char *picture, uint8_t *fb) {
    draw_picture(picture, fb);
    memcpy((void *) K.C.fb4, fb, (size_t) PANEL_W * PANEL_H / 2);
    memset(K.C.tile_dirty, 1, (size_t) K.C.ntx * K.C.nty);
    return kernel_drive(picture);
}

/* A small area on the logical screen turns black or white, as a button does. */
static bool kernel_toggle(int on) {
    enum { LX = 240, LY = 560, SIZE = 200 };
    uint8_t *fb4 = (uint8_t *) K.C.fb4;
    const int nx0 = LY, ny0 = PANEL_H - LX - SIZE;
    for (int y = ny0; y < ny0 + SIZE; y++)
        for (int x = nx0; x < nx0 + SIZE; x++) {
            uint8_t *b = fb4 + (size_t) y * (PANEL_W / 2) + x / 2;
            const int level = on ? 0 : 15;
            *b = (x & 1) ? (uint8_t) ((*b & 0x0f) | (level << 4)) : (uint8_t) ((*b & 0xf0) | level);
        }
    for (int ty = ny0 / PQBN_TPY; ty <= (ny0 + SIZE - 1) / PQBN_TPY; ty++)
        for (int tx = nx0 / PQBN_TPX; tx <= (nx0 + SIZE - 1) / PQBN_TPX; tx++) K.C.tile_dirty[ty * K.C.ntx + tx] = 1;
    return kernel_drive(on ? "small_black" : "small_white");
}

/* The factory driver cleans the panel and shows a picture; the kernel starts from that. */
static void factory_show(EpdiyHighlevelState *hl, uint8_t *fb, const char *picture, int temp) {
    epd_poweron();
    guard_arm(FACTORY_GUARD_MS);
    epd_fullclear(hl, temp);
    draw_picture(picture, fb);
    char what[32];
    snprintf(what, sizeof what, "factory_%s", picture);
    show(hl, what, MODE_GC16, temp);
    G.armed = false;
    epd_poweroff();
}

/* Until a finger comes, or the time is up. Says it is alive: a silent console means something is wrong. */
static void hold_for_touch(int ms) {
    uint8_t t[1];
    const int64_t end = now_us() + (int64_t) ms * 1000;
    int64_t next_word = now_us() + 30000000;
    while (now_us() < end) {
        if (!rd(0x38, 0x02, t, 1) && (t[0] & 0x0f)) {
            R("\"test\":\"touched\",\"after_ms\":%lld", (int64_t) ms - (end - now_us()) / 1000);
            hold(500);
            return;
        }
        if (now_us() > next_word) {
            printf("BENCH_WAITING\n");
            fflush(stdout);
            next_word += 30000000;
        }
        hold(50);
    }
}

static void trial(EpdiyHighlevelState *hl, uint8_t *fb, float full_swing, int frames_per_push, int temp) {
    R("\"test\":\"pqbn_trial\",\"full_swing\":%.1f,\"frames_per_push\":%d", full_swing, frames_per_push);
    pqbn_v4_ctx *C = &K.C;
    if (pqbn_v4_build((uint8_t *) C->dec, (uint8_t *) C->exk, C->step, full_swing, 0.3f, 120.0f)) {
        R("\"test\":\"pqbn_trial\",\"error\":\"the tables cannot be built for this swing\"");
        return;
    }
    factory_show(hl, fb, "Main", temp);
    memcpy((void *) C->fb4, fb, (size_t) PANEL_W * PANEL_H / 2);
    K.now = 0;
    K.frames_per_push = frames_per_push;
    K.last_drive_us = 0;
    pqbn_v4_init(C, 0);
    hold(3000);
    static const struct { const char *picture; int hold_ms; } STEPS[] = {
        {"Settings", 6000}, {"Today", 6000}, {"read1", 5000}, {"read2", 3000}, {"read3", 3000}, {"read4", 3000},
    };
    for (unsigned i = 0; i < sizeof STEPS / sizeof *STEPS; i++) {
        if (!kernel_show(STEPS[i].picture, fb)) return;
        hold(STEPS[i].hold_ms);
    }
    for (int i = 0; i < 6; i++) {
        if (!kernel_toggle(!(i & 1))) return;
        hold(800);
    }
    if (!kernel_show("white", fb)) return; /* what stays behind on white is the ghost: a photo can show it */
    hold(8000);
}

static void panel_lab(void) {
    EpdiyHighlevelState *hl = MetalioEpd_Hl();
    epd_set_rotation(EPD_ROT_INVERTED_PORTRAIT);
    uint8_t *fb = epd_hl_get_framebuffer(hl);
    const int temp = (int) epd_ambient_temperature();
    R("\"test\":\"panel_temp\",\"celsius\":%d", temp);
    scan_start_tasks();
    heap("scan_on");
    if (!guard_self_test()) return; /* then nothing protects the panel: nothing more happens to it */
    hold(500);
    if (rehearse()) {
        /* how the panel answers: stays up for a photo */
        factory_show(hl, fb, "wedge", temp);
        hold(2000);
        run_wedge();
        if (!L.failed) hold_for_touch(120000);
        if (!L.failed && kernel_alloc()) {
            heap("kernel_on");
            trial(hl, fb, 14.0f, 1, temp); /* at most what the table format can hold, one scan per push */
            if (!L.failed) trial(hl, fb, 10.0f, 2, temp); /* two scans per push: a slower panel still fits the tables */
        }
    }
    factory_show(hl, fb, "grays", temp);
    hold_for_touch(120000);
    factory_show(hl, fb, "done", temp);
    heap("lab_done");
}
