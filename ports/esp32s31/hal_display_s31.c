/* Factory LCD transport with ink's PQ-BN kernel and shared board power. Compile-only port. */
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "esp_attr.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ink/hal_device.h"
#include "ink/hal_display.h"
#include "ink/osal.h"
#include "lcd_driver.h"
#include "line_queue.h"
#include "metalio_plus/metalio_board.h"
#include "metalio_plus/metalio_epd_power.h"
#include "panel_e0470a14.h"
#include "s31_board.h"
#include "tps65185/tps65185.h"

enum { W = 1216, H = 684 };
#define LINE_BYTES (W / 4)
#define TILES ((W / PQBN_TPX) * (H / PQBN_TPY))
#define SCAN_MS 200
#define GUARD_MS 2000
#define PHASE_US 12000
#define PREFILL 192
#define ATOMIC(v) atomic_load_explicit(&(v), memory_order_acquire)

static struct {
    pqbn_v4_ctx c;
    epd_power_t power;
    uint8_t *target;
    LineQueue_t q[2];
    TaskHandle_t scanners[2], task;
    SemaphoreHandle_t scan_done[2], frame_done, lock;
    atomic_bool stop, armed, powered, testing, tripped;
    atomic_uint beat, pending;
    atomic_int row, missed;
    int owed[2];
    bool empty, first, ready, tested;
    uint32_t phase, next_id, hold_ms, wake_until;
    uint64_t last_drive;
    hal_rect_t area;
    hal_display_result_t result;
    hal_display_done_fn done;
    void *ctx;
} D;

static void gate(void *ctx, bool on) {
    (void)ctx;
    gpio_set_level(EPD_PIN_XOE, on);
    gpio_set_level(EPD_PIN_MODE, on);
    if (!on) esp_rom_delay_us(12);
}
static void power_off(void) {
    epd_power_off(&D.power);
    atomic_store(&D.powered, false);
}
static void trip(void) {
    atomic_store(&D.stop, true);
    epd_lcd_line_source_cb(NULL, NULL);
    epd_lcd_frame_done_cb(NULL, NULL);
    epd_lcd_abort_frame();
    power_off();
    atomic_store(&D.armed, false);
}
static void guard_check(void) {
    if (!ATOMIC(D.armed) || (uint32_t)(xTaskGetTickCount() - ATOMIC(D.beat)) < pdMS_TO_TICKS(GUARD_MS)) return;
    if (!ATOMIC(D.testing)) { trip(); return; }
    atomic_store(&D.armed, false); /* The self-test: nothing has started the LCD, so only the power goes. */
    power_off();
    atomic_store(&D.tripped, true);
}
static void guard(void *ctx) {
    (void)ctx;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(50));
        guard_check();
    }
}
/* Rule 5 of docs/hardware/README.md: the protection proves itself before the panel is driven, as the bench
 * firmware's did (tools/hw-bench/main/panel_lab.c, guard_self_test): rails on, nobody scanning, and the guard
 * alone must take them off. A PMIC in SLEEP stops answering; PWR_GOOD floats high then and proves nothing. */
