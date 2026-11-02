/*
 * Simulator self-test: checks that the simulated device behaves like the
 * profile says (tick quantization, preemption, priority inheritance, queue
 * timeouts, heap limits, e-ink timing and CPU load, flash stalls, input
 * latency, LittleFS persistence). Runs against every profile in CI.
 *
 * Results are reported as probe counters "test.pass"/"test.fail"; the budget
 * file requires zero failures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/hal_device.h"
#include "ink/hal_display.h"
#include "ink/hal_input.h"
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "sim.h" /* the self-test may peek at the profile to compute expectations */

#define CHECK(cond, ...)                                       \
    do {                                                       \
        if (cond) {                                            \
            ink_probe_count("test.pass");                      \
        } else {                                               \
            char _m[300];                                      \
            snprintf(_m, sizeof _m, __VA_ARGS__);              \
            sim_warn("CHECK FAILED (%s:%d) %s", __FILE__, __LINE__, _m); \
            ink_probe_count("test.fail");                      \
        }                                                      \
    } while (0)

static uint64_t tick_us(void) { return g_profile.tick_hz ? 1000000ull / (uint64_t)g_profile.tick_hz : 1; }
static uint64_t mhz(void) { return (uint64_t)g_profile.mhz; }

static void spawn(const char *name, osal_task_fn fn, void *arg, int prio, int core, uint32_t stack, uint32_t flags) {
    osal_task_cfg_t c = {.name = name, .fn = fn, .arg = arg, .stack_bytes = stack, .priority = prio, .core = core, .flags = flags};
    if (osal_task_create(&c, NULL) != OSAL_OK) osal_panic("selftest: cannot create %s", name);
}

/* ------------------------------------------------------------- 1. ticks */

static void test_ticks(void) {
    osal_task_delay_us(1); /* align to a tick */
    uint64_t t1 = osal_now_us();
    CHECK(t1 % tick_us() < 200, "delay should wake on a tick boundary (t=%llu, tick=%llu)", (unsigned long long)t1,
          (unsigned long long)tick_us());
    osal_task_delay_us(tick_us() / 2);
    uint64_t t2 = osal_now_us();
    CHECK(t2 - t1 >= tick_us() / 2 && t2 - t1 <= tick_us() + 200, "half-tick delay rounds up to one tick (%llu us)",
          (unsigned long long)(t2 - t1));
}

/* ------------------------------------------------------- 2. preemption */

static volatile bool hog_run;
static osal_sem_t *sem_hi;
static uint64_t t_timer_cb, t_hi_woke;

static void hog_task(void *arg) {
    while (hog_run) osal_cost_cycles(mhz() * 1000); /* 1 ms chunks of pure computation */
}

static void timer_cb(void *arg) {
    t_timer_cb = osal_now_us();
    osal_sem_give(sem_hi);
}

static void hi_task(void *arg) {
    osal_sem_take(sem_hi, OSAL_WAIT_FOREVER);
    t_hi_woke = osal_now_us();
    hog_run = false; /* main (priority 1) is starved on this core while the hog (priority 3) computes */
}

static void test_preemption(void) {
    sem_hi = osal_sem_create(0, 1);
    hog_run = true;
    spawn("hi", hi_task, NULL, 10, 0, 2048, 0);
    osal_timer_t *t = osal_timer_create("wake-hi", timer_cb, NULL);
    osal_timer_start(t, 5000, 0);
    /* created last: the hog (priority 3) preempts main (priority 1) on core 0 right away,
     * and main only continues once `hi` stops it */
    spawn("hog", hog_task, NULL, 3, 0, 2048, 0);
    osal_task_delay_us(30000);
    uint64_t lat = t_hi_woke - t_timer_cb;
    CHECK(t_hi_woke && lat < 1500, "high-priority task should preempt a CPU hog quickly (latency %llu us)",
          (unsigned long long)lat);
    osal_timer_delete(t);
    osal_task_delay_us(5000);
}

/* ----------------------------------------------- 3. priority inheritance */

static osal_mutex_t *pi_mutex;
static uint64_t pi_t0, pi_h_start, pi_h_got;

static void pi_low(void *arg) {
    osal_mutex_lock(pi_mutex, OSAL_WAIT_FOREVER);
    for (int i = 0; i < 20; i++) osal_cost_cycles(mhz() * 1000); /* 20 ms holding the lock */
    osal_mutex_unlock(pi_mutex);
}

static void pi_mid(void *arg) {
    for (int i = 0; i < 60; i++) osal_cost_cycles(mhz() * 1000); /* 60 ms unrelated work */
}

