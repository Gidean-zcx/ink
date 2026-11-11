/* Execute the actual port against deterministic IDF/LCD recorders; no device. */
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef INK_DISPLAY_SOURCE
#define INK_DISPLAY_SOURCE "../hal_display_s31.c"
#endif
#include INK_DISPLAY_SOURCE

struct test_sem { bool mutex; pthread_mutex_t lock; };
struct test_task { void (*fn)(void *); void *arg; };
#define ALLOCATION_LIMIT 40
static void *allocations[ALLOCATION_LIMIT];
static unsigned allocation_count;
static uint64_t now;
static unsigned calls, scans, off_count, abort_count, notify_count, isr_gives;
static int xoe;
static bool drop_frame, supply_good = true;
static bool wake_during_off, selftest_mode, guard_alive, pmic_awake;
static char last_log[160];
static int expected_pending;
static pthread_t caller, callback_thread;
static hal_display_result_t completed;
static _Thread_local int loops;
static void (*lcd_done)(void *);
static void *lcd_ctx;
static uint8_t queue_line[LINE_BYTES];
static bool lcd_stopped;
static pthread_mutex_t access_watch = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t access_wait = PTHREAD_COND_INITIALIZER;
static bool watching_access, access_waiting;
void lcd_ll_stop(void *hw) { assert(hw == NULL && ATOMIC(D.stop) && !lcd_done); lcd_stopped = true; }
void ink_s31_panel_panic(int oe, int mode, uint8_t p0) {
    assert(oe == EPD_PIN_XOE && mode == EPD_PIN_MODE && p0 == 0);
    assert(ATOMIC(D.stop) && lcd_stopped && !lcd_done);
    xoe = 0;
    off_count++;
}

