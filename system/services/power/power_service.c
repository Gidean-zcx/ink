/*
 * Power service (see ink/sys/power.h).
 *
 * The task sleeps until an event (activity, a hold released, lock/unlock) or
 * the next policy deadline; it never polls while the device is active or
 * idle. Any task may change the state under the lock (input, for example,
 * switches IDLE -> ACTIVE and raises the clock at once, before the UI starts
 * rendering); listeners are told from the power task afterwards.
 *
 * Locked: the task waits for the panel and the flash to settle, then puts
 * the chip into light sleep in slices (upstream: 60 s) with the power key,
 * the AI key, the RTC and (with the modem kept up) the modem as wake
 * sources. A key wake is confirmed by the input service unlocking; if no
 * unlock follows within a second, the chip goes back to sleep.
 *
 * Policy defaults (provisional, see the docs): idle after 5 s, lock after
 * 10 min without input (upstream: 180 s default; reading a page can take
 * minutes), modem kept registered while locked when the device has one
 * (SR-NF-01 "answerable standby").
 */
#include <string.h>

#include "ink/hal_device.h"
#include "ink/hal_power.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/display.h"
#include "ink/sys/power.h"
#include "ink/sys/storage.h"

#define PW_STACK 3072 /* internal RAM: the light-sleep entry runs on this stack */
#define PW_PRIO 4
#define IDLE_AFTER_US 5000000ull
#define LOCK_AFTER_US 600000000ull
#define SLICE_US 60000000ull
#define QUIET_POLL_US 200000
#define WAKE_CONFIRM_US 1000000

static struct {
    osal_mutex_t *lock;
    osal_sem_t *kick;
    osal_task_t *task;
    ink_power_state_t state, notified;
    uint64_t last_activity, cause_us, locked_at;
    int holds[INK_NEED_COUNT];
    bool lock_req;
    bool keep_modem;
    struct {
        ink_power_fn fn;
        void *ctx;
    } subs[8];
    int nsubs;
    bool (*checks[6])(void);
    int nchecks;
} PS;

static const char *STATE_NAME[] = {"active", "idle", "locked"};

static void lock(void) { osal_mutex_lock(PS.lock, OSAL_WAIT_FOREVER); }
static void unlock(void) { osal_mutex_unlock(PS.lock); }

/* PS.state changes under the lock; the sleep cancel check reads it without (atomic). */
static void set_state(ink_power_state_t s) { __atomic_store_n(&PS.state, s, __ATOMIC_SEQ_CST); }

static bool cancel_sleep(void *ctx) { /* called by the HAL right before the chip sleeps: no blocking */
    (void)ctx;
    return __atomic_load_n(&PS.state, __ATOMIC_SEQ_CST) != INK_PWR_LOCKED;
}

static hal_power_state_t hal_state(ink_power_state_t s) {
    return s == INK_PWR_ACTIVE ? HAL_PWR_STATE_ACTIVE : s == INK_PWR_IDLE ? HAL_PWR_STATE_IDLE : HAL_PWR_STATE_LOCKED;
}

static void notify(ink_power_state_t from, ink_power_state_t to, uint64_t cause) {
    hal_power_set_state(hal_state(to));
    char key[40];
    strcpy(key, "power.to_");
    strcat(key, STATE_NAME[to]);
    ink_probe_count(key);
    for (int i = 0; i < PS.nsubs; i++) PS.subs[i].fn(PS.subs[i].ctx, from, to, cause);
}

/* Nothing left that would change the panel or the flash while the chip sleeps. The UI check
 * comes first: what it still has to draw reaches the display service before it reports idle. */
static bool quiet(void) {
    for (int i = 0; i < PS.nchecks; i++)
        if (PS.checks[i]()) return false;
    return ink_display_busy() == 0 && ink_storage_pending() == 0;
}

/* Sleep while locked, as long as nothing needs the chip. Returns when the state may have changed. */
static void locked_step(void) {
    lock();
    bool busy = PS.holds[INK_NEED_AUDIO] > 0 || PS.holds[INK_NEED_NET] > 0;
    unlock();
    if (busy) { /* e.g. an audiobook: stay awake, locked; a release kicks us */
        osal_sem_take(PS.kick, OSAL_WAIT_FOREVER);
        return;
    }
    if (!quiet()) { /* the lock mark is still being drawn, a write is queued ... */
        ink_probe_count("power.sleep_deferred");
        osal_sem_take(PS.kick, QUIET_POLL_US);
        return;
    }
    hal_sleep_cfg_t cfg = {.max_us = SLICE_US,
                           .wake_mask = HAL_WAKE_POWER_KEY | HAL_WAKE_AI_KEY | HAL_WAKE_RTC | HAL_WAKE_MODEM,
                           .keep_modem = PS.keep_modem,
                           .cancel = cancel_sleep};
    lock();
    bool first = PS.locked_at != 0;
    uint64_t locked_at = PS.locked_at;
    PS.locked_at = 0;
    unlock();
    if (first) ink_probe_value("power.lock_to_sleep_ms", (double)(osal_now_us() - locked_at) / 1000.0);
    uint32_t why = hal_power_sleep(&cfg);
    if (!why) {
        ink_probe_count("power.sleep_refused");
        osal_sem_take(PS.kick, QUIET_POLL_US);
        return;
    }
    ink_probe_count(why & HAL_WAKE_TIMER ? "power.wake.timer" : why & (HAL_WAKE_POWER_KEY | HAL_WAKE_AI_KEY) ? "power.wake.key" : "power.wake.other");
    if (why & (HAL_WAKE_POWER_KEY | HAL_WAKE_AI_KEY)) osal_sem_take(PS.kick, WAKE_CONFIRM_US); /* the unlock follows */
}

