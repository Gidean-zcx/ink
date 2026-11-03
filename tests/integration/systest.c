/*
 * System service checks on the simulated device: what a service promises its callers, beyond what
 * the scenarios exercise. Results are the probe counters "test.pass" / "test.fail", as in
 * tests/simulator/selftest.c; the budget file requires zero failures.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/audio.h"
#include "ink/sys/display.h"
#include "ink/sys/power.h"
#include "ink/sys/storage.h"
#include "metalio_plus/metalio_board.h"
#include "sim.h"

#define CHECK(cond, ...)                                                 \
    do {                                                                 \
        if (cond) {                                                      \
            ink_probe_count("test.pass");                                \
        } else {                                                         \
            char _m[300];                                                \
            snprintf(_m, sizeof _m, __VA_ARGS__);                        \
            sim_warn("CHECK FAILED (%s:%d) %s", __FILE__, __LINE__, _m); \
            ink_probe_count("test.fail");                                \
        }                                                                \
    } while (0)

static void drain_storage(void) {
    for (int i = 0; i < 500 && ink_storage_pending() > 0; i++) osal_task_delay_us(20000);
}

/* The read cache never returns an older version than the last write (audit AUD2-002: a small file
 * replaced by one too large for the cache kept reading as the old small file). */
static void storage_checks(void) {
    static char big[1500];
    char rb[32];
    const char *path = "/user/apps/systest-cache";
    memset(big, 'N', sizeof big);
    CHECK(ink_storage_write_atomic(path, "OLD", 3) == 0, "a small write is queued");
    drain_storage();
    memset(rb, 0, sizeof rb);
    CHECK(ink_storage_read(path, rb, sizeof rb) == 3 && !memcmp(rb, "OLD", 3), "it reads back (%.3s)", rb);
    CHECK(ink_storage_write_atomic(path, big, sizeof big) == 0, "a write too large for the cache is queued");
    drain_storage();
    memset(rb, 0, sizeof rb);
    int n = ink_storage_read(path, rb, sizeof rb);
    CHECK(n == (int)sizeof rb && rb[0] == 'N', "after it is written a read returns the new content, not the cached old one (%d, %c)", n,
          rb[0] ? rb[0] : '0');
    CHECK(ink_storage_write_atomic(path, "NEW", 3) == 0, "a small write again");
    memset(rb, 0, sizeof rb);
    CHECK(ink_storage_read(path, rb, sizeof rb) == 3 && !memcmp(rb, "NEW", 3), "while queued, a read sees it (%.3s)", rb);
    drain_storage();
    memset(rb, 0, sizeof rb);
    CHECK(ink_storage_read(path, rb, sizeof rb) == 3 && !memcmp(rb, "NEW", 3), "and after it is written (%.3s)", rb);
    CHECK(ink_storage_failed() == 0, "no write failed");
}

/* Playback while the speaker cannot start (the Bluetooth audio chip, which clocks the I2S, unpowered:
 * audit AUD5-002): the stream's frames wait instead of being taken and dropped; they all play once it
 * can start; a stream that cannot start for long fails and its owner is told. */
static volatile int g_failed_id;

static void on_failed(void *ctx, int id) {
    (void)ctx;
    g_failed_id = id;
}

static int push_frames(int id, int n) {
    int ok = 0;
    for (int i = 0; i < n; i++) {
        const uint8_t f[8] = {0xA5, 0x00, 0x10, 0x30, 0x11, 0, 0, 0}; /* the stand-in codec: a 440 Hz tone */
        if (ink_audio_stream_push(id, f, sizeof f) == 0) ok++;
    }
    return ok;
}

static bool playing_for(double since, double ms) { /* waits up to 4 s until `ms` more were played */
    for (int i = 0; i < 400 && sim_audio_played_ms() - since < ms; i++) osal_task_delay_us(10000);
    return sim_audio_played_ms() - since >= ms;
}