static void *record_alloc(size_t n) {
    assert(allocation_count < ALLOCATION_LIMIT);
    void *p = calloc(1, n);
    assert(p);
    allocations[allocation_count++] = p;
    return p;
}
void *osal_calloc(size_t n, size_t size, uint32_t caps) { (void)caps; return record_alloc(n * size); }
void osal_panic(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    abort();
}
uint64_t osal_now_us(void) { return now; }
bool metalio_board_ready(void) { return true; }
hal_i2c_status_t metalio_board_read(int port, uint8_t *levels) { assert(port == 0); *levels = 0xA0; return HAL_I2C_OK; }
const char *epd_power_result_name(epd_power_result_t r) { return r == EPD_POWER_OK ? "ok" : "failed"; }
tca9555_t *metalio_ioexp(void) { return NULL; }
void metalio_board_lock(void) { }
void metalio_board_unlock(void) { }
bool metalio_board_rails_cut(void) { return false; }
hal_i2c_dev_t *hal_i2c_device(int bus, uint8_t addr, const char *name) {
    (void)name;
    assert(bus == 0 && addr == 0x68);
    return (hal_i2c_dev_t *)&D;
}
epd_power_result_t epd_power_on(epd_power_t *p) { p->gate_output(p->ctx, true); now += 140000; pmic_awake = true; return EPD_POWER_OK; }
bool epd_power_good(epd_power_t *p) { (void)p; return supply_good; }
hal_i2c_status_t tps65185_read(hal_i2c_dev_t *dev, uint8_t reg, uint8_t *value) {
    (void)dev; assert(reg == TPS_REG_PG); *value = 0xFA;
    return pmic_awake ? HAL_I2C_OK : HAL_I2C_ERR; /* in SLEEP the PMIC does not answer */
}
static bool logged_kept_failing;
void hal_log_write(const char *line) {
    snprintf(last_log, sizeof last_log, "%s", line);
    if (strstr(line, "keep failing")) logged_kept_failing = true;
}
void epd_power_off(epd_power_t *p) {
    p->gate_output(p->ctx, false);
    pmic_awake = false;
    off_count++;
    if (wake_during_off) {
        wake_during_off = false;
        hal_display_wake(500);
    }
}
bool epd_power_kept(epd_power_t *p) { (void)p; return supply_good; }
struct hal_cpu_boost { bool held; };
static struct hal_cpu_boost boost;
hal_cpu_boost_t *hal_cpu_boost_create(const char *name) { assert(!strcmp(name, "display")); return &boost; }
void hal_cpu_boost_acquire(hal_cpu_boost_t *b) { assert(b == &boost && !b->held); b->held = true; }
void hal_cpu_boost_release(hal_cpu_boost_t *b) { assert(b == &boost && b->held); b->held = false; }
void gpio_reset_pin(int pin) { (void)pin; }
void gpio_set_direction(int pin, int mode) { (void)pin; assert(mode == GPIO_MODE_OUTPUT); }
void gpio_set_level(int pin, int level) { if (pin == EPD_PIN_XOE) xoe = level; }
void esp_rom_delay_us(unsigned us) { now += us; }
SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    struct test_sem *s = record_alloc(sizeof *s);
    s->mutex = true;
    assert(pthread_mutex_init(&s->lock, NULL) == 0);
    return s;
}
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return record_alloc(sizeof(struct test_sem)); }
static unsigned nibble(const uint8_t *fb, int x, int y);
static bool watching; /* the levels one pixel goes through, frame by frame */
static unsigned watched_levels;
int xSemaphoreTake(SemaphoreHandle_t s, uint32_t timeout) {
    if (s->mutex) {
        if (s == D.access && watching_access) {
            assert(pthread_mutex_lock(&access_watch) == 0);
            access_waiting = true;
            assert(pthread_cond_signal(&access_wait) == 0);
            assert(pthread_mutex_unlock(&access_watch) == 0);
        }
        assert(pthread_mutex_lock(&s->lock) == 0); return pdTRUE;
    }
    if (s == D.frame_done) {
        assert(boost.held); /* every frame at the full clock, whatever the power state says meanwhile */
        if (watching) watched_levels |= 1u << nibble(D.c.fb4, 4, 3);
        scans++;
        now += (uint64_t)(drop_frame ? timeout : 2) * 10000;
        if (!drop_frame && lcd_done) lcd_done(lcd_ctx);
        return !drop_frame;
    }
    return pdTRUE;
}
void xSemaphoreGive(SemaphoreHandle_t s) { if (s->mutex) assert(pthread_mutex_unlock(&s->lock) == 0); }
void xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t *woken) {
    assert(s == D.frame_done);
    isr_gives++;
    *woken = pdFALSE;
}
int xTaskCreatePinnedToCore(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned prio, TaskHandle_t *out, int core) {
    (void)name; (void)prio; (void)core;
    assert(stack == 4096);
    struct test_task *t = record_alloc(sizeof *t);
    *t = (struct test_task){fn, arg};
    if (out) *out = t;
    return pdPASS;
}
void xTaskNotifyGive(TaskHandle_t t) { assert(t); notify_count++; }
unsigned ulTaskNotifyTake(int clear, uint32_t ticks) {
    (void)ticks;
    assert(clear);
    if (loops++) pthread_exit(NULL);
    return 1;
}
void vTaskDelay(uint32_t ticks) {
    if (selftest_mode) { /* the guard task runs alongside: one check per wait */
        now += (uint64_t)ticks * 10000;
        if (guard_alive) guard_check();
        return;
    }
    if (loops++) pthread_exit(NULL);
    now += (uint64_t)ticks * 10000;
}
void taskYIELD(void) { now += 1000; }
void vTaskPrioritySet(TaskHandle_t t, unsigned priority) { (void)t; (void)priority; }
uint32_t xTaskGetTickCount(void) { return (uint32_t)(now / 10000); }
int xPortGetCoreID(void) { return 0; }
void epd_lcd_init(const LcdEpdConfig_t *cfg, int w, int h) {
    assert(w == W && h == H && cfg->pixel_clock == 20000000 && cfg->bus_width == 8);
}
void epd_lcd_line_source_cb(bool (*fn)(void *, uint8_t *), void *ctx) { (void)fn; (void)ctx; }
void epd_lcd_frame_done_cb(void (*fn)(void *), void *ctx) { lcd_done = fn; lcd_ctx = ctx; }
void epd_lcd_start_frame(void) { }
void epd_lcd_abort_frame(void) { abort_count++; }
LineQueue_t lq_init(int size, int bytes) { return (LineQueue_t){.size = size, .element_size = (size_t)bytes}; }
uint8_t *lq_current(LineQueue_t *q) { (void)q; return queue_line; }
void lq_commit(LineQueue_t *q) { atomic_fetch_add(&q->current, 1); }
int lq_read(LineQueue_t *q, uint8_t *out) {
    if (atomic_load(&q->current) == atomic_load(&q->last)) return -1;
    atomic_fetch_add(&q->last, 1);
    memset(out, 0x55, LINE_BYTES);
    return 0;
}
void lq_reset(LineQueue_t *q) { atomic_store(&q->current, 0); atomic_store(&q->last, 0); }

