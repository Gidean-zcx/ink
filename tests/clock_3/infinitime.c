/* clock-3: every copied InfiniTime function, called from C through system/apps/clock/clock_infinitime.cpp. The services
 * are faked: virtual time, deadlines fired in time order by advance(), cards and wake-lock holds counted. No page
 * ever calls ck_timer_tick for the ringing timer: its sound stops by its own deadline. */
#include <stdio.h>
#include <string.h>
#include "clock_infinitime.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/exec.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/power.h"
#include "ink/sys/svc.h"

static int failures, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint64_t now = 1000000;
static ink_svc_deadline_t *armed[128];
static int narmed, holds, raised, ended, quieted, full, finished, label_part = -1, labels;
static bool table_full;
uint64_t osal_now_us(void) { return now; }
void ink_svc_lock(void) {}
void ink_svc_unlock(void) {}
void ink_svc_at(ink_svc_deadline_t *d, uint64_t t_us, void (*fn)(void *), void *ctx) {
    bool known = false;
    for (int i = 0; i < narmed; i++) known |= armed[i] == d;
    if (!known) armed[narmed++] = d;
    d->at_us = t_us; d->fn = fn; d->ctx = ctx; d->armed = true;
}
void ink_svc_cancel(ink_svc_deadline_t *d) { d->armed = false; }
int ink_exec_post(ink_exec_t e, ink_exec_job_t *job, void (*fn)(void *), void *arg) { (void)e; (void)job; fn(arg); return 0; }
void ink_interrupt_update(int id, const char *body, const ink_int_action_t *actions, int nactions, bool ringing) {
    (void)body; (void)actions; (void)nactions;
    if (id > 0 && !ringing) quieted++;
}
void ink_interrupt_end(int id) { if (id > 0) ended++; }
void ink_power_hold(ink_power_need_t need) { if (need == INK_NEED_AWAKE) holds++; }
void ink_power_release(ink_power_need_t need) { if (need == INK_NEED_AWAKE) holds--; }
void ink_probe_count(const char *metric) { if (!strcmp(metric, "clock.timer_card_full")) full++; }
static void edit_label(int part) { label_part = part; labels++; }
static int ring(int t) { (void)t; return table_full ? -1 : ++raised; }
static void done(int t) { (void)t; finished++; }

static void advance(uint64_t us) { /* the service timer task: each due deadline in time order */
    uint64_t until = now + us;
    for (;;) {
        ink_svc_deadline_t *next = NULL;
        for (int i = 0; i < narmed; i++)
            if (armed[i]->armed && armed[i]->at_us <= until && (!next || armed[i]->at_us < next->at_us)) next = armed[i];
        if (!next) break;
        if (next->at_us > now) now = next->at_us;
        next->armed = false;
        next->fn(next->ctx);
    }
    now = until;
}

