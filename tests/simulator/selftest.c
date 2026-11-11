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

#include "ink/hal_audio.h"
#include "ink/hal_camera.h"
#include "ink/hal_device.h"
#include "ink/hal_display.h"
#include "ink/hal_input.h"
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "bh1098g/bh1098g.h"
#include "metalio_plus/metalio_board.h"
#include "metalio_plus/metalio_epd_power.h"
#include "ink/hal_net.h"
#include "ink/hal_power.h"
#include "ink/probe.h"
#include "ink/rotation.h"
#include "ft6336u/ft6336u.h"
#include "nt26/nt26.h"
#include "tps65185/tps65185.h"
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
        int phases = g_profile.wf[HAL_WF_GC16].phases;
        if (g_profile.gc16_full_phases > phases) phases = g_profile.gc16_full_phases; /* full-screen GC16: complete table */
        uint64_t ph_ns, b0, b1;
        double slack;
        sim_display_plan_info(&full, &ph_ns, &b0, &b1, &slack);
        uint64_t play = (uint64_t)phases * ph_ns / 1000;
        uint64_t pu = g_profile.power_up_us;
        double diff_cycles_px = profile_cost("epd.diff_px") + profile_cost("psram_to_internal.copy_byte") +
                                profile_cost("internal_to_psram.copy_byte");
        uint64_t diff_us = g_profile.diff_pass ? (uint64_t)((double)pi.width * pi.height * diff_cycles_px / g_profile.mhz) : 0;
        CHECK(r.started_us - r.queued_us >= pu + diff_us * 9 / 10 && r.started_us - r.queued_us <= pu + diff_us + 20000,
              "GC16 starts after the PMIC power-up and the difference pass (%llu us, expected ~%llu)",
              (unsigned long long)(r.started_us - r.queued_us), (unsigned long long)(pu + diff_us));
        CHECK(r.done_us - r.started_us >= play - 100 && r.done_us - r.started_us <= play + 1000,
              "GC16 plays phases x the planned phase (%llu us, expected %llu)", (unsigned long long)(r.done_us - r.started_us),
              (unsigned long long)play);
        if (g_profile.feeders_cpu) {
            CHECK(slack > 0, "a full-screen phase leaves the feeders ahead of the scan (slack %.1f lines)", slack);
            double total_ms = (double)(r.done_us - r.queued_us) / 1000.0;
            double share = (double)counter_ms / total_ms;
            double free_us = (double)phases * (double)(ph_ns - (b1 < ph_ns ? b1 : ph_ns)) / 1000.0;
            double expect = ((double)pu + (double)diff_us + free_us) / 1000.0 / total_ms;
            CHECK(share < expect + 0.08, "the EPD feeders leave only ~%.0f%% of core 1 during refresh (got %.0f%%)",
                  expect * 100, share * 100);
        }
        /* partial DU with power held: no power-up on the second update, same full-frame scan time */
        hal_display_hold_power(1000);
        hal_rect_t small = {100, 100, 64, 32};
        hal_display_update(&small, fb, pi.width, HAL_WF_DU, 0, disp_done, NULL);
        osal_queue_recv(disp_q, &r, OSAL_WAIT_FOREVER);
        hal_display_update(&small, fb + 64, pi.width, HAL_WF_DU, 0, disp_done, NULL);
        osal_queue_recv(disp_q, &r, OSAL_WAIT_FOREVER);
        uint64_t du_ph;
        sim_display_plan_info(&small, &du_ph, &b0, &b1, &slack);
        uint64_t du = (uint64_t)g_profile.wf[r.waveform].phases * du_ph / 1000;
        uint64_t small_diff_us = g_profile.diff_pass ? (uint64_t)(32.0 * (g_profile.diff_pass == 2 ? pi.width : 64) * diff_cycles_px / g_profile.mhz) : 0;
        CHECK(r.started_us - r.queued_us < 2000 + small_diff_us, "held rails: no power-up delay (%llu us)",
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
static uint64_t victim_gap_max, worker_gap_max, victim_iters, worker_iters;

static void spin_task(void *arg) {
    bool victim = arg != NULL;
    uint64_t last = osal_now_us();
    while (victim_run) {
        osal_cost_cycles(mhz() * 200);
        uint64_t now = osal_now_us();
        uint64_t *gap = victim ? &victim_gap_max : &worker_gap_max;
        if (now - last > *gap) *gap = now - last;
        last = now;
        if (victim) victim_iters++;
        else worker_iters++;
    }
}

static void test_flash(void) {
    victim_run = true;
    int other = g_profile.cores > 1 ? 1 : 0;
    /* two equal-priority CPU hogs on the same core: one with a PSRAM stack, one with an internal stack */
    spawn("psram_victim", spin_task, (void *)1, 3, other, 2048, OSAL_TASK_STACK_EXTERNAL);
    spawn("internal_worker", spin_task, NULL, 3, other, 2048, 0);
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
    CHECK(victim_iters > 10 && worker_iters > 10,
          "equal-priority tasks share a core by tick time slicing (%llu / %llu iterations)",
          (unsigned long long)victim_iters, (unsigned long long)worker_iters);
    if (g_profile.flash_stalls_cpu) {
        CHECK(victim_gap_max >= g_profile.erase_us_typ * 9 / 10,
              "a PSRAM-stack task freezes for a whole sector erase (max gap %llu us, erase typ %u us)",
              (unsigned long long)victim_gap_max, (unsigned)g_profile.erase_us_typ);
        CHECK(worker_gap_max >= g_profile.erase_us_typ * 9 / 10,
              "an internal-stack task freezes too: the scheduler is suspended on both cores (max gap %llu us)",
              (unsigned long long)worker_gap_max);
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

/* ------------------------------------------ 7a. nothing runs in a stall */

/* While a flash operation freezes the device, a task on the other core must not get work done,
 * even when its CPU slice started before the stall was registered (review M-B). */
static volatile bool leak_run;
static volatile uint64_t leak_work_ns;

static void leak_task(void *arg) {
    (void)arg;
    while (leak_run) {
        sim_charge_ns_always(200000);
        leak_work_ns += 200000;
    }
}

static void test_stall_leak(void) {
    if (!g_profile.flash_stalls_cpu || g_profile.cores < 2) return;
    leak_run = true;
    leak_work_ns = 0;
    spawn("leak_busy", leak_task, NULL, 6, 1, 2048, 0);
    osal_task_delay_us(5000);
    uint64_t t0 = osal_now_us(), w0 = leak_work_ns, s0 = vtk_stall_total_ns();
    for (int i = 0; i < 200; i++) {
        vtk_consume_ns(150000); /* pending CPU of the caller, not yet charged */
        vtk_lock();
        vtk_stall_begin_locked(vtk_now() + 530000);
        vtk_unlock();
        vtk_sched_point();
        osal_task_delay_us(1000);
    }
    uint64_t elapsed = (osal_now_us() - t0) * 1000, stalled = vtk_stall_total_ns() - s0, work = leak_work_ns - w0;
    leak_run = false;
    CHECK(work <= elapsed - stalled + 400000, "no work during stalls on the other core (%.1f ms done in %.1f ms awake)",
          work / 1e6, (elapsed - stalled) / 1e6);
}

/* ------------------------------------- 7b. flash write during a waveform */

/* A flash stall longer than the line queues can bridge, in the middle of a waveform, must be caught
 * as an underrun (the feeders freeze while the interrupt keeps pulling lines) and cost the upstream
 * recovery. (Whether a short program stall is tolerated depends on the queue level at that moment.) */
static void test_underrun(void) {
    if (!g_profile.flash_stalls_cpu || !g_profile.feeders_cpu) return;
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    static uint8_t px[64 * 32];
    memset(px, 0, sizeof px);
    hal_rect_t small = {200, 200, 64, 32};
    uint32_t id = hal_display_update(&small, px, 64, HAL_WF_DU, 0, disp_done, NULL);
    CHECK(id > 0, "queue a DU update for the underrun test");
    for (int i = 0; i < 400 && !sim_display_playing(); i++) osal_task_delay_us(1000);
    osal_task_delay_us(100000); /* well inside the waveform */
    CHECK(sim_display_playing(), "the DU is still playing when the stall starts");
    /* right after the next phase's scan starts: more lines left than the queues can bridge (a stall
     * near the end of a scan, or before it, only delays the next phase); e.g. a sector erase */
    uint64_t bridge_ns = (uint64_t)(2 * g_profile.line_queue_lines + 2 * g_profile.bounce_lines) * g_profile.line_ns;
    sim_display_stall_next_scan(bridge_ns + 2000000);
    hal_display_result_t r;
    osal_queue_recv(disp_q, &r, OSAL_WAIT_FOREVER);
    hal_rect_t all = {0, 0, pi.width, pi.height};
    uint64_t ph_ns, b0, b1;
    double slack;
    sim_display_plan_info(&all, &ph_ns, &b0, &b1, &slack);
    uint64_t clear_ns = (uint64_t)g_profile.line_count * g_profile.line_ns + (uint64_t)g_profile.frame_overhead_us * 1000;
    uint64_t rec = ((uint64_t)g_profile.clear_frames * clear_ns + (uint64_t)g_profile.gc16_full_phases * ph_ns) / 1000;
    CHECK(r.done_us - r.started_us >= rec,
          "a flash stall longer than the queues inside a waveform underruns and pays the clear + full GC16 recovery "
          "(%llu us, expected >= %llu)",
          (unsigned long long)(r.done_us - r.started_us), (unsigned long long)rec);
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

/* ----------------------------------------------------- 8b. speaker */

/* The TX ring plays zeros when nobody refills it: an underrun only when that cuts into sound. */
static void test_speaker(void) {
    if (!g_profile.info.has_audio_out) return;
    hal_audio_info_t ai;
    hal_audio_get_info(&ai);
    static int16_t pcm[480];
    uint64_t u0, u1;
    double gap0, gap1, idle0, idle1;
    CHECK(hal_audio_play_start() == 0, "speaker output starts");
    int64_t d = hal_audio_play_delay_us();
    CHECK(d >= (int64_t)g_profile.audio_start_us - 1000 && d <= (int64_t)g_profile.audio_start_us,
          "nothing plays before the amplifier settles (%lld us)", (long long)d);
    for (int i = 0; i < 480; i++) pcm[i] = (i / 8) % 2 ? 6000 : -6000; /* 1 kHz square, no fade */
    sim_audio_speaker_counters(&u0, &gap0, &idle0);
    CHECK(hal_audio_play_write(pcm, 480, 0) == 480, "two descriptors written");
    osal_task_delay_us((uint64_t)g_profile.audio_start_us + 120000);
    sim_audio_speaker_counters(&u1, &gap1, &idle1);
    CHECK(u1 == u0 + 1 && gap1 - gap0 >= 15.0, "running dry right after sound is one audible gap (%llu, %.0f ms)",
          (unsigned long long)(u1 - u0), gap1 - gap0);
    for (int i = 416; i < 480; i++) pcm[i] = 0; /* this time the sound ends in silence */
    hal_audio_play_write(pcm, 480, 0);
    osal_task_delay_us(120000);
    uint64_t u2;
    double gap2, idle2;
    sim_audio_speaker_counters(&u2, &gap2, &idle2);
    CHECK(u2 == u1 && idle2 - idle1 >= 15.0, "running dry after silence is idle, not an underrun (%llu, idle %.0f ms)",
          (unsigned long long)(u2 - u1), idle2 - idle1);
    for (int i = 0; i < 480; i++) pcm[i] = (i / 8) % 2 ? 6000 : -6000;
    hal_audio_play_write(pcm, 480, 0);
    hal_audio_play_stop(true); /* drain: plays out, no gap */
    uint64_t u3;
    double gap3, idle3;
    sim_audio_speaker_counters(&u3, &gap3, &idle3);
    CHECK(u3 == u2, "stopping with drain plays everything without a gap");
    CHECK(ai.runway_ms == g_profile.audio_dma_desc * g_profile.audio_dma_frames * 1000 / HAL_AUDIO_RATE,
          "runway = DMA ring (%d ms)", ai.runway_ms);
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

/* ------------------------------------------- timer delete vs a running callback (osal.h) */

static volatile bool g_tcb_in, g_tcb_done;

static void slow_timer_cb(void *arg) {
    (void)arg;
    g_tcb_in = true;
    sim_charge_ns_always(SIM_MS(5)); /* 5 ms of work in the timer task */
    g_tcb_done = true;
}

static void test_timer_delete(void) {
    osal_timer_t *t = osal_timer_create("slow", slow_timer_cb, NULL);
    g_tcb_in = g_tcb_done = false;
    osal_timer_start(t, 1000, 0);
    for (int i = 0; !g_tcb_in && i < 1000; i++) sim_charge_ns_always(SIM_US(50)); /* spin: on two cores the callback runs meanwhile */
    bool was_running = g_tcb_in && !g_tcb_done;
    osal_timer_delete(t);
    CHECK(g_tcb_in && g_tcb_done, "osal_timer_delete returns only after the callback that was running (%s) finished",
          was_running ? "it was" : "it had ended");
    if (g_profile.cores > 1) CHECK(was_running, "on two cores the delete really met a running callback");
}

/* A callback that re-arms its own timer while another task deletes it (audit AUD3-001): the delete
 * wins, nothing fires afterwards (it would touch freed memory; the ASan build in CI sees that). */
static osal_timer_t *g_rearm;
static volatile bool g_rearm_in;
static volatile int g_rearm_calls, g_rearm_rc = 99;

static void rearm_cb(void *arg) {
    (void)arg;
    if (++g_rearm_calls == 1) {
        g_rearm_in = true;
        sim_charge_ns_always(SIM_MS(2)); /* on two cores the delete arrives meanwhile */
        g_rearm_rc = osal_timer_start(g_rearm, 1000, 0);
    }
}

static void test_timer_rearm_delete(void) {
    g_rearm = osal_timer_create("rearm", rearm_cb, NULL);
    osal_timer_start(g_rearm, 1000, 0);
    for (int i = 0; !g_rearm_in && i < 1000; i++) sim_charge_ns_always(SIM_US(50));
    osal_timer_delete(g_rearm);
    osal_task_delay_us(20000); /* a re-armed firing would land here */
    CHECK(g_rearm_calls == 1, "a timer deleted while its callback re-arms it does not fire again (%d calls)", g_rearm_calls);
    if (g_profile.cores > 1) CHECK(g_rearm_rc == OSAL_INVALID, "re-arming a timer that is being deleted is refused (%d)", g_rearm_rc);
}

static void selftest_task(void *arg) {
    (void)arg;
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
    test_stall_leak();
    test_underrun();
    test_sd();
    test_speaker();
    test_timer_delete();
    test_timer_rearm_delete();
    /* the script taps at 4.0 s and 4.5 s and presses NEXT at 5 s */
    uint64_t now = osal_now_us();
    if (now < 6500000) osal_task_delay_us(6500000 - now);
    check_input();
    ink_probe_mark("selftest-done");
}

static void bad_psram_task(void *arg) {
    (void)arg;
    char buf[16];
    hal_file_t *f = hal_fs_open("/user/anything", HAL_O_READ); /* must not return on an MCU profile */
    if (f) {
        hal_fs_read(f, buf, sizeof buf);
        hal_fs_close(f);
    }
    sim_warn("death test: a PSRAM-stack task read /user without crashing the device");
}

/* INK_SELFTEST_CELL=1: the 4G modem model (two SIM slots, AT commands) driven by our NT26 driver.
 * Online switches keep the device running; LTE goes down for the switch and comes back on the new
 * card; an empty slot leaves it down and can be left again; AT+ECRST makes the modem deaf for a
 * while and it re-registers by itself. */
static bool lte_up(void) {
    hal_link_status_t st;
    hal_net_link_status(&st);
    return st.type == HAL_LINK_LTE && st.up;
}

static void cell_task(void *arg) {
    (void)arg;
    char resp[64];
    if (hal_modem_at("AT", resp, sizeof resp, 1000) == HAL_AT_NO_MODEM) {
        nt26_slot_t s0;
        CHECK(nt26_sim_slot(&s0) == NT26_NO_MODEM, "no modem: the driver says so");
        ink_probe_mark("cell-done");
        return;
    }
    nt26_slot_t slot = NT26_INTERNAL_CARD;
    bool reg = false;
    CHECK(nt26_sim_slot(&slot) == NT26_OK && (int)slot == g_profile.sim_selected, "the slot setting reads back (AT+ECSIMCFG?)");
    CHECK(nt26_registered(&reg) == NT26_OK && reg && lte_up(), "registered at boot, LTE up");
    CHECK(hal_modem_at("AT+FOO", resp, sizeof resp, 1000) == HAL_AT_ERROR, "an unknown command answers ERROR");

    uint64_t t0 = osal_now_us();
    nt26_result_t r = nt26_select_sim(NT26_INTERNAL_CARD, 30000);
    uint64_t took = osal_now_us() - t0;
    CHECK(r == NT26_OK && nt26_sim_slot(&slot) == NT26_OK && slot == NT26_INTERNAL_CARD && lte_up(),
          "online switch to the internal card registers without a reboot (%s)", nt26_result_name(r));
    CHECK(took > 3000000 && took < 10000000, "the switch takes a few seconds (%llu ms)", (unsigned long long)(took / 1000));

    vtk_lock();
    sim_net_set_card(NT26_INTERNAL_CARD, false, false, false); /* the card in use is taken away */
    vtk_unlock();
    CHECK(nt26_registered(&reg) == NT26_OK && !reg && !lte_up(), "losing the card in use drops LTE");
    r = nt26_select_sim(NT26_EXTERNAL_SLOT, 30000);
    CHECK(r == NT26_OK && lte_up(), "switch back to the external card (%s)", nt26_result_name(r));

    r = nt26_select_sim(NT26_INTERNAL_CARD, 8000);
    CHECK(r == NT26_NOT_REGISTERED && !lte_up(), "a switch to the empty slot reports it (%s)", nt26_result_name(r));
    r = nt26_select_sim(NT26_EXTERNAL_SLOT, 30000);
    CHECK(r == NT26_OK && lte_up(), "switching away from the empty slot works (%s)", nt26_result_name(r));

    /* SMS and calls on the external card (data, SMS and voice) */
    char num[24], text[200], urc[120];
    CHECK(nt26_sms_send("10086", "查询话费 OK", 10000) == NT26_OK, "a Chinese text message goes out (UCS2)");
    CHECK(sim_net_sent_sms(0, num, sizeof num, text, sizeof text) == 1 && !strcmp(num, "10086") && !strcmp(text, "查询话费 OK"),
          "the network got it as written (%s: %s)", num, text);
    vtk_lock();
    bool delivered = sim_net_deliver_sms("95588", "您的验证码是 123456");
    vtk_unlock();
    int idx = -1;
    CHECK(delivered && hal_modem_urc(urc, sizeof urc, 1000) > 0 && sscanf(urc, "+CMTI: \"SM\",%d", &idx) == 1, "an incoming message is announced (%s)", urc);
    CHECK(nt26_sms_read(idx, num, sizeof num, text, sizeof text) == NT26_OK && !strcmp(num, "95588") && !strcmp(text, "您的验证码是 123456"),
          "and reads back (%s: %s)", num, text);
    CHECK(nt26_sms_delete(idx) == NT26_OK, "and is deleted");
    CHECK(nt26_dial("13800138000") == NT26_OK && hal_modem_urc(urc, sizeof urc, 1000) > 0 && strstr(urc, ",3,"), "a call goes out and rings (%s)", urc);
    CHECK(hal_modem_urc(urc, sizeof urc, 6000) > 0 && strstr(urc, ",0,0,0,0,"), "the far end picks up (%s)", urc);
    CHECK(nt26_hangup() == NT26_OK && hal_modem_urc(urc, sizeof urc, 1000) > 0 && !strcmp(urc, "NO CARRIER"), "hanging up ends it (%s)", urc);
    vtk_lock();
    bool rang = sim_net_incoming_call("13900139000");
    vtk_unlock();
    bool got_ring = false;
    for (int i = 0; i < 4 && hal_modem_urc(urc, sizeof urc, 1000) > 0;) {
        if (!strcmp(urc, "RING")) got_ring = true;
        if (got_ring && strstr(urc, "+CLIP: \"13900139000\"")) break;
        i++;
    }
    CHECK(rang && got_ring, "an incoming call rings with the caller's number");
    CHECK(nt26_answer() == NT26_OK && hal_modem_urc(urc, sizeof urc, 1000) > 0 && strstr(urc, "+CLCC: 1,1,0,"), "answering it (%s)", urc);
    vtk_lock();
    sim_net_remote_hangup();
    vtk_unlock();
    CHECK(hal_modem_urc(urc, sizeof urc, 1000) > 0 && !strcmp(urc, "NO CARRIER"), "the caller hangs up (%s)", urc);

    /* the environment says what is around; the system picks the link (hal_net_use) */
    hal_link_status_t ls;
    vtk_lock();
    sim_net_env(0, false, 0); /* no Wi-Fi around */
    vtk_unlock();
    CHECK(lte_up(), "losing Wi-Fi around does not touch the LTE link in use");
    hal_net_use(HAL_LINK_WIFI);
    hal_net_link_status(&ls);
    CHECK(ls.type == HAL_LINK_WIFI && !ls.up, "choosing Wi-Fi with none around: no link");
    vtk_lock();
    sim_net_env(0, true, -55);
    vtk_unlock();
    osal_task_delay_us(3000000);
    hal_net_link_status(&ls);
    CHECK(ls.type == HAL_LINK_WIFI && ls.up && ls.signal_dbm == -55, "Wi-Fi in range: joined after a few seconds (%d dBm)", ls.signal_dbm);
    vtk_lock();
    sim_net_env(1, false, 0); /* no cellular coverage */
    vtk_unlock();
    hal_net_use(HAL_LINK_LTE);
    hal_net_link_status(&ls);
    CHECK(ls.type == HAL_LINK_LTE && !ls.up, "choosing LTE without coverage: no link");
    vtk_lock();
    sim_net_env(1, true, -108);
    vtk_unlock();
    hal_net_link_status(&ls);
    CHECK(ls.up && ls.signal_dbm == -108, "coverage back: LTE up on the registered card (%d dBm)", ls.signal_dbm);

    CHECK(hal_modem_at("AT+ECRST", resp, sizeof resp, 1000) == HAL_AT_OK, "AT+ECRST is acknowledged");
    CHECK(hal_modem_at("AT", resp, sizeof resp, 300) == HAL_AT_TIMEOUT && !lte_up(), "the rebooting modem does not answer");
    osal_task_delay_us(4000000);
    CHECK(nt26_registered(&reg) == NT26_OK && reg && lte_up(), "after its reboot the modem re-registers by itself");
    ink_probe_mark("cell-done");
}

static void cam_cut_task(void *arg) {
    (void)arg;
    osal_task_delay_us(500000);
    metalio_board_set(0, MB_P0_CAM_SCR, 0);
}

/* INK_SELFTEST_BOARD=1: the Plus board's IO expander wiring (config.h @690a023) through our board
 * driver: the motor on P03, the shared touch + 4G reset on P12 and the shared supply on P14 acting
 * on the modem, then the official power-off pulse train on P13, which ends the run. */
/* Light sleep on the board: what each part is in while the chip sleeps, read from an event (every task
 * is suspended then); kept apart from the current figures, which are estimates. */
static struct {
    bool sampled;
    uint8_t cfg0, cfg1, pmic_pins;
    bool cam, p14, bt, modem, light;
    unsigned accel_moves;
} SLP;
static int g_power_presses, g_power_releases, g_vol_events;
static volatile unsigned g_mic_samples;
static volatile int g_cap_rc = 99;

static void board_mic(void *ctx, const int16_t *mic, const int16_t *ref, int samples, int lost, uint64_t t_us) {
    (void)ctx, (void)mic, (void)ref, (void)lost, (void)t_us;
    g_mic_samples += (unsigned)samples;
}

static void capture_late_task(void *arg) { /* starts recording while the sleep entry is under way */
    (void)arg;
    osal_task_delay_us(20000);
    g_cap_rc = hal_audio_capture_start(board_mic, NULL);
}

static void board_sink(const hal_input_event_t *e, void *ctx) {
    (void)ctx;
    if (e->type == HAL_IN_KEY && e->key.key == HAL_KEY_POWER) {
        if (e->key.pressed) g_power_presses++;
        else g_power_releases++;
    }
    if (e->type == HAL_IN_KEY && e->key.key == HAL_KEY_VOL_UP) g_vol_events++;
}

static void sample_asleep(void *arg) { /* ISR */
    (void)arg;
    sim_power_gui_t g;
    sim_power_gui_state(&g);
    SLP.sampled = sim_power_sleeping();
    SLP.cfg0 = sim_tca_config(0);
    SLP.cfg1 = sim_tca_config(1);
    SLP.pmic_pins = sim_tca_levels(0) & 0x07;
    SLP.cam = sim_board_cam_power();
    SLP.p14 = sim_board_shared_power();
    SLP.bt = sim_bt_audio_powered();
    SLP.modem = sim_net_modem_registered();
    SLP.light = g.light_cold || g.light_warm;
    unsigned before = sim_accel_motions();
    sim_accel_shake(); /* the accelerometer is quiet: no movement detected, nothing on GPIO2 */
    SLP.accel_moves = sim_accel_motions() - before;
    sim_input_key(HAL_KEY_VOL_UP, true); /* not a wake input while asleep: lost */
}

static void vol_up_release(void *arg) { /* ISR */
    (void)arg;
    sim_input_key(HAL_KEY_VOL_UP, false);
}

static void power_press(void *arg) { /* ISR */
    sim_input_key(HAL_KEY_POWER, arg != NULL);
}

static void expander_faults(void *arg) { /* ISR: every expander transfer fails while on */
    sim_i2c_set_faults(arg ? 1.0 : 0.0, arg ? "io-expander" : NULL);
}

static void vol_up(void *arg) { /* ISR */
    sim_input_key(HAL_KEY_VOL_UP, arg != NULL);
}

static void finger(void *arg) { /* ISR or locked */
    sim_input_touch(0, arg != NULL, 300, 300);
}


static void sleep_checks(void) {
    hal_input_set_sink(board_sink, NULL);
    hal_sleep_cfg_t sc = {.max_us = 1000000, .wake_mask = HAL_WAKE_POWER_KEY | HAL_WAKE_RTC};
    metalio_board_set(0, MB_P0_MOTOR, MB_P0_MOTOR);
    CHECK(hal_power_sleep(&sc) == 0, "a sleep with the motor still on is refused");
    metalio_board_set(0, MB_P0_MOTOR, 0);

    /* without answerable standby: P14 goes, and the modem and the Bluetooth chip with it */
    hal_light_set(400, 0);
    vtk_lock();
    vtk_event_after(SIM_MS(600 + POWER_DISCHARGE_MOTOR_PULSE_MS), sample_asleep, NULL);
    vtk_event_after(SIM_MS(700 + POWER_DISCHARGE_MOTOR_PULSE_MS), vol_up_release, NULL);
    vtk_unlock();
    uint64_t t0 = osal_now_us();
    uint32_t cause = hal_power_sleep(&sc);
    CHECK(cause == HAL_WAKE_TIMER && osal_now_us() - t0 >= 1000000, "no armed input: the slice timer ends the sleep (cause %u)", (unsigned)cause);
    CHECK(SLP.sampled && SLP.cfg0 == 0x00 && SLP.cfg1 == (MB_P1_POWER_KEY | MB_P1_RTC_INT),
          "asleep the expander pins are outputs but the wake inputs P17 and P16 (cfg %02x %02x)", SLP.cfg0, SLP.cfg1);
    CHECK(SLP.pmic_pins == 0 && !SLP.cam && !SLP.p14 && !SLP.bt && !SLP.modem,
          "asleep the PMIC pins are low, P05 and P14 are off, the modem and the Bluetooth chip unpowered");
    CHECK(!SLP.light && SLP.accel_moves == 0, "asleep the front light is off and the accelerometer detects nothing");
    CHECK(sim_tca_config(0) == 0xC0 && sim_tca_config(1) == 0xE3 && sim_board_cam_power() && sim_board_shared_power(),
          "after the wake the directions are as at boot and P05 and P14 are on again");
    int ax = 0, ay = 0, az = 0;
    CHECK(hal_accel_read(&ax, &ay, &az) && ay == 0 && ax == 1000, "the accelerometer is set up again (%d %d %d)", ax, ay, az);
    CHECK(g_power_presses == 0, "the key pressed while asleep was not reported");
    static int16_t tone[4800];
    CHECK(hal_audio_play_start() == 0 && sim_bt_audio_mode() == 1, "before the next audio the port sets the Bluetooth chip's mode again");
    CHECK(hal_audio_play_write(tone, 4800, 1000000) == 4800, "and the speaker queue moves");
    hal_audio_play_stop(true);
    osal_task_delay_us(4000000); /* the modem boots and registers again */

    /* answerable standby: P14 stays; a POWER press wakes the chip through P17, INT and GPIO2 */
    sc.keep_modem = true;
    vtk_lock();
    vtk_event_after(SIM_MS(400), power_press, (void *)1);
    vtk_event_after(SIM_MS(450), power_press, NULL);
    vtk_unlock();
    g_power_presses = g_power_releases = 0;
    t0 = osal_now_us();
    cause = hal_power_sleep(&sc);
    CHECK(cause == HAL_WAKE_POWER_KEY && osal_now_us() - t0 < 900000, "a short POWER press wakes the chip (cause %u)", (unsigned)cause);
    osal_task_delay_us(100000);
    CHECK(g_power_presses == 1 && g_power_releases == 1, "the wake key is reported once, pressed and released (%d %d)", g_power_presses, g_power_releases);
    CHECK(sim_net_modem_registered(), "the modem stayed registered");

    /* POWER held when the sleep starts: the armed input is already low, the chip wakes at once */
    vtk_lock();
    sim_input_key(HAL_KEY_POWER, true);
    vtk_unlock();
    osal_task_delay_us(100000);
    g_power_presses = g_power_releases = 0;
    t0 = osal_now_us();
    cause = hal_power_sleep(&sc);
    CHECK(cause == HAL_WAKE_POWER_KEY && osal_now_us() - t0 < 600000, "POWER held into the sleep wakes it at once (cause %u)", (unsigned)cause);
    vtk_lock();
    sim_input_key(HAL_KEY_POWER, false);
    vtk_unlock();
    osal_task_delay_us(100000);
    CHECK(g_power_presses == 0 && g_power_releases == 1, "its press was reported before the sleep and not again, the release after (%d %d)",
          g_power_presses, g_power_releases);

    /* a POWER wake while the expander's bus fails for 250 ms (audit AUD3-002): the restore is retried
     * until the bus answers, the wake key is reported pressed and released, keys work afterwards */
    vtk_lock();
    vtk_event_after(SIM_MS(400), power_press, (void *)1);
    vtk_event_after(SIM_MS(401), expander_faults, (void *)1);
    vtk_event_after(SIM_MS(450), power_press, NULL);
    vtk_event_after(SIM_MS(650), expander_faults, NULL);
    vtk_unlock();
    g_power_presses = g_power_releases = g_vol_events = 0;
    cause = hal_power_sleep(&sc);
    osal_task_delay_us(1500000);
    CHECK(cause == HAL_WAKE_POWER_KEY && sim_tca_config(0) == 0xC0 && sim_tca_config(1) == 0xE3,
          "after a wake whose expander bus failed for a while, its directions are restored (cause %u, cfg %02x %02x)", (unsigned)cause,
          sim_tca_config(0), sim_tca_config(1));
    CHECK(g_power_presses == 1 && g_power_releases == 1, "and the wake key is reported pressed and released (%d %d)", g_power_presses,
          g_power_releases);
    vtk_lock();
    vtk_event_after(SIM_MS(10), vol_up, (void *)1);
    vtk_event_after(SIM_MS(200), vol_up, NULL);
    vtk_unlock();
    osal_task_delay_us(500000);
    CHECK(g_vol_events == 2, "and the keys work again (%d VOL+ events)", g_vol_events);

    /* the same with a finger on the screen from just after the wake (audit AUD4-002): the touch
     * polling does not hold the restore back */
    vtk_lock();
    vtk_event_after(SIM_MS(400), power_press, (void *)1);
    vtk_event_after(SIM_MS(401), expander_faults, (void *)1);
    vtk_event_after(SIM_MS(450), power_press, NULL);
    vtk_event_after(SIM_MS(470), finger, (void *)1);
    vtk_event_after(SIM_MS(650), expander_faults, NULL);
    vtk_unlock();
    g_power_presses = g_power_releases = g_vol_events = 0;
    cause = hal_power_sleep(&sc);
    osal_task_delay_us(1500000);
    CHECK(cause == HAL_WAKE_POWER_KEY && sim_tca_config(0) == 0xC0 && sim_tca_config(1) == 0xE3 && g_power_releases == 1,
          "with a finger held on the screen the restore still gets through (cfg %02x %02x, %d releases)", sim_tca_config(0),
          sim_tca_config(1), g_power_releases);
    vtk_lock();
    vtk_event_after(SIM_MS(10), vol_up, (void *)1);
    vtk_event_after(SIM_MS(200), vol_up, NULL);
    vtk_unlock();
    osal_task_delay_us(500000);
    CHECK(g_vol_events == 2, "and the keys work with the finger still down (%d VOL+ events)", g_vol_events);
    vtk_lock();
    finger(NULL);
    vtk_unlock();
    osal_task_delay_us(100000);

    /* recording started while the sleep entry is under way (audit AUD3-004): it waits for the sleep
     * to finish instead of losing P14 under it, sets the Bluetooth chip's mode again, and records */
    hal_sleep_cfg_t sc2 = {.max_us = 1000000, .wake_mask = HAL_WAKE_POWER_KEY}; /* P14 is cut */
    osal_task_cfg_t ct = {.name = "cap_late", .fn = capture_late_task, .stack_bytes = 3072, .priority = OSAL_PRIO_NORMAL + 1, .core = -1};
    osal_task_create(&ct, NULL);
    g_mic_samples = 0;
    cause = hal_power_sleep(&sc2);
    osal_task_delay_us(2500000);
    CHECK(cause == HAL_WAKE_TIMER && g_cap_rc == 0 && sim_bt_audio_mode() == 1 && g_mic_samples > 0,
          "recording started during the sleep entry works after the wake (cause %u, rc %d, mode %d, %u samples)", (unsigned)cause,
          g_cap_rc, sim_bt_audio_mode(), g_mic_samples);
    hal_audio_capture_stop();
    osal_task_delay_us(4000000); /* the modem boots and registers again */

    /* a wake whose supply restore has to wait for the expander's bus (P14 cut asleep; audit AUD4-003):
     * until it gets through, recording refuses to start instead of starting without the Bluetooth
     * chip's clock; afterwards it records */
    vtk_lock();
    vtk_event_after(SIM_MS(300 + POWER_DISCHARGE_MOTOR_PULSE_MS), power_press, (void *)1);
    vtk_event_after(SIM_MS(301 + POWER_DISCHARGE_MOTOR_PULSE_MS), expander_faults, (void *)1);
    vtk_event_after(SIM_MS(350 + POWER_DISCHARGE_MOTOR_PULSE_MS), power_press, NULL);
    vtk_event_after(SIM_MS(900 + POWER_DISCHARGE_MOTOR_PULSE_MS), expander_faults, NULL);
    vtk_unlock();
    cause = hal_power_sleep(&sc2);
    int rc_early = hal_audio_capture_start(board_mic, NULL);
    if (rc_early == 0) hal_audio_capture_stop();
    CHECK(cause == HAL_WAKE_POWER_KEY && rc_early == -1 && !sim_audio_busy(),
          "right after a wake whose supply is not back yet, recording refuses to start (cause %u, rc %d)", (unsigned)cause, rc_early);
    osal_task_delay_us(2000000);
    g_mic_samples = 0;
    int rc_late = hal_audio_capture_start(board_mic, NULL);
    osal_task_delay_us(500000);
    CHECK(rc_late == 0 && sim_board_shared_power() && sim_bt_audio_mode() == 1 && g_mic_samples > 0,
          "once the restore gets through it records (rc %d, mode %d, %u samples)", rc_late, sim_bt_audio_mode(), g_mic_samples);
    hal_audio_capture_stop();
    osal_task_delay_us(4000000); /* the modem boots and registers again */

    /* P16 held at its active level by something else: it cannot signal, so the sleep does not arm it and
     * the timer ends the slice instead of an endless wake */
    vtk_lock();
    sim_tca_input(1, MB_P1_RTC_INT, (MB_RTC_INT_ACTIVE_HIGH ? MB_P1_RTC_INT : 0));
    vtk_event_after(SIM_MS(600), sample_asleep, NULL);
    vtk_event_after(SIM_MS(700), vol_up_release, NULL);
    vtk_unlock();
    SLP.sampled = false;
    cause = hal_power_sleep(&sc);
    CHECK(cause == HAL_WAKE_TIMER && SLP.sampled && SLP.cfg1 == MB_P1_POWER_KEY,
          "with P16 held active the RTC is not armed and the slice timer ends the sleep (cause %u, cfg %02x)", (unsigned)cause, SLP.cfg1);
    vtk_lock();
    sim_tca_input(1, MB_P1_RTC_INT, (MB_RTC_INT_ACTIVE_HIGH ? 0 : MB_P1_RTC_INT));
    vtk_unlock();
    hal_input_set_sink(NULL, NULL);
}

/* A panel update whose supply goes away part way fails, and the panel does not take it; the next
 * update powers the panel up again (audit AUD3-003). */
static volatile bool g_upd_done, g_upd_ok;

static void upd_done(void *ctx, const hal_display_result_t *r) {
    (void)ctx;
    g_upd_ok = r->ok;
    g_upd_done = true;
}

static void cut_panel_task(void *arg) {
    (void)arg;
    for (int i = 0; i < 2000 && !sim_display_playing(); i++) osal_task_delay_us(1000);
    metalio_board_set(0, EPD_PWR_VCOM_CTRL | EPD_PWR_PWRUP | EPD_PWR_WAKEUP, 0); /* PMIC pins low mid-waveform */
}

/* a blip that is back before the waveform ends (audit AUD4-001): PWRUP (the rails restart; VCOM
 * enabled again by hand, as an outside writer could), VCOM_CTRL alone (PWR_GOOD does not move), or
 * the screen's power P05 */
static const uint8_t BLIP_PIN[3] = {EPD_PWR_PWRUP, EPD_PWR_VCOM_CTRL, MB_P0_CAM_SCR};
static const char *const BLIP_NAME[3] = {"supply (PWRUP)", "VCOM (VCOM_CTRL)", "screen power (P05)"};

static void blip_task(void *arg) {
    const int kind = (int)(intptr_t)arg;
    for (int i = 0; i < 2000 && !sim_display_playing(); i++) osal_task_delay_us(1000);
    osal_task_delay_us(5000); /* a phase or so in */
    metalio_board_set(0, BLIP_PIN[kind], 0);
    osal_task_delay_us(20000);
    metalio_board_set(0, BLIP_PIN[kind], BLIP_PIN[kind]);
    if (kind == 0) {
        hal_i2c_dev_t *pmic = hal_i2c_device(0, TPS65185_ADDR, "pmic");
        tps65185_write(pmic, TPS_REG_ENABLE, 0);
        tps65185_write(pmic, TPS_REG_ENABLE, TPS_ENABLE_ALL_RAILS);
    }
}

/* what the panel shows, as one sees it, averaged over a native area (-1: no image) */
static double panel_mean(const hal_rect_t *a) {
    static uint8_t vis[1216 * 1216];
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    int lw = 0, lh = 0;
    if (!sim_display_visible(vis, sizeof vis, &lw, &lh)) return -1;
    double sum = 0;
    for (int y = a->y; y < a->y + a->h; y++)
        for (int x = a->x; x < a->x + a->w; x++) {
            int lx, ly;
            ink_native_to_logical(pi.width, pi.height, pi.rotation_deg, x, y, &lx, &ly);
            sum += vis[(size_t)ly * (size_t)lw + (size_t)lx];
        }
    return sum / ((double)a->w * a->h);
}

static volatile bool g_lost_done, g_lost_ok;
static volatile double g_lost_mean = -1;
static hal_rect_t g_lost_area;

static double panel_mean(const hal_rect_t *a);

static void lost_area_done(void *ctx, const hal_display_result_t *r) { /* display task */
    (void)ctx;
    g_lost_ok = r->ok;
    g_lost_mean = panel_mean(&g_lost_area); /* what the panel shows there when the update says it is done */
    g_lost_done = true;
}

static void ignore_done(void *ctx, const hal_display_result_t *r) {
    (void)ctx;
    (void)r;
}

static bool update_wf_and_wait(const uint8_t *px, const hal_rect_t *area, hal_waveform_t wf, int max_ms) {
    g_upd_done = false;
    hal_display_update(area, px, area->w, wf, 0, upd_done, NULL);
    for (int i = 0; i < max_ms / 10 && !g_upd_done; i++) osal_task_delay_us(10000);
    return g_upd_done;
}

static bool update_and_wait_for(const uint8_t *px, const hal_rect_t *area, int max_ms) {
    return update_wf_and_wait(px, area, HAL_WF_GL16, max_ms);
}

static bool update_and_wait(const uint8_t *px, const hal_rect_t *area) { return update_and_wait_for(px, area, 5000); }

static void display_cut_checks(void) {
    static uint8_t px[64 * 64];
    hal_rect_t area = {0, 0, 64, 64};
    memset(px, 0, sizeof px);
    uint32_t v0 = sim_display_version();
    osal_task_cfg_t c = {.name = "panel_cut", .fn = cut_panel_task, .stack_bytes = 2048, .priority = OSAL_PRIO_NORMAL + 2, .core = -1};
    osal_task_create(&c, NULL);
    bool done = update_and_wait(px, &area);
    CHECK(done && !g_upd_ok, "an update whose panel supply goes away part way fails (ok %d)", g_upd_ok);
    /* EPDiy's model shows the image at the end of the waveform, PQ-BN phase by phase: the phases
     * driven before the cut did reach the panel there */
    if (g_profile.display_driver != 1)
        CHECK(sim_display_version() == v0, "and the panel does not show it");
    CHECK(update_and_wait(px, &area) && g_upd_ok && sim_display_version() != v0, "the next update powers the panel up again and works");
    for (int b = 0; b < 3; b++) {
        memset(px, b & 1 ? 0x00 : 0xFF, sizeof px); /* new content: both pipelines drive their full phases */
        osal_task_cfg_t cb = {.name = "panel_blip", .fn = blip_task, .arg = (void *)(intptr_t)b, .stack_bytes = 2048,
                              .priority = OSAL_PRIO_NORMAL + 2, .core = -1};
        osal_task_create(&cb, NULL);
        bool ok = update_and_wait(px, &area);
        CHECK(ok && !g_upd_ok, "an update whose %s blips part way fails, although it is back before the end", BLIP_NAME[b]);
    }
    CHECK(update_and_wait(px, &area) && g_upd_ok, "after a blip the next update powers up from the start and works");
    /* the screen's power left off (audit AUD5-001): updates keep failing until it is back; the
     * power-up does not take P05 over (upstream's camera path leaves the shared outputs alone too) */
    metalio_board_set(0, MB_P0_CAM_SCR, 0);
    uint64_t t_off = osal_now_us();
    bool off1 = update_and_wait(px, &area) && !g_upd_ok;
    bool off2 = update_and_wait(px, &area) && !g_upd_ok;
    uint64_t took_off = osal_now_us() - t_off; /* no wait for P05 when no wake is restoring it (AUD7-004) */
    CHECK(off1 && off2 && took_off < 250000, "with the screen's power P05 left off, update after update fails, at once (%llu ms for two)",
          (unsigned long long)(took_off / 1000));
    metalio_board_set(0, MB_P0_CAM_SCR, MB_P0_CAM_SCR);
    CHECK(update_and_wait(px, &area) && g_upd_ok, "and they work again once P05 is back");
    /* after a drive that lost its supply the panel's state is unknown: the next update redraws the
     * whole panel, so the same image sent again is what it shows (audit AUD5-001: PQ-BN's controller
     * had counted pushes the panel never got) */
    memset(px, 0xFF, sizeof px);
    CHECK(update_and_wait(px, &area) && g_upd_ok && panel_mean(&area) > 230, "a white area to start from");
    memset(px, 0x00, sizeof px);
    osal_task_cfg_t cb = {.name = "panel_blip", .fn = blip_task, .arg = (void *)(intptr_t)0, .stack_bytes = 2048,
                          .priority = OSAL_PRIO_NORMAL + 2, .core = -1};
    osal_task_create(&cb, NULL);
    bool lost = update_and_wait(px, &area) && !g_upd_ok;
    uint32_t redraws = (uint32_t)sim_metric_value("display.redraws_after_supply_loss");
    bool again = update_and_wait_for(px, &area, 20000) && g_upd_ok; /* PQ-BN: a full clean first, ~10 s */
    double mean = panel_mean(&area);
    CHECK(lost && again && mean < 25 && (uint32_t)sim_metric_value("display.redraws_after_supply_loss") == redraws + 1,
          "after a supply loss the same image sent again is on the panel, redrawn in full (mean %.0f)", mean);

    /* a full-screen update that loses its supply, then a full GC16 (the system's own recovery): that GC16
     * is the redraw, the next small update is not held up by a second full one (audit AUD6-002) */
    hal_panel_info_t pi;
    hal_display_get_info(&pi);
    hal_rect_t all = {0, 0, pi.width, pi.height};
    uint8_t *full = osal_malloc((size_t)pi.width * pi.height, OSAL_MEM_EXTERNAL);
    if (full) memset(full, 0x80, (size_t)pi.width * pi.height);
    osal_task_create(&cb, NULL);
    bool lost2 = full && update_wf_and_wait(full, &all, HAL_WF_GL16, 20000) && !g_upd_ok;
    if (full) memset(full, 0xFF, (size_t)pi.width * pi.height);
    bool full_ok = full && update_wf_and_wait(full, &all, HAL_WF_GC16, 20000) && g_upd_ok;
    memset(px, 0x00, sizeof px);
    uint64_t t_small = osal_now_us();
    bool small = update_and_wait(px, &area) && g_upd_ok;
    uint64_t took = osal_now_us() - t_small;
    CHECK(lost2 && full_ok && small && took < 2000000, "after a supply loss and a full GC16, a small update takes its usual time (%llu ms)",
          (unsigned long long)(took / 1000));
    osal_free(full);

    /* a supply loss on one area, then at once a GC16 of another area and an update of the lost one:
     * the update of the lost area is done only when the panel shows it there (audit AUD7-001) */
    static uint8_t other[64 * 64];
    const hal_rect_t right = {128, 0, 64, 64};
    memset(px, 0xFF, sizeof px);
    bool base = update_and_wait(px, &area) && g_upd_ok;
    memset(px, 0x00, sizeof px);
    osal_task_create(&cb, NULL);
    bool lost3 = update_and_wait(px, &area) && !g_upd_ok;
    memset(other, 0xFF, sizeof other);
    g_lost_area = area;
    g_lost_done = false;
    hal_display_update(&right, other, 64, HAL_WF_GC16, 0, ignore_done, NULL);
    hal_display_update(&area, px, 64, HAL_WF_GL16, 0, lost_area_done, NULL);
    for (int i = 0; i < 2000 && !g_lost_done; i++) osal_task_delay_us(10000);
    CHECK(base && lost3 && g_lost_done && g_lost_ok && g_lost_mean >= 0 && g_lost_mean < 25,
          "with a GC16 elsewhere, the lost area's update is done only once the panel shows it (mean %.0f at its callback)", g_lost_mean);
    for (int i = 0; i < 2000 && hal_display_pending() > 0; i++) osal_task_delay_us(10000);
}

static void board_task(void *arg) {
    (void)arg;
    CHECK(metalio_board_ready(), "the board's expander is set up at boot");
    uint8_t p1 = 0;
    CHECK(metalio_board_read(1, &p1) == HAL_I2C_OK && (p1 & MB_P1_KEYS) == MB_P1_KEYS && (p1 & MB_P1_SHARED_RST) && (p1 & MB_P1_SHARED_PWR),
          "port 1 idles with keys released and the shared reset and supply high (0x%02x)", p1);
    hal_haptic_pulse(50);
    osal_task_delay_us(20000);
    CHECK(sim_board_motor_on(), "a haptic pulse switches the motor on (P03)");
    osal_task_delay_us(80000);
    CHECK(!sim_board_motor_on(), "and off again after the pulse");

    /* RTC (PCF8563): the boot time, setting it, an alarm that drives P16 to its active level and wakes the chip */
    int64_t now_s = 0;
    CHECK(hal_rtc_get(&now_s) && now_s >= g_sim.wall_start_us / 1000000 && now_s < g_sim.wall_start_us / 1000000 + 60,
          "the RTC keeps the simulated wall time (%lld)", (long long)now_s);
    const int64_t t_set = 1798761598; /* 2026-12-31 23:59:58 UTC */
    CHECK(hal_rtc_set(t_set) && hal_rtc_get(&now_s) && now_s >= t_set && now_s <= t_set + 1, "the RTC takes a new time (%lld)", (long long)now_s);
    CHECK(hal_rtc_set_alarm(t_set + 2), "an alarm for the next minute is set");
    hal_sleep_cfg_t sc = {.max_us = 10000000, .wake_mask = HAL_WAKE_RTC, .keep_modem = true};
    uint32_t cause = hal_power_sleep(&sc);
    CHECK(cause == HAL_WAKE_RTC && sim_rtc_alarms() == 1, "the RTC alarm wakes the chip from light sleep (cause %u)", (unsigned)cause);
    CHECK(metalio_board_read(1, &p1) == HAL_I2C_OK && MB_RTC_PENDING(p1), "the alarm holds TCA9555 P16 at its active level (0x%02x)", p1);
    hal_rtc_clear_alarm();
    CHECK(metalio_board_read(1, &p1) == HAL_I2C_OK && !MB_RTC_PENDING(p1), "clearing the alarm releases P16 (0x%02x)", p1);

    /* accelerometer (SC7A20H): gravity in portrait, a turn, the movement interrupt on GPIO2 */
    int ax = 0, ay = 0, az = 0;
    CHECK(hal_accel_read(&ax, &ay, &az) && ax == 0 && ay == 1000 && az == 0, "upright in portrait: gravity on +y (%d %d %d)", ax, ay, az);
    uint32_t moves = hal_accel_motion_count();
    vtk_lock();
    sim_accel_orient("landscape_left");
    vtk_unlock();
    osal_task_delay_us(50000);
    CHECK(hal_accel_read(&ax, &ay, &az) && ax == 1000 && ay == 0, "turned left: gravity on +x (%d %d %d)", ax, ay, az);
    CHECK(hal_accel_motion_count() == moves + 1 && !sim_accel_int_low(), "the turn raised INT1 on GPIO2 and the driver task cleared it");
    vtk_lock();
    sim_input_key(HAL_KEY_VOL_UP, true); /* with INT1 serviced, a key on the shared line still gets through */
    vtk_unlock();
    osal_task_delay_us(60000);
    CHECK(metalio_board_read(1, &p1) == HAL_I2C_OK && !(p1 & MB_P1_VOL_UP), "a key press after the movement reaches the expander");
    vtk_lock();
    sim_input_key(HAL_KEY_VOL_UP, false);
    vtk_unlock();
    osal_task_delay_us(60000);

    /* a failed read of the accelerometer's latch leaves INT1 holding GPIO2 low, and GPIO2 interrupts
     * on the falling edge: the driver task looks again while the line stays low, so once the bus
     * answers the latch is read and the keys on the shared line work again */
    hal_input_set_sink(board_sink, NULL);
    sim_i2c_set_faults(1.0, "accel");
    vtk_lock();
    sim_accel_orient("portrait");
    vtk_unlock();
    osal_task_delay_us(30000);
    CHECK(sim_accel_int_low() && sim_gpio2_low(), "a turn while the accelerometer's bus fails: its latch stays set, GPIO2 low");
    sim_i2c_set_faults(0, NULL);
    g_vol_events = 0;
    vtk_lock();
    vtk_event_after(SIM_MS(10), vol_up, (void *)1);
    vtk_event_after(SIM_MS(200), vol_up, NULL);
    vtk_unlock();
    osal_task_delay_us(1500000);
    CHECK(!sim_accel_int_low() && g_vol_events == 2, "once the bus answers the latch is read and a key press gets through (%d VOL+ events)",
          g_vol_events);
    hal_input_set_sink(NULL, NULL);
    vtk_lock();
    sim_accel_orient("landscape_left"); /* back as before, for the sleep checks */
    vtk_unlock();
    osal_task_delay_us(50000);

    /* camera (GC2145): a VGA RGB565 picture after the warm-up frames; no answer without P05 */
    hal_camera_info_t ci;
    CHECK(hal_camera_info(&ci) && ci.width == 640 && ci.height == 480 && ci.frame_bytes == 640u * 480u * 2u, "the camera is a 640x480 RGB565 sensor");
    uint8_t *pic = osal_malloc(ci.frame_bytes, OSAL_MEM_DMA | OSAL_MEM_EXTERNAL);
    uint64_t t_cam = osal_now_us();
    int got = pic ? hal_camera_capture(pic, ci.frame_bytes, 2000) : -100;
    uint64_t took_cam = osal_now_us() - t_cam;
    CHECK(got == (int)ci.frame_bytes && pic[0] == 0xFF && pic[1] == 0xFF, "a picture arrives (test pattern: white bar first; %d bytes)", got);
    CHECK(took_cam >= 650000 && took_cam < 1200000, "taking it costs the vendor sequence and the warm-up frames (%llu ms)", (unsigned long long)(took_cam / 1000));
    metalio_board_set(0, MB_P0_CAM_SCR, 0);
    CHECK(pic && hal_camera_capture(pic, ci.frame_bytes, 2000) < 0, "without its supply (P05 low) the sensor does not answer");
    metalio_board_set(0, MB_P0_CAM_SCR, MB_P0_CAM_SCR);
    osal_task_cfg_t cut = {.name = "cam_cut", .fn = cam_cut_task, .stack_bytes = 2048, .priority = OSAL_PRIO_NORMAL + 1, .core = -1};
    osal_task_create(&cut, NULL); /* cuts P05 in the middle of the next capture */
    CHECK(pic && hal_camera_capture(pic, ci.frame_bytes, 3000) == -5, "a capture that loses power half way fails instead of returning a frame");
    metalio_board_set(0, MB_P0_CAM_SCR, MB_P0_CAM_SCR);
    uint64_t t_short = osal_now_us();
    CHECK(pic && hal_camera_capture(pic, ci.frame_bytes, 100) == -4 && osal_now_us() - t_short < 150000,
          "the timeout covers the whole capture, its set-up too (100 ms: gave up after %llu ms)",
          (unsigned long long)((osal_now_us() - t_short) / 1000));
    uint8_t *internal = osal_malloc(64, OSAL_MEM_INTERNAL);
    CHECK(internal && hal_camera_capture(internal, ci.frame_bytes, 3000) == -1, "a buffer the camera DMA cannot fill is refused");
    if (internal) osal_free(internal);
    if (pic) osal_free(pic);

    /* Bluetooth audio chip (BH1098G on UART1): conversation mode set at boot, version answer */
    hal_uart_t *bt = hal_uart_open(BH1098G_UART_PORT, BH1098G_BAUD);
    char ver[16] = "";
    CHECK(bt && sim_bt_audio_mode() == 1, "the Bluetooth audio chip is in conversation mode after boot (mode %d)", sim_bt_audio_mode());
    CHECK(bt && bh1098g_version(bt, ver, sizeof ver) == 0 && !strcmp(ver, "V1.03"), "it answers its version (%s)", ver);

    char resp[64];
    bool reg = false;
    CHECK(nt26_registered(&reg) == NT26_OK && reg, "the modem is registered");
    metalio_board_set(1, MB_P1_SHARED_RST, 0); /* resetting the touch controller resets the modem too */
    CHECK(hal_modem_at("AT", resp, sizeof resp, 500) == HAL_AT_TIMEOUT && !lte_up(), "P12 low holds the 4G modem in reset");
    uint8_t chip = 0, reg_id = 0xA3;
    CHECK(hal_i2c_write_read(hal_i2c_device(0, FT6336U_ADDR, "touch"), &reg_id, 1, &chip, 1) == HAL_I2C_NACK,
          "and the touch controller with it");
    metalio_board_set(1, MB_P1_SHARED_RST, MB_P1_SHARED_RST);
    osal_task_delay_us(4000000);
    CHECK(nt26_registered(&reg) == NT26_OK && reg && lte_up(), "after P12 goes high the modem boots and registers");
    metalio_board_set(1, MB_P1_SHARED_PWR, 0);
    CHECK(hal_modem_at("AT", resp, sizeof resp, 500) == HAL_AT_TIMEOUT && !lte_up(), "P14 off powers the modem down");
    /* the BH1098G clocks the I2S: without it (P14 off) audio does not start, it would get no clock */
    static int16_t tone[4800];
    CHECK(hal_audio_play_start() == -1 && hal_audio_capture_start(board_mic, NULL) == -1 && !sim_audio_busy(),
          "with the Bluetooth chip unpowered, speaker and microphone refuse to start");
    metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    CHECK(sim_bt_audio_mode() == 0, "the P14 cycle also reset the Bluetooth audio chip (no mode)");
    osal_task_delay_us(4000000);
    CHECK(nt26_registered(&reg) == NT26_OK && reg, "P14 back on: the modem registers again");
    CHECK(hal_audio_play_start() == 0 && sim_bt_audio_mode() == 1, "the audio port sets conversation mode again after the power cycle");
    /* the chip's clock going away under a running stream (the amplifier off first, as the wiring
     * needs): the queue stops moving, the writer times out, the drain is bounded */
    metalio_board_set(0, MB_P0_AMP, 0);
    metalio_board_set(1, MB_P1_SHARED_PWR, 0);
    int took = hal_audio_play_write(tone, 4800, 300000);
    CHECK(took < 4800, "no clock: the queue fills and the writer times out (%d of 4800 samples taken)", took);
    hal_audio_play_stop(true); /* must not wait forever */
    metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    osal_task_delay_us(4000000); /* the modem registers again */
    display_cut_checks();
    sleep_checks();
    ink_probe_mark("board-shutdown");
    hal_power_shutdown(); /* 15 pulses on P13: the run ends at the last one */
    CHECK(0, "hal_power_shutdown returned while the device should be off");
}

/* INK_SELFTEST_AGE=1 with --flash-image: leave /user 50-60 % full and fragmented (written to 80 %,
 * every third file deleted, some rewritten), so scenarios run on an aged filesystem where writes
 * trigger block reclaim and metadata compaction. */
static void age_task(void *arg) {
    (void)arg;
    hal_fs_volume_info_t vi;
    if (hal_fs_volume_info("/user", &vi) != 0) return;
    static uint8_t buf[6000];
    uint32_t x = 12345;
    char path[64];
    int n = 0;
    for (; n < 400; n++) {
        hal_fs_volume_info("/user", &vi);
        if (vi.free_bytes < vi.total_bytes * 20 / 100) break;
        x = x * 1103515245u + 12345u;
        size_t len = 200 + (x >> 8) % (sizeof(buf) - 200);
        for (size_t i = 0; i < len; i++) buf[i] = (uint8_t)(x >> (i % 24));
        snprintf(path, sizeof path, "/user/aged/f%03d.bin", n);
        if (n == 0) hal_fs_mkdir("/user/aged");
        hal_file_t *f = hal_fs_open(path, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
        if (!f) break;
        hal_fs_write(f, buf, len);
        hal_fs_close(f);
    }
    for (int i = 0; i < n; i += 3) { /* punch holes */
        snprintf(path, sizeof path, "/user/aged/f%03d.bin", i);
        hal_fs_remove(path);
    }
    for (int i = 1; i < n; i += 9) { /* rewrite some survivors */
        snprintf(path, sizeof path, "/user/aged/f%03d.bin", i);
        hal_file_t *f = hal_fs_open(path, HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
        if (f) {
            hal_fs_write(f, buf, 3000);
            hal_fs_close(f);
        }
    }
    hal_fs_volume_info("/user", &vi);
    sim_note("aged /user: %d files written, a third removed; %llu of %llu KiB used", n,
             (unsigned long long)((vi.total_bytes - vi.free_bytes) / 1024), (unsigned long long)(vi.total_bytes / 1024));
    ink_probe_mark("aging-done");
}

/* INK_SELFTEST_EXPANDER_DEAD with --i2c-faults 1.0:io-expander: the expander never set up at boot.
 * Keys must not reach the system around it; once the bus answers, the next edge sets it up and keys
 * work (audit AUD2-012). */
static int g_dead_keys;

static void dead_sink(const hal_input_event_t *e, void *ctx) {
    (void)ctx;
    if (e->type == HAL_IN_KEY) g_dead_keys++;
}

static void power_key_event(void *arg) { sim_input_key(HAL_KEY_POWER, arg != NULL); } /* ISR */

static void expander_dead_task(void *arg) {
    (void)arg;
    hal_input_set_sink(dead_sink, NULL);
    CHECK(!metalio_board_ready(), "the expander did not set up with its bus failing");
    vtk_lock();
    vtk_event_after(SIM_MS(20), power_key_event, (void *)1);
    vtk_event_after(SIM_MS(200), power_key_event, NULL);
    vtk_unlock();
    osal_task_delay_us(500000);
    CHECK(g_dead_keys == 0, "no key event without a working expander (%d)", g_dead_keys);
    sim_i2c_set_faults(0, NULL);
    vtk_lock();
    vtk_event_after(SIM_MS(20), power_key_event, (void *)1);
    vtk_event_after(SIM_MS(300), power_key_event, NULL);
    vtk_unlock();
    osal_task_delay_us(800000);
    CHECK(metalio_board_ready() && g_dead_keys == 2, "once the bus answers the expander is set up and keys work (%d events)", g_dead_keys);
    /* the Bluetooth audio chip, which the boot could not set up either (audit AUD4-004): the first
     * recording sets its mode and gets samples */
    g_mic_samples = 0;
    int rc = hal_audio_capture_start(board_mic, NULL);
    osal_task_delay_us(500000);
    CHECK(rc == 0 && sim_bt_audio_mode() == 1 && g_mic_samples > 0, "and the first recording sets the Bluetooth chip up and records (rc %d, mode %d, %u samples)",
          rc, sim_bt_audio_mode(), g_mic_samples);
    hal_audio_capture_stop();
    ink_probe_mark("expander-dead-done");
}

void ink_main(void) {
    if (getenv("INK_SELFTEST_EXPANDER_DEAD")) {
        osal_task_cfg_t cfg = {.name = "exp_dead", .fn = expander_dead_task, .stack_bytes = 4096, .priority = OSAL_PRIO_NORMAL, .core = -1};
        osal_task_create(&cfg, NULL);
        return;
    }
    if (getenv("INK_SELFTEST_BOARD")) {
        osal_task_cfg_t cfg = {.name = "board", .fn = board_task, .stack_bytes = 6144, .priority = OSAL_PRIO_NORMAL, .core = -1};
        osal_task_create(&cfg, NULL);
        return;
    }
    if (getenv("INK_SELFTEST_CELL")) {
        osal_task_cfg_t cfg = {.name = "cell", .fn = cell_task, .stack_bytes = 4096, .priority = OSAL_PRIO_NORMAL, .core = -1};
        osal_task_create(&cfg, NULL);
        return;
    }
    if (getenv("INK_SELFTEST_AGE")) {
        osal_task_cfg_t cfg = {.name = "age", .fn = age_task, .stack_bytes = 4096, .priority = OSAL_PRIO_NORMAL, .core = -1};
        osal_task_create(&cfg, NULL);
        return;
    }
    const char *death = getenv("INK_SELFTEST_DEATH");
    if (death && !strcmp(death, "psram_flash")) {
        osal_task_cfg_t bad = {.name = "psram_reader", .fn = bad_psram_task, .stack_bytes = 3072,
                               .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
        osal_task_create(&bad, NULL);
        return;
    }
    /* the checks need more stack than the product's main task gets; internal, because they write flash */
    osal_task_cfg_t cfg = {.name = "selftest", .fn = selftest_task, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create the self-test task");
}