static void finished(void *ctx, const hal_display_result_t *r) {
    assert(ctx == &calls);
    assert(!pthread_equal(caller, pthread_self()));
    assert(scans > 0 && hal_display_pending() == expected_pending && xoe == 0);
    assert(r->queued_us <= r->started_us && r->started_us <= r->done_us);
    callback_thread = pthread_self();
    completed = *r;
    calls++;
}
static void *run_display(void *ctx) { display_task(ctx); return NULL; }
static void *run_guard(void *ctx) { guard(ctx); return NULL; }
static void run(void *(*fn)(void *)) {
    pthread_t task;
    assert(pthread_create(&task, NULL, fn, NULL) == 0);
    assert(pthread_join(task, NULL) == 0);
}
static unsigned nibble(const uint8_t *fb, int x, int y) {
    return (fb[(size_t)y * (W / 2) + x / 2] >> ((x & 1) * 4)) & 15;
}
static void regions(void) {
    hal_rect_t a = {1, 3, 3, 2};
    uint8_t pixels[8] = {0, 17, 255, 99, 34, 136, 221, 99};
    uint32_t id = hal_display_update(&a, pixels, 4, HAL_WF_GL16, 0, finished, &calls);
    assert(id && calls == 0 && hal_display_pending() == 1);
    assert(!hal_display_update(&a, pixels, 4, HAL_WF_GL16, 0, NULL, NULL));
    memset(pixels, 99, sizeof pixels); /* The caller may reuse the input immediately. */
    assert(nibble(D.target, 1, 3) == 0 && nibble(D.target, 2, 3) == 1 && nibble(D.target, 3, 3) == 15);
    assert(nibble(D.target, 1, 4) == 2 && nibble(D.target, 2, 4) == 8 && nibble(D.target, 3, 4) == 13);
    assert(nibble(D.target, 0, 3) == 15 && nibble(D.target, 4, 3) == 15);
    target(&a, -1);
    assert(nibble(D.c.fb4, 2, 4) == 8 && D.c.tile_dirty[D.c.ntx] == 1);
    run(run_display);
    assert(calls == 1 && completed.ok && completed.id == id && !ATOMIC(D.stop));
    assert(!pthread_equal(caller, callback_thread) && isr_gives == scans);
    a = (hal_rect_t){3, 3, 2, 1};
    uint8_t next[] = {68, 85};
    /* a new page is a GL16 on this panel: one pass of the algorithm, the pixel goes straight to its level (D-110) */
    assert(hal_display_update(&a, next, 2, HAL_WF_GL16, 0, finished, &calls));
    watching = true;
    run(run_display);
    watching = false;
    assert(calls == 2 && completed.ok && watched_levels == 1u << 5);
    /* the counted clean is a GC16: black, white, then the picture */
    uint8_t clean[] = {68, 102};
    assert(hal_display_update(&a, clean, 2, HAL_WF_GC16, 0, finished, &calls));
    watched_levels = 0;
    watching = true;
    run(run_display);
    watching = false;
    assert(calls == 3 && completed.ok && completed.waveform == HAL_WF_GC16);
    assert(watched_levels == ((1u << 0) | (1u << 15) | (1u << 6)));
    assert(nibble(D.c.fb4, 1, 3) == 0 && nibble(D.c.fb4, 3, 3) == 4 && nibble(D.c.fb4, 4, 3) == 6);
}
/* A refresh that loses its frame: rails off at once; the driver takes the next one (the whole panel again), and only
 * FAILS_MAX in a row keep the panel off until the next boot. */