static void pi_high(void *arg) {
    pi_h_start = osal_now_us();
    osal_mutex_lock(pi_mutex, OSAL_WAIT_FOREVER);
    pi_h_got = osal_now_us();
    osal_mutex_unlock(pi_mutex);
}

static void test_priority_inheritance(void) {
    if (g_profile.cores < 2) return;
    pi_mutex = osal_mutex_create();
    pi_t0 = osal_now_us();
    spawn("pi_low", pi_low, NULL, 2, 1, 2048, 0);
    osal_task_delay_us(1); /* next tick: low holds the lock */
    spawn("pi_mid", pi_mid, NULL, 6, 1, 2048, 0);
    spawn("pi_high", pi_high, NULL, 9, 1, 2048, 0);
    osal_task_delay_us(120000);
    uint64_t wait = pi_h_got - pi_h_start;
    CHECK(pi_h_got && wait < 25000, "priority inheritance bounds the high task's wait (%llu us; ~20 ms expected, "
          "~80 ms without inheritance)", (unsigned long long)wait);
}

/* --------------------------------------------------------- 4. queues */

static void test_queues(void) {
    osal_queue_t *q = osal_queue_create(sizeof(int), 4);
    int v = 7, out = 0;
    CHECK(osal_queue_recv(q, &out, 0) == OSAL_EMPTY, "non-blocking receive on an empty queue");
    for (int i = 0; i < 4; i++) osal_queue_send(q, &v, 0);
    CHECK(osal_queue_send(q, &v, 0) == OSAL_FULL, "non-blocking send on a full queue");
    for (int i = 0; i < 4; i++) osal_queue_recv(q, &out, 0);
    uint64_t t0 = osal_now_us();
    osal_err_t e = osal_queue_recv(q, &out, 15000);
    uint64_t dt = osal_now_us() - t0;
    CHECK(e == OSAL_TIMEOUT && dt >= 15000 && dt <= 15000 + tick_us() + 500,
          "receive timeout honours the tick (%llu us for 15000)", (unsigned long long)dt);
    osal_queue_delete(q);
}

/* --------------------------------------------------------- 5. memory */

static void test_memory(void) {
    osal_mem_stats_t before, after;
    osal_mem_stats(OSAL_MEM_INTERNAL, &before);
    void *blocks[512];
    int n = 0;
    while (n < 512 && (blocks[n] = osal_malloc(4096, OSAL_MEM_INTERNAL)) != NULL) n++;
    size_t got = (size_t)n * 4096;
    CHECK((n < 512 || g_profile.internal_heap >= 2u * 1024 * 1024) && got <= g_profile.internal_heap + g_profile.dma_heap,
          "internal RAM is limited to the profile budget (got %zu bytes)", got);
    for (int i = 0; i < n; i++) osal_free(blocks[i]);
    osal_mem_stats(OSAL_MEM_INTERNAL, &after);
    CHECK(after.free == before.free, "freeing restores internal heap (%zu vs %zu)", after.free, before.free);
    void *big = osal_malloc(512 * 1024, OSAL_MEM_DEFAULT);
    CHECK(big != NULL, "a 512 KiB block fits in external RAM");
    osal_free(big);
}

/* ------------------------------------------------------- 6. display */

static osal_queue_t *disp_q;
static void disp_done(void *ctx, const hal_display_result_t *r) { osal_queue_send(disp_q, r, OSAL_WAIT_FOREVER); }

static volatile bool counter_run;
static volatile uint64_t counter_ms;
static void counter_task(void *arg) {
    while (counter_run) {
        osal_cost_cycles(mhz() * 1000);
        counter_ms++;
    }
}