static void stopwatch(void) { /* StopWatchController: Start, Pause, Clear, AddLapToHistory, GetLapFromHistory, GetElapsedTime */
    CHECK(ck_watch_state(0) == 0 && ck_watch_ms(0) == 0, "a new stopwatch is cleared");
    ck_watch_start(0); advance(1500000); ck_watch_pause(0);
    CHECK(ck_watch_ms(0) == 1500 && ck_watch_state(0) == 2, "paused at 1.5 s: %u", (unsigned)ck_watch_ms(0));
    advance(2000000); CHECK(ck_watch_ms(0) == 1500, "a pause does not count");
    CHECK(!ck_watch_lap(0) && ck_watch_laps(0) == 0, "no lap while paused");
    ck_watch_start(0); advance(250000); CHECK(ck_watch_lap(0), "lap 1");
    advance(510000); CHECK(ck_watch_lap(0), "lap 2");
    advance(120000); CHECK(ck_watch_lap(0), "lap 3");
    CHECK(ck_watch_lap_ms(0, 1) == 1750 && ck_watch_lap_ms(0, 2) == 2260 && ck_watch_lap_ms(0, 3) == 2380, "lap totals exclude the pause: %u %u %u",
          (unsigned)ck_watch_lap_ms(0, 1), (unsigned)ck_watch_lap_ms(0, 2), (unsigned)ck_watch_lap_ms(0, 3));
    CHECK(ck_watch_lap_ms(0, 2) - ck_watch_lap_ms(0, 1) == 510 && ck_watch_lap_ms(0, 3) - ck_watch_lap_ms(0, 2) == 120, "a lap is the difference of totals");
    for (int i = 3; i < 99; i++) { advance(1000); ck_watch_lap(0); }
    CHECK(ck_watch_laps(0) == 99 && ck_watch_lap_ms(0, 1) == 1750, "99 laps kept, the first still there");
    CHECK(!ck_watch_lap(0) && ck_watch_laps(0) == 99 && ck_watch_lap_ms(0, 99) == 2380 + 96, "the 100th is refused without losing one");
    ck_watch_start(0); CHECK(ck_watch_laps(0) == 99, "start while running changes nothing");
    ck_watch_clear(0); CHECK(ck_watch_ms(0) == 0 && ck_watch_laps(0) == 0 && ck_watch_state(0) == 0, "clear");
    ck_watch_start(1); advance(1000ull * 3600 * 1000000 + 5000000); /* the original wraps at 1000 hours */
    CHECK(ck_watch_ms(1) == 5000, "1000 hours wrap to zero: %u", (unsigned)ck_watch_ms(1));
    CHECK(ck_watch_ms(0) == 0, "stopwatches are separate");
    ck_watch_clear(1);
}
static void counters(void) { /* Counter: UpBtnPressed, DownBtnPressed, SetValue */
    ck_edit_set(CK_HOUR, 23); CHECK(label_part == CK_HOUR && ck_edit_get(CK_HOUR) == 23, "set redraws its own number");
    ck_edit_step(CK_HOUR, 1); CHECK(ck_edit_get(CK_HOUR) == 0, "23 + 1 wraps to 0");
    ck_edit_step(CK_HOUR, -1); CHECK(ck_edit_get(CK_HOUR) == 23, "0 - 1 wraps to 23");
    ck_edit_set(CK_MINUTE, 0); ck_edit_step(CK_MINUTE, -1); CHECK(ck_edit_get(CK_MINUTE) == 59 && label_part == CK_MINUTE, "minutes wrap, no carry");
    ck_edit_set(CK_SECOND, 59); int before = labels; ck_edit_step(CK_SECOND, 1);
    CHECK(ck_edit_get(CK_SECOND) == 0 && ck_edit_get(CK_MINUTE) == 59 && labels == before + 1, "seconds wrap alone, one label");
}
static void timers(void) { /* Timer controller and the copied ToggleRunning, Refresh, DisplayTime, Reset */
    ck_timer_set(0, 65); CHECK(ck_timer_shown(0) == 65 && ck_timer_state(0) == CK_IDLE && holds == 0, "65 s set");
    ck_timer_set(1, 180); ck_timer_toggle(1); ck_timer_toggle(0);
    CHECK(ck_timer_state(0) == CK_RUNNING && holds == 2 && ck_timer_shown(0) == 65, "both run, each holds the device awake once");
    advance(1000); CHECK(!ck_timer_tick(0) && ck_timer_shown(0) == 65, "rounded up: the set time shows first");
    advance(29999000); CHECK(ck_timer_tick(0) && ck_timer_shown(0) == 35, "35 s left at 30 s: %u", (unsigned)ck_timer_shown(0));
    CHECK(!ck_timer_tick(0), "the same second does not redraw");
    ck_timer_toggle(0); CHECK(ck_timer_state(0) == CK_PAUSED && ck_timer_shown(0) == 35 && holds == 1, "paused at 35");
    advance(10000000); CHECK(ck_timer_shown(0) == 35 && !ck_timer_tick(0), "a pause holds");
    ck_timer_toggle(0); CHECK(ck_timer_state(0) == CK_RUNNING && holds == 2, "resumed");
    advance(34500000); ck_timer_tick(0); CHECK(ck_timer_shown(0) == 1 && finished == 0, "half a second shows 0:01");
    advance(500000);
    CHECK(finished == 1 && ck_timer_state(0) == CK_FINISHED && raised == 1 && ck_timer_shown(0) == 0, "rings at 65 s of running");
    CHECK(ck_timer_state(1) == CK_RUNNING && ck_timer_tick(1) && ck_timer_shown(1) == 105, "the other keeps going: %u", (unsigned)ck_timer_shown(1));
    CHECK(holds == 2 && quieted == 0, "ringing holds the device awake");
    advance(59000000); CHECK(quieted == 0, "still ringing at 59 s");
    advance(1100000); CHECK(quieted == 1 && holds == 1 && ck_timer_state(0) == CK_FINISHED && ended == 0, "quiet after 60 s without a page, card stays");
    advance(5000000); CHECK(quieted == 1, "quiet once");
    ck_timer_restart(0); CHECK(ended == 1 && ck_timer_state(0) == CK_RUNNING && ck_timer_shown(0) == 65 && holds == 2, "again from 1:05");
    ck_timer_set(0, 65); CHECK(ck_timer_state(0) == CK_IDLE && ck_timer_shown(0) == 65 && holds == 1, "reset while running: back to the duration");
    ck_timer_set(1, 180); CHECK(holds == 0, "everything stopped: nothing held");
    ck_timer_set(2, 3600); CHECK(ck_timer_shown(2) == 3600, "an hour");
    ck_timer_toggle(2); CHECK(ck_timer_state(2) == CK_RUNNING, "1:00:00 starts (the hour counts)");
    advance(1000000); ck_timer_tick(2); CHECK(ck_timer_shown(2) == 3599 && ck_timer_left_ms(2) == 3599000, "carries to 59:59: %u", (unsigned)ck_timer_shown(2));
    ck_timer_set(2, 0); ck_timer_toggle(2); CHECK(ck_timer_state(2) == CK_IDLE, "zero does not start");
    ck_timer_set(3, 2); ck_timer_set(4, 2); ck_timer_toggle(3); ck_timer_toggle(4); table_full = true; advance(2000000);
    CHECK(finished == 3 && full == 2 && holds == 0 && ck_timer_state(3) == CK_FINISHED, "a full card table: finished, nothing rings, nothing held");
    table_full = false; ck_timer_set(3, 2); ck_timer_set(4, 2);
    CHECK(ck_timer_state(3) == CK_IDLE && holds == 0, "dismissed");
}
int main(void) {
    ck_init(&(ck_hooks_t){.edit_label = edit_label, .ring = ring, .finished = done});
    stopwatch(); counters(); timers();
    printf("clock-3-infinitime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