static bool guard_self_test(void) {
    bool up = epd_power_on(&D.power) == EPD_POWER_OK && epd_power_good(&D.power);
    atomic_store(&D.powered, true);
    atomic_store(&D.tripped, false);
    atomic_store(&D.testing, true);
    uint64_t t0 = osal_now_us();
    atomic_store(&D.beat, xTaskGetTickCount());
    atomic_store(&D.armed, true);
    while (!ATOMIC(D.tripped) && osal_now_us() - t0 < (uint64_t)(GUARD_MS + 1000) * 1000) vTaskDelay(pdMS_TO_TICKS(20));
    bool tripped = ATOMIC(D.tripped);
    unsigned ms = (unsigned)((osal_now_us() - t0) / 1000);
    atomic_store(&D.armed, false);
    atomic_store(&D.testing, false);
    if (!tripped) power_off();
    bool asleep = false;
    for (int i = 0; i < 100 && !asleep; i++) {
        uint8_t pg;
        asleep = tps65185_read(D.power.pmic, TPS_REG_PG, &pg) != HAL_I2C_OK;
        if (!asleep) vTaskDelay(pdMS_TO_TICKS(20));
    }
    char line[112];
    snprintf(line, sizeof line, "display: guard self-test rails_up=%d tripped=%d after_ms=%u asleep=%d", up, tripped, ms, asleep);
    hal_log_write(line);
    return up && tripped && asleep;
}
static bool IRAM_ATTR line_source(void *ctx, uint8_t *line) {
    (void)ctx;
    int row = atomic_fetch_add(&D.row, 1);
    if (row >= H) { memset(line, 0, LINE_BYTES); return false; }
    int id = row & 1;
    while (D.owed[id] && lq_read(&D.q[id], line) == 0) --D.owed[id];
    if (D.owed[id] || lq_read(&D.q[id], line) != 0) {
        ++D.owed[id];
        atomic_fetch_add(&D.missed, 1);
        memset(line, 0, LINE_BYTES);
    }
    return false;
}
static void IRAM_ATTR frame_done(void *ctx) {
    (void)ctx;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(D.frame_done, &woken);
    portYIELD_FROM_ISR(woken);
}
static void scanner(void *ctx) {
    int id = (int)(intptr_t)ctx;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (int y = id; y < H && !ATOMIC(D.stop); y += 2) {
            uint8_t *line;
            while (!(line = lq_current(&D.q[id])) && !ATOMIC(D.stop)) taskYIELD();
            if (!line || ATOMIC(D.stop)) break;
            if (D.empty || !pqbn_v4_exec_row(&D.c, D.phase, y, line)) memset(line, 0, LINE_BYTES);
            lq_commit(&D.q[id]);
            if (y == PREFILL) {
                while (atomic_load(&D.q[1].current) < PREFILL / 2 && !ATOMIC(D.stop)) taskYIELD();
                if (!ATOMIC(D.stop)) epd_lcd_start_frame();
            }
        }
        xSemaphoreGive(D.scan_done[id]);
    }
}
static bool scan(bool empty) {
    uint64_t start = osal_now_us();
    D.empty = empty;
    D.owed[0] = D.owed[1] = 0;
    atomic_store(&D.row, 0);
    atomic_store(&D.missed, 0);
    epd_lcd_line_source_cb(line_source, NULL);
    epd_lcd_frame_done_cb(frame_done, NULL);
    int here = xPortGetCoreID();
    xTaskNotifyGive(D.scanners[!here]);
    xTaskNotifyGive(D.scanners[here]);
    bool ok = xSemaphoreTake(D.frame_done, pdMS_TO_TICKS(SCAN_MS)) == pdTRUE;
    for (int i = 0; i < 2; i++)
        if (xSemaphoreTake(D.scan_done[i], pdMS_TO_TICKS(SCAN_MS)) != pdTRUE) ok = false;
    epd_lcd_line_source_cb(NULL, NULL);
    epd_lcd_frame_done_cb(NULL, NULL);
    if (!ok || ATOMIC(D.missed) || ATOMIC(D.stop)) { trip(); return false; }
    for (int i = 0; i < 2; i++) lq_reset(&D.q[i]);
    atomic_store(&D.beat, xTaskGetTickCount());
    while (osal_now_us() - start < PHASE_US) taskYIELD();
    return true;
}
static void target(const hal_rect_t *a, int level) {
    uint8_t *fb = (uint8_t *)D.c.fb4;
    for (int y = a->y; y < a->y + a->h; y++)
        for (int x = a->x; x < a->x + a->w; x++) {
            size_t i = (size_t)y * (W / 2) + x / 2;
            int shift = (x & 1) * 4, v = level < 0 ? (D.target[i] >> shift) & 15 : level;
            fb[i] = (uint8_t)((fb[i] & ~(15 << shift)) | (v << shift));
            D.c.tile_dirty[(y / PQBN_TPY) * D.c.ntx + x / PQBN_TPX] = 1;
        }
}
static bool drive(void) {
    uint64_t start = osal_now_us();
    bool holding = false;
    do {
        if (ATOMIC(D.stop) || osal_now_us() - start > 30000000) { trip(); return false; }
        if (holding && pqbn_v4_plan_due(&D.c, D.phase + 1) && !scan(true)) return false;
        pqbn_v4_phase(&D.c, ++D.phase);
        if (!scan(false)) return false;
        holding = true;
    } while (pqbn_v4_busy_after(&D.c, D.phase));
    return scan(true); /* End the final push before a callback or pause. */
}
static void display_task(void *ctx) {
    (void)ctx;
    if (!D.tested) {
        D.tested = true;
        if (!guard_self_test()) {
            atomic_store(&D.stop, true);
            hal_log_write("display: the guard did not prove itself, the panel stays off");
        }
    }
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
        xSemaphoreTake(D.lock, portMAX_DELAY);
        hal_display_result_t r = D.result;
        hal_display_done_fn done = D.done;
        void *arg = D.ctx;
        bool wake = D.wake_until && (int32_t)(D.wake_until - xTaskGetTickCount()) > 0;
        xSemaphoreGive(D.lock);
        if ((r.id || wake) && !ATOMIC(D.stop) && !ATOMIC(D.powered)) {
            atomic_store(&D.powered, epd_power_on(&D.power) == EPD_POWER_OK);
            if (!ATOMIC(D.powered)) trip();
            atomic_store(&D.beat, xTaskGetTickCount());
            atomic_store(&D.armed, ATOMIC(D.powered));
        }
        if (r.id && !ATOMIC(D.stop)) {
            uint32_t rest = D.last_drive ? (uint32_t)((osal_now_us() - D.last_drive) / PHASE_US) : 0;
            pqbn_v4_skip(&D.c, D.phase, D.phase + rest);
            D.phase += rest;
            atomic_store(&D.beat, xTaskGetTickCount());
            atomic_store(&D.armed, true);
            vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
            r.started_us = osal_now_us();
            hal_rect_t a = D.first ? (hal_rect_t){0, 0, W, H} : D.area;
            bool ok = true;
            if (D.first || r.waveform == HAL_WF_GC16) {
                target(&a, 0);
                ok = drive();
                if (ok) { target(&a, 15); ok = drive(); }
            }
            if (ok) { target(&a, -1); ok = drive(); }
            r.ok = ok && epd_power_kept(&D.power);
            if (!r.ok) trip();
            D.first = false;
            D.last_drive = osal_now_us();
            vTaskPrioritySet(NULL, 5);
        }
        xSemaphoreTake(D.lock, portMAX_DELAY);
        uint32_t hold = D.hold_ms;
        if (r.id) D.wake_until = 0;
        wake = D.wake_until && (int32_t)(D.wake_until - xTaskGetTickCount()) > 0;
        xSemaphoreGive(D.lock);
        if (ATOMIC(D.powered) && !wake && (!hold || osal_now_us() - D.last_drive >= (uint64_t)hold * 1000)) power_off();
        if (!ATOMIC(D.powered)) atomic_store(&D.armed, false);
        else atomic_store(&D.beat, xTaskGetTickCount());
        if (r.id) r.done_us = osal_now_us();
        xSemaphoreTake(D.lock, portMAX_DELAY);
        if (r.id) D.result.id = 0;
        atomic_store(&D.pending, D.result.id || ATOMIC(D.powered) || (D.wake_until && (int32_t)(D.wake_until - xTaskGetTickCount()) > 0));
        xSemaphoreGive(D.lock);
        if (r.id && done) done(arg, &r);
    }
}
static void *alloc(size_t n, bool internal) {
    void *p = osal_calloc(1, n, internal ? OSAL_MEM_INTERNAL : OSAL_MEM_EXTERNAL);
    if (!p) osal_panic("display: allocation failed");
    return p;
}
static void init(void) {
    if (D.ready || !metalio_board_ready()) return;
    D.lock = xSemaphoreCreateMutex();
    D.frame_done = xSemaphoreCreateBinary();
    D.c = (pqbn_v4_ctx){.W = W, .H = H, .ntx = W / PQBN_TPX, .nty = H / PQBN_TPY};
    D.target = alloc(W * H / 2, false);
    D.c.fb4 = alloc(W * H / 2, false);
    D.c.hot = alloc(W * H / 2, false);
    D.c.cold = alloc(W * H * 2, false);
    D.c.tile_dirty = alloc(TILES, true);
    D.c.tile_nmax = alloc(TILES, true);
    D.c.tile_plan = alloc(TILES * 2, true);
    D.c.band = alloc(D.c.nty * 4, true);
    if (pqbn_v4_build(&D.c, alloc(16384, false), alloc(6 * 256, true), alloc(4096, false), &PQBN_PANEL_E0470A14)) osal_panic("display: tables failed");
    memset(D.target, 255, W * H / 2);
    memset((void *)D.c.fb4, 255, W * H / 2);
    pqbn_v4_init(&D.c, 0);
    D.power = (epd_power_t){.ioexp = metalio_ioexp(), .io_lock = metalio_board_lock, .io_unlock = metalio_board_unlock,
        .pmic = hal_i2c_device(0, 0x68, "panel-power"), .vcom_mv = EPD_VCOM_MV, .gate_output = gate, .screen_coming = metalio_board_rails_cut};
    const int pins[] = {EPD_PIN_XOE, EPD_PIN_MODE, EPD_PIN_BORDER};
    for (unsigned i = 0; i < sizeof pins / sizeof pins[0]; i++) {
        gpio_reset_pin(pins[i]);
        gpio_set_direction(pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(pins[i], 0);
    }
    LcdEpdConfig_t cfg = {.pixel_clock = 20000000, .ckv_high_time = 60, .line_front_porch = 4, .le_high_time = 9,
        .bus_width = 8, .dummy_clocks_after_data = 10, .white_border_pixels = DISPLAY_WHITE_BORDER_PIXELS,
        .bus = {.clock = EPD_PIN_XCL, .ckv = EPD_PIN_CKV, .leh = EPD_PIN_XLE, .start_pulse = EPD_PIN_XSTL,
            .stv = EPD_PIN_SPV, .data = {EPD_PIN_D0, EPD_PIN_D1, EPD_PIN_D2, EPD_PIN_D3, EPD_PIN_D4, EPD_PIN_D5, EPD_PIN_D6, EPD_PIN_D7}}};
    epd_lcd_init(&cfg, W, H);
    configASSERT(D.lock && D.frame_done && D.power.pmic);
    for (int i = 0; i < 2; i++) {
        D.q[i] = lq_init(128, LINE_BYTES);
        D.scan_done[i] = xSemaphoreCreateBinary();
        configASSERT(D.scan_done[i]);
        configASSERT(xTaskCreatePinnedToCore(scanner, "epd_scan", 4096, (void *)(intptr_t)i, configMAX_PRIORITIES - 2, &D.scanners[i], i) == pdPASS);
    }
    configASSERT(xTaskCreatePinnedToCore(guard, "epd_guard", 4096, NULL, configMAX_PRIORITIES - 1, NULL, 1) == pdPASS);
    D.first = D.ready = true;
    configASSERT(xTaskCreatePinnedToCore(display_task, "epd", 4096, NULL, 5, &D.task, 0) == pdPASS);
}
void hal_display_get_info(hal_panel_info_t *out) {
    init();
    *out = (hal_panel_info_t){.width = D.ready ? W : 0, .height = D.ready ? H : 0, .dpi = 300, .gray_levels = 16,
        .tech = HAL_PANEL_EPD, .waveforms = (1u << HAL_WF_GC16) | (1u << HAL_WF_GL16), .rotation_deg = S31_PANEL_ROTATION, .full_frame_scan = true};
}
float hal_display_level_luminance(unsigned level) { return PANEL_LEVEL[level < 16 ? level : 15]; }
uint32_t hal_display_update(const hal_rect_t *a, const uint8_t *pixels, int stride, hal_waveform_t wf,
                            uint32_t flags, hal_display_done_fn done, void *ctx) {
    (void)flags;
    if (!D.ready || !a || !pixels || a->x < 0 || a->y < 0 || a->w <= 0 || a->h <= 0 ||
        a->w > W - a->x || a->h > H - a->y || stride < a->w || (unsigned)wf >= HAL_WF_COUNT || ATOMIC(D.stop)) return 0;
    xSemaphoreTake(D.lock, portMAX_DELAY);
    if (D.result.id) { xSemaphoreGive(D.lock); return 0; }
    for (int y = 0; y < a->h; y++)
        for (int x = 0; x < a->w; x++) {
            size_t i = (size_t)(a->y + y) * (W / 2) + (a->x + x) / 2;
            int shift = ((a->x + x) & 1) * 4, v = (pixels[(size_t)y * stride + x] * 15 + 127) / 255;
            D.target[i] = (uint8_t)((D.target[i] & ~(15 << shift)) | (v << shift));
        }
    if (!++D.next_id) ++D.next_id;
    D.area = *a;
    D.done = done;
    D.ctx = ctx;
    D.result = (hal_display_result_t){.id = D.next_id, .queued_us = osal_now_us(), .waveform = wf == HAL_WF_GC16 ? wf : HAL_WF_GL16};
    uint32_t id = D.next_id;
    atomic_store(&D.pending, 1);
    xSemaphoreGive(D.lock);
    xTaskNotifyGive(D.task);
    return id;
}
int hal_display_pending(void) { return (int)ATOMIC(D.pending); }
void hal_display_hold_power(uint32_t ms) {
    if (!D.ready) return;
    xSemaphoreTake(D.lock, portMAX_DELAY);
    D.hold_ms = ms;
    xSemaphoreGive(D.lock);
}
void hal_display_wake(uint32_t ms) {
    if (!D.ready || !ms || ATOMIC(D.stop)) return;
    xSemaphoreTake(D.lock, portMAX_DELAY);
    D.wake_until = xTaskGetTickCount() + pdMS_TO_TICKS(ms);
    atomic_store(&D.pending, 1);
    xSemaphoreGive(D.lock);
    xTaskNotifyGive(D.task);
}
void hal_display_pause(bool pause) { (void)pause; /* Serial port: async_updates is false. */ }
void hal_display_set_temperature(int celsius) { (void)celsius; /* PQ-BN uses the measured 25 C calibration. */ }