static void timeout(void) {
    hal_rect_t a = {0, 0, 1, 1};
    uint8_t p = 0;
    drop_frame = true;
    for (int i = 1; i <= FAILS_MAX; i++) {
        assert(hal_display_update(&a, &p, 1, HAL_WF_GL16, 0, finished, &calls));
        run(run_display);
        assert(calls == (unsigned)i && !completed.ok && off_count > 0 && abort_count > 0 && !ATOMIC(D.powered) && !boost.held);
        if (i < FAILS_MAX) assert(!ATOMIC(D.stop) && D.first);
    }
    assert(ATOMIC(D.stop) && logged_kept_failing);
    assert(!hal_display_update(&a, &p, 1, HAL_WF_GL16, 0, NULL, NULL));
}
/* One lost frame, then a good one: drawn over the whole panel, and the count of failures starts again. */
static void recover(void) {
    hal_rect_t a = {0, 0, 1, 1};
    uint8_t p = 0;
    drop_frame = true;
    assert(hal_display_update(&a, &p, 1, HAL_WF_GL16, 0, finished, &calls));
    run(run_display);
    assert(calls == 1 && !completed.ok && !ATOMIC(D.stop) && D.first && D.fails == 1);
    drop_frame = false;
    unsigned before = scans;
    assert(hal_display_update(&a, &p, 1, HAL_WF_GL16, 0, finished, &calls));
    run(run_display);
    assert(calls == 2 && completed.ok && !ATOMIC(D.stop) && !D.first && D.fails == 0 && !boost.held);
    unsigned redraw = scans - before; /* black, white and the picture: the GC16 path for a GL16 request */
    before = scans;
    assert(hal_display_update(&a, &p, 1, HAL_WF_GL16, 0, finished, &calls));
    run(run_display);
    assert(calls == 3 && completed.ok && redraw > scans - before);
}
static void guard_only(void) {
    atomic_store(&D.powered, true);
    atomic_store(&D.armed, true);
    atomic_store(&D.beat, 0);
    xoe = 1;
    now = GUARD_MS * 1000 - 50000;
    run(run_guard); /* Display task never runs: its own timeout cannot help. */
    assert(ATOMIC(D.stop) && !ATOMIC(D.powered) && !ATOMIC(D.armed));
    assert(xoe == 0 && off_count == 1 && abort_count == 1);
}
static void wake_overlap(void) {
    hal_rect_t a = {0, 0, 1, 1};
    uint8_t pixel = 0;
    assert(hal_display_update(&a, &pixel, 1, HAL_WF_GL16, 0, finished, &calls));
    wake_during_off = true; /* Input arrives while the PMIC power-down yields. */
    expected_pending = 1;
    run(run_display);
    assert(calls == 1 && completed.ok && hal_display_pending() == 1 && !ATOMIC(D.powered));
    run(run_display);
    assert(ATOMIC(D.powered) && hal_display_pending() == 1);
    now += 500000;
    run(run_display);
    assert(!ATOMIC(D.powered) && hal_display_pending() == 0);
}
/* The guard proves itself before the first drive; a guard that never trips keeps the panel off for good. */
static void selftest(bool alive) { /* init ran it, before any task could fail with the rails up */
    assert(!ATOMIC(D.powered) && !ATOMIC(D.armed) && !ATOMIC(D.testing) && !pmic_awake && off_count == 1);
    assert(strstr(last_log, alive ? "tripped=1" : "stays off"));
    assert(ATOMIC(D.stop) == !alive && abort_count == 0);
    hal_rect_t a = {0, 0, 1, 1};
    uint8_t p = 0;
    assert(!hal_display_update(&a, &p, 1, HAL_WF_GL16, 0, NULL, NULL) == !alive);
}
static void lines(void) {
    uint8_t line[LINE_BYTES];
    atomic_store(&D.row, H);
    memset(line, 255, sizeof line);
    assert(!line_source(NULL, line));
    for (unsigned i = 0; i < sizeof line; i++) assert(line[i] == 0);
    atomic_store(&D.row, 0);
    assert(!line_source(NULL, line));
    assert(ATOMIC(D.missed) == 1 && D.owed[0] == 1);
    atomic_store(&D.q[0].current, 2);
    atomic_store(&D.row, 2);
    assert(!line_source(NULL, line));
    assert(D.owed[0] == 0 && atomic_load(&D.q[0].last) == 2 && line[0] == 0x55);
}
static void camera_access(void) {
    atomic_store(&D.powered, true); atomic_store(&D.armed, true);
    hal_display_pause(true);
    assert(!ATOMIC(D.powered) && !ATOMIC(D.armed));
    hal_rect_t area = {0, 0, 1, 1}; uint8_t pixel = 0;
    assert(hal_display_update(&area, &pixel, 1, HAL_WF_GL16, 0, finished, &calls));
    pthread_t thread;
    watching_access = true;
    assert(pthread_mutex_lock(&access_watch) == 0);
    assert(pthread_create(&thread, NULL, run_display, NULL) == 0);
    while (!access_waiting) assert(pthread_cond_wait(&access_wait, &access_watch) == 0);
    assert(!scans && !calls); /* The pending waveform has reached the camera's gate. */
    assert(pthread_mutex_unlock(&access_watch) == 0);
    hal_display_pause(false);
    assert(pthread_join(thread, NULL) == 0);
    assert(calls == 1 && completed.ok && scans > 0);
}
int main(int argc, char **argv) {
    caller = pthread_self();
    const char *which = argc > 1 ? argv[1] : "regions";
    hal_panel_info_t info;
    selftest_mode = true; /* init proves the guard; the guard task runs alongside */
    guard_alive = strcmp(which, "selftest-dead") != 0;
    hal_display_get_info(&info);
    selftest_mode = false;
    assert(info.width == W && info.height == H && info.rotation_deg == S31_PANEL_ROTATION && !info.async_updates);
    assert(info.waveforms == ((1u << HAL_WF_GC16) | (1u << HAL_WF_GL16)));
    assert(info.cleans_pages); /* the system then sends GC16 only for its counted clean */
    assert(hal_display_level_luminance(0) == PANEL_LEVEL[0] && hal_display_level_luminance(16) == 1.0f);
    assert(!hal_display_update(NULL, NULL, 0, HAL_WF_GL16, 0, NULL, NULL));
    if (strcmp(which, "selftest") && strcmp(which, "selftest-dead")) off_count = 0; /* the other cases start after it */
    if (!strcmp(which, "regions")) regions();
    else if (!strcmp(which, "timeout")) timeout();
    else if (!strcmp(which, "recover")) recover();
    else if (!strcmp(which, "guard")) guard_only();
    else if (!strcmp(which, "lines")) lines();
    else if (!strcmp(which, "camera-access")) camera_access();
    else if (!strcmp(which, "wake-overlap")) wake_overlap();
    else if (!strcmp(which, "selftest")) selftest(true);
    else if (!strcmp(which, "selftest-dead")) selftest(false);
    else if (!strcmp(which, "shutdown")) {
        atomic_store(&D.powered, true);
        ink_s31_display_shutdown();
        assert(ATOMIC(D.stop) && !ATOMIC(D.powered) && !ATOMIC(D.armed));
        assert(off_count == 1 && abort_count == 1 && xoe == 0);
    } else if (!strcmp(which, "panic")) {
        ink_s31_display_panic();
        assert(ATOMIC(D.stop) && lcd_stopped && off_count == 1 && xoe == 0);
    }
    else abort();
    assert(pthread_mutex_destroy(&D.lock->lock) == 0);
    assert(pthread_mutex_destroy(&D.access->lock) == 0);
    for (unsigned i = 0; i < allocation_count; i++) free(allocations[i]);
    printf("display %s: passed\n", which);
    return 0;
}