static void test_display(void) {
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    disp_q = osal_queue_create(sizeof(hal_display_result_t), 4);
    uint8_t *fb = osal_malloc((size_t)pi.width * pi.height, OSAL_MEM_EXTERNAL);
    CHECK(fb != NULL, "full-screen 8-bit buffer fits in external RAM");
    if (!fb) return;
    for (int y = 0; y < pi.height; y++)
        for (int x = 0; x < pi.width; x++) fb[(size_t)y * pi.width + x] = (uint8_t)(((x / 64 + y / 64) & 1) ? 255 : 0);

    /* measure how much CPU a background task gets on core 1 while a full GC16 refresh plays */
    counter_run = true;
    counter_ms = 0;
    spawn("bg_counter", counter_task, NULL, 4, 1 % g_profile.cores, 2048, 0);

    hal_rect_t full = {0, 0, pi.width, pi.height};
    uint32_t id = hal_display_update(&full, fb, pi.width, HAL_WF_GC16, 0, disp_done, NULL);
    CHECK(id > 0, "display accepts a full update");
    hal_display_result_t r;
    osal_queue_recv(disp_q, &r, OSAL_WAIT_FOREVER);
    counter_run = false;
    if (pi.tech == HAL_PANEL_EPD) {
        uint64_t play = (uint64_t)g_profile.wf[HAL_WF_GC16].phases * g_profile.frame_time_us;
        uint64_t pu = g_profile.power_up_us;
        CHECK(r.started_us - r.queued_us >= pu && r.started_us - r.queued_us <= pu + 20000,
              "GC16 starts after the PMIC power-up (%llu us, expected ~%llu)",
              (unsigned long long)(r.started_us - r.queued_us), (unsigned long long)pu);
        CHECK(r.done_us - r.started_us >= play - 100 && r.done_us - r.started_us <= play + 1000,
              "GC16 plays phases x frame time (%llu us, expected %llu)", (unsigned long long)(r.done_us - r.started_us),
              (unsigned long long)play);
        if (g_profile.prep_cpu_duty > 0) {
            double total_ms = (double)(r.done_us - r.queued_us) / 1000.0;
            double share = (double)counter_ms / total_ms;
            double expect = ((double)pu + (double)play * (1.0 - g_profile.prep_cpu_duty)) / 1000.0 / total_ms;
            CHECK(share < expect + 0.08, "the EPD feeders leave only ~%.0f%% of a core during refresh (got %.0f%%)",
                  expect * 100, share * 100);
        }
        /* partial DU with power held: no power-up on the second update, same full-frame scan time */
        hal_display_hold_power(1000);
        hal_rect_t small = {100, 100, 64, 32};
        hal_display_update(&small, fb, pi.width, HAL_WF_DU, 0, disp_done, NULL);
        osal_queue_recv(disp_q, &r, OSAL_WAIT_FOREVER);
        hal_display_update(&small, fb + 64, pi.width, HAL_WF_DU, 0, disp_done, NULL);
        osal_queue_recv(disp_q, &r, OSAL_WAIT_FOREVER);
        uint64_t du = (uint64_t)g_profile.wf[r.waveform].phases * g_profile.frame_time_us;
        CHECK(r.started_us - r.queued_us < 2000, "held rails: no power-up delay (%llu us)",
              (unsigned long long)(r.started_us - r.queued_us));
        CHECK(r.done_us - r.started_us >= du - 100, "a tiny partial update still scans the full panel (%llu us)",
              (unsigned long long)(r.done_us - r.started_us));
        hal_display_hold_power(0);
    } else {
        CHECK(r.done_us - r.queued_us < 50000, "LCD update visible within a few frames (%llu us)",
              (unsigned long long)(r.done_us - r.queued_us));
    }
    osal_free(fb);
    osal_task_delay_us(50000);
}

/* --------------------------------------------------- 7. flash stalls */

static volatile bool victim_run;
static uint64_t victim_gap_max, iram_gap_max;

static void victim_task(void *arg) { /* stack in PSRAM: freezes while flash is written */
    uint64_t last = osal_now_us();
    while (victim_run) {
        osal_cost_cycles(mhz() * 200);
        uint64_t now = osal_now_us();
        if (now - last > victim_gap_max) victim_gap_max = now - last;
        last = now;
    }
}

static void iram_task(void *arg) { /* IRAM-safe: keeps running */
    uint64_t last = osal_now_us();
    while (victim_run) {
        osal_cost_cycles(mhz() * 200);
        uint64_t now = osal_now_us();
        if (now - last > iram_gap_max) iram_gap_max = now - last;
        last = now;
    }
}