static void audio_checks(void) {
    ink_audio_on_stream_failed(on_failed, NULL);
    metalio_board_set(1, MB_P1_SHARED_PWR, 0);
    double played0 = sim_audio_played_ms();
    int id = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, 0, NULL);
    CHECK(id > 0 && push_frames(id, 10) == 10, "a stream takes 10 frames while the speaker cannot start");
    ink_audio_stream_finish(id);
    osal_task_delay_us(1500000);
    CHECK(sim_audio_played_ms() == played0 && g_failed_id == 0, "they wait: nothing played, the stream has not failed");
    metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    osal_task_delay_us(4000000);
    double played = sim_audio_played_ms() - played0;
    CHECK(played >= 590 && g_failed_id == 0, "once the speaker can start all of them play (%.0f ms of 600)", played);
    metalio_board_set(1, MB_P1_SHARED_PWR, 0);
    id = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, 0, NULL);
    push_frames(id, 5);
    osal_task_delay_us(10000000);
    CHECK(g_failed_id == id && push_frames(id, 1) == 0, "a stream whose speaker stays off fails after a while, its owner is told (%d, %d)",
          g_failed_id, id);
    metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    osal_task_delay_us(2000000);

    /* the chip's supply going away under a stream that plays, and coming back (audit AUD6-001): the
     * output stalls, is opened again (the chip set up again) and the rest plays; gone for good, the
     * stream fails and nothing keeps the device awake (amplifier off first, as the wiring needs) */
    g_failed_id = 0;
    played0 = sim_audio_played_ms();
    id = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, 0, NULL);
    push_frames(id, 50); /* 3 s */
    ink_audio_stream_finish(id);
    bool was_playing = playing_for(played0, 200);
    double stalls0 = sim_metric_value("audio.output_stalls");
    metalio_board_set(0, MB_P0_AMP, 0);
    metalio_board_set(1, MB_P1_SHARED_PWR, 0);
    osal_task_delay_us(1000000);
    metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    osal_task_delay_us(8000000);
    played = sim_audio_played_ms() - played0;
    CHECK(was_playing && sim_metric_value("audio.output_stalls") > stalls0 && played >= 3000 - 61 && played <= 3000 && !sim_audio_busy() &&
              g_failed_id == 0,
          "a supply cut under a playing stream: it stalls, is opened again and all but the ring's 60 ms plays (%.0f ms of 3000)", played);
    played0 = sim_audio_played_ms();
    id = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, 0, NULL);
    push_frames(id, 50);
    ink_audio_stream_finish(id);
    CHECK(playing_for(played0, 200), "the next stream plays");
    metalio_board_set(0, MB_P0_AMP, 0);
    metalio_board_set(1, MB_P1_SHARED_PWR, 0);
    osal_task_delay_us(11000000);
    CHECK(g_failed_id == id && !sim_audio_busy(), "cut for good: the stream fails, its owner is told and the output is off (%d, %d)", g_failed_id,
          id);
    metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    osal_task_delay_us(2000000);

    /* cancelled while its stalled output is being opened again (that waits for the chip to boot and take
     * its mode, ~1.4 s): nothing more of it plays (audit AUD7-003) */
    played0 = sim_audio_played_ms();
    id = ink_audio_stream_open(INK_AUDIO_PRIO_VOICE, 0, NULL);
    push_frames(id, 50);
    ink_audio_stream_finish(id);
    bool playing = playing_for(played0, 200);
    metalio_board_set(0, MB_P0_AMP, 0);
    metalio_board_set(1, MB_P1_SHARED_PWR, 0);
    osal_task_delay_us(1300000); /* stalled and closed; opening fails while P14 is off */
    metalio_board_set(1, MB_P1_SHARED_PWR, MB_P1_SHARED_PWR);
    osal_task_delay_us(500000); /* the next try is waiting for the chip */
    double at_cancel = sim_audio_played_ms();
    ink_audio_stop_all();
    osal_task_delay_us(3000000);
    CHECK(playing && sim_audio_played_ms() == at_cancel && !sim_audio_busy(),
          "a stream cancelled while its output is opened again plays nothing more (%.0f ms after the cancel)",
          sim_audio_played_ms() - at_cancel);
    ink_audio_on_stream_failed(NULL, NULL);
}

static void systest_task(void *arg) {
    (void)arg;
    if (ink_display_init() != 0 || ink_storage_init() != 0 || ink_power_init() != 0 || ink_audio_init() != 0) {
        CHECK(0, "the display, storage, power and audio services start");
        return;
    }
    storage_checks();
    audio_checks();
    ink_probe_mark("systest-done");
}

void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "systest", .fn = systest_task, .stack_bytes = 6144, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create the system test task");
}