static void power_task(void *arg) {
    (void)arg;
    for (;;) {
        uint64_t now = osal_now_us();
        lock();
        ink_power_state_t s = PS.state;
        /* a transfer the user waits for (an AI turn, a sync) keeps the device active: its
         * answer is parsed and drawn at full clock, as upstream keeps AppIdle off while the
         * device is not idle; audio does not (the codec holds its own CPU boost) */
        bool user_idle = now - PS.last_activity >= IDLE_AFTER_US && PS.holds[INK_NEED_NET] == 0;
        if (PS.lock_req) {
            PS.lock_req = false;
            if (s != INK_PWR_LOCKED) s = INK_PWR_LOCKED;
        } else if (s == INK_PWR_ACTIVE && user_idle) {
            s = INK_PWR_IDLE;
        } else if (s == INK_PWR_IDLE && !user_idle) {
            s = INK_PWR_ACTIVE;
        } else if (s == INK_PWR_IDLE && !PS.holds[INK_NEED_NO_LOCK] && now - PS.last_activity >= LOCK_AFTER_US) {
            s = INK_PWR_LOCKED;
        }
        if (s == INK_PWR_LOCKED && PS.state != INK_PWR_LOCKED) PS.locked_at = now;
        set_state(s);
        ink_power_state_t from = PS.notified;
        uint64_t cause = PS.cause_us;
        PS.notified = s;
        uint64_t deadline = s == INK_PWR_ACTIVE ? (PS.holds[INK_NEED_NET] ? 0 : PS.last_activity + IDLE_AFTER_US)
                            : s == INK_PWR_IDLE && !PS.holds[INK_NEED_NO_LOCK] ? PS.last_activity + LOCK_AFTER_US
                                                                                : 0;
        unlock();
        if (from != s) notify(from, s, cause);
        if (s == INK_PWR_LOCKED) {
            locked_step();
            continue;
        }
        now = osal_now_us();
        if (!deadline) osal_sem_take(PS.kick, OSAL_WAIT_FOREVER);
        else if (deadline > now) osal_sem_take(PS.kick, (int64_t)(deadline - now));
    }
}

int ink_power_init(void) {
    const char *prev = osal_mem_tag("power");
    PS.lock = osal_mutex_create();
    PS.kick = osal_sem_create(0, 1);
    osal_mem_tag(prev);
    if (!PS.lock || !PS.kick) return -1;
    hal_device_info_t di;
    hal_device_get_info(&di);
    PS.keep_modem = di.has_modem;
    set_state(INK_PWR_ACTIVE);
    PS.notified = INK_PWR_ACTIVE;
    PS.last_activity = osal_now_us();
    hal_power_set_state(HAL_PWR_STATE_ACTIVE);
    osal_task_cfg_t cfg = {.name = "power", .fn = power_task, .stack_bytes = PW_STACK, .priority = PW_PRIO, .core = -1};
    return osal_task_create(&cfg, &PS.task) == OSAL_OK ? 0 : -1;
}

ink_power_state_t ink_power_state(void) {
    lock();
    ink_power_state_t s = PS.state;
    unlock();
    return s;
}

void ink_power_activity(uint64_t t_us) {
    lock();
    if (t_us > PS.last_activity) PS.last_activity = t_us;
    bool wake = PS.state == INK_PWR_IDLE;
    if (wake) set_state(INK_PWR_ACTIVE);
    unlock();
    if (wake) {
        hal_power_set_state(HAL_PWR_STATE_ACTIVE); /* full clock before the UI reacts to this input */
        osal_sem_give(PS.kick);
    }
}

void ink_power_hold(ink_power_need_t need) {
    if ((unsigned)need >= INK_NEED_COUNT) return;
    lock();
    PS.holds[need]++;
    unlock();
    osal_sem_give(PS.kick);
}

void ink_power_release(ink_power_need_t need) {
    if ((unsigned)need >= INK_NEED_COUNT) return;
    lock();
    if (PS.holds[need] > 0) PS.holds[need]--;
    unlock();
    osal_sem_give(PS.kick);
}

void ink_power_lock(void) {
    lock();
    PS.lock_req = true;
    unlock();
    osal_sem_give(PS.kick);
}

bool ink_power_unlock(uint64_t t_us) {
    lock();
    bool was = PS.state == INK_PWR_LOCKED;
    if (was) {
        set_state(INK_PWR_ACTIVE);
        PS.cause_us = t_us;
        if (t_us > PS.last_activity) PS.last_activity = t_us;
        PS.lock_req = false;
    }
    unlock();
    if (was) {
        hal_power_set_state(HAL_PWR_STATE_ACTIVE);
        osal_sem_give(PS.kick);
    }
    return was;
}

int ink_power_add_busy_check(bool (*busy)(void)) {
    if (!busy || PS.nchecks >= 6) return -1;
    PS.checks[PS.nchecks++] = busy; /* registered at start-up, before the device can lock */
    return 0;
}

int ink_power_subscribe(ink_power_fn fn, void *ctx) {
    if (!fn || PS.nsubs >= 8) return -1;
    PS.subs[PS.nsubs].fn = fn;
    PS.subs[PS.nsubs].ctx = ctx;
    PS.nsubs++;
    return 0;
}