static void test_flash(void) {
    victim_run = true;
    int other = g_profile.cores > 1 ? 1 : 0;
    spawn("psram_victim", victim_task, NULL, 3, other, 2048, OSAL_TASK_STACK_EXTERNAL);
    spawn("iram_worker", iram_task, NULL, 3, other, 2048, OSAL_TASK_IRAM_SAFE);
    osal_task_delay_us(20000);
    static uint8_t data[8192];
    for (size_t i = 0; i < sizeof data; i++) data[i] = (uint8_t)(i * 31 + 7);
    uint64_t t0 = osal_now_us();
    hal_file_t *f = hal_fs_open("/user/selftest.bin", HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    CHECK(f != NULL, "open /user file for writing");
    if (!f) return;
    CHECK(hal_fs_write(f, data, sizeof data) == (ssize_t)sizeof data, "write 8 KiB to /user");
    CHECK(hal_fs_close(f) == 0, "close /user file");
    uint64_t dt = osal_now_us() - t0;
    victim_run = false;
    osal_task_delay_us(20000);
    if (g_profile.flash_stalls_cpu) {
        CHECK(victim_gap_max >= g_profile.erase_us_typ * 9 / 10,
              "a PSRAM task freezes for a whole sector erase (max gap %llu us, erase typ %u us)",
              (unsigned long long)victim_gap_max, g_profile.erase_us_typ);
        CHECK(iram_gap_max < 5000, "an IRAM-safe task keeps running during flash writes (max gap %llu us)",
              (unsigned long long)iram_gap_max);
    }
    ink_probe_value("selftest.user_write_8k_ms", dt / 1000.0);
    static uint8_t back[8192]; /* never put buffers this size on a task stack */
    f = hal_fs_open("/user/selftest.bin", HAL_O_READ);
    CHECK(f && hal_fs_read(f, back, sizeof back) == (ssize_t)sizeof back && !memcmp(back, data, sizeof data),
          "read back /user data");
    if (f) hal_fs_close(f);
    hal_fs_volume_info_t vi;
    CHECK(hal_fs_volume_info("/user", &vi) == 0 && vi.total_bytes > 0 && vi.free_bytes < vi.total_bytes,
          "volume info for /user");
}

/* ----------------------------------------------------- 8. SD card */

static void test_sd(void) {
    static uint8_t buf[64 * 1024];
    memset(buf, 0x5a, sizeof buf);
    uint64_t t0 = osal_now_us();
    hal_fs_mkdir("/content/books");
    hal_file_t *f = hal_fs_open("/content/books/selftest.dat", HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    CHECK(f != NULL, "open SD file");
    if (!f) return;
    hal_fs_write(f, buf, sizeof buf);
    hal_fs_close(f);
    uint64_t dt = osal_now_us() - t0;
    double mbps = g_profile.sd_write_mbps;
    uint64_t min_us = (uint64_t)(sizeof buf / mbps);
    CHECK(dt >= min_us, "64 KiB SD write takes at least size/bandwidth (%llu us >= %llu)", (unsigned long long)dt,
          (unsigned long long)min_us);
    hal_dir_t *d = hal_fs_opendir("/content/books");
    hal_dirent_t e;
    int found = 0;
    while (d && hal_fs_readdir(d, &e) == 1)
        if (!strcmp(e.name, "selftest.dat") && e.size == (int64_t)sizeof buf) found = 1;
    hal_fs_closedir(d);
    CHECK(found, "directory listing shows the SD file");
}

/* ----------------------------------------------------- 9. input */

#define MAXEV 32
static hal_input_event_t evs[MAXEV];
static uint64_t ev_rx[MAXEV];
static int nev;

static void sink(const hal_input_event_t *e, void *ctx) {
    if (nev < MAXEV) {
        evs[nev] = *e;
        ev_rx[nev] = osal_now_us();
        nev++;
    }
}

static void check_input(void) {
    int downs = 0, ups = 0, keys = 0;
    for (int i = 0; i < nev; i++) {
        if (evs[i].type == HAL_IN_TOUCH && evs[i].touch.phase == HAL_TOUCH_DOWN) {
            downs++;
            uint64_t lat = ev_rx[i] - evs[i].t_us;
            CHECK(lat >= g_profile.touch_detect_us && lat < g_profile.touch_detect_us + 30000,
                  "touch-down reaches the system after detect + I2C read (%llu us)", (unsigned long long)lat);
        }
        if (evs[i].type == HAL_IN_TOUCH && evs[i].touch.phase == HAL_TOUCH_UP) ups++;
        if (evs[i].type == HAL_IN_KEY) keys++;
    }
    if (g_profile.has_touch) CHECK(downs == 2 && ups == 2, "two scripted taps -> 2 downs, 2 ups (got %d/%d)", downs, ups);
    if (g_profile.keys_mask & (1u << HAL_KEY_NEXT)) CHECK(keys == 2, "one scripted key press -> press + release (got %d)", keys);
}

/* ------------------------------------------------------------- main */

void ink_main(void) {
    hal_input_set_sink(sink, NULL);
    hal_device_info_t di;
    hal_device_get_info(&di);
    CHECK(di.cpu_cores == g_profile.cores, "device info matches the profile");
    test_ticks();
    test_preemption();
    test_priority_inheritance();
    test_queues();
    test_memory();
    test_display();
    test_flash();
    test_sd();
    /* the script taps at 4.0 s and 4.5 s and presses NEXT at 5 s */
    uint64_t now = osal_now_us();
    if (now < 6500000) osal_task_delay_us(6500000 - now);
    check_input();
    ink_probe_mark("selftest-done");
}
