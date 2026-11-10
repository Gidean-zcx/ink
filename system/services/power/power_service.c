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
 * Lift to wake (D-122, setting power.lift_wake): lying flat when the chip goes to sleep, it also wakes on the
 * accelerometer's motion interrupt; if within a second the device is held at a reading angle, that is the
 * power key's unlock. A bump leaves it flat, a bag rarely holds it at that angle; after three motion wakes
 * that were not a lift the next slice goes without. Lying flat and still while idle, it locks after 3 min.
 *
 * Policy defaults (provisional, see the docs): idle after 5 s, lock after
 * 10 min without input (upstream: 180 s default; reading a page can take
 * minutes), modem kept registered while locked when the device has one
 * (SR-NF-01 "answerable standby").
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_device.h"
#include "ink/hal_display.h"
#include "ink/hal_power.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/data.h"
#include "ink/sys/audio.h"
#include "ink/sys/media.h"
#include "ink/sys/display.h"
#include "ink/sys/power.h"
#include "ink/sys/cap.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/sys/log.h"

/* Internal RAM: the light-sleep entry runs on this stack. 4096 like the vendor's light-sleep task
 * (Metalio-E-INK4-Plus power_policy_standby_lp.cc, "standby_lp", internal RAM); 3072 overflowed the
 * host estimate on Linux once lift-to-wake was added. */
#define PW_STACK 4096
#define PW_PRIO 4
#define IDLE_AFTER_US 5000000ull
#define LOCK_AFTER_US 600000000ull
#define SAVING_LOCK_US 60000000ull
#define SLICE_US 60000000ull
#define QUIET_POLL_US 200000
#define WAKE_CONFIRM_US 1000000
#define WAKE_AT_GRACE_US 5000000 /* something was due: it gets this long to be handled before the chip may sleep again */
#define BATTERY_POLL_US 60000000ull /* awake; asleep every slice is one look */
#define BATTERY_LOW 15
#define BATTERY_VERY_LOW 5
#define BATTERY_OFF 2
#define BATTERY_OFF_WAIT_US 5000000ull /* what waits to be written gets this long */
/* Postures by the gravity on z (out of the screen, mg): flat up to ~20 degrees of tilt; held to read
 * 35..84 degrees, screen up (at least 15 degrees apart). Axes and values: calibrate on the device. */
#define FLAT_Z_MG 940
#define VIEW_Z_MIN_MG 100
#define VIEW_Z_MAX_MG 819
#define LIFT_LOOKS 4 /* looks at the posture after a motion wake, 250 ms apart: lifting takes up to a second */
#define LIFT_LOOK_US 250000
#define LIFT_MISSES 3
#define STILL_LOCK_US 180000000ull

static struct {
    osal_mutex_t *lock;
    osal_sem_t *kick;
    osal_task_t *task;
    ink_power_state_t state;
    _Atomic ink_power_state_t notified;
    uint64_t last_activity, cause_us, locked_at;
    int holds[INK_NEED_COUNT];
    _Atomic bool lock_req;
    bool keep_modem;
    struct {
        ink_power_fn fn;
        void *ctx;
    } subs[8];
    int nsubs;
    bool (*checks[6])(void);
    int nchecks;
    uint64_t lock_after_us; /* the setting power.lock_after_s */
    bool saving;           /* runtime cap on the same auto-lock deadline */
    uint64_t wake_at;       /* ink_power_wake_at */
    uint64_t next_battery_us;
    bool battery_policy;    /* ink_power_battery_policy_start */
    void (*off_picture)(const char *why);
    bool off_picture_ok;
    int battery_level;      /* what has been told: 0 fine, 1 low, 2 very low, 3 powering off */
    bool powering_off;
    uint64_t off_at;
    int h_state, h_level, h_charging, h_low, h_saving, h_lifted;
    bool lift;              /* the setting power.lift_wake */
    int lift_misses;        /* motion wakes in a row that were not a lift */
    uint64_t still_since;   /* idle from then on, with this many movements seen */
    uint32_t still_moves;
} PS;

static uint64_t lock_after(void) {
    uint64_t us = __atomic_load_n(&PS.lock_after_us, __ATOMIC_RELAXED);
    return __atomic_load_n(&PS.saving, __ATOMIC_RELAXED) && us > SAVING_LOCK_US ? SAVING_LOCK_US : us;
}

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

static void tell_battery(int level, int percent) {
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL); /* large for this stack */
    if (!a) return;
    ink_args_init(a);
    ink_args_int(a, "level", level);
    ink_args_int(a, "percent", percent);
    ink_cap_emit("battery.low", a);
    osal_free(a);
}

static void stop_audio(void) {
    ink_audio_capture_end();
    ink_audio_ring(false);
    ink_audio_stop_all();
    ink_media_stop();
}

static bool audio_finished(void) {
    ink_media_status_t m;
    ink_media_status(&m);
    return !ink_audio_capture_busy() && !ink_audio_recording() && m.state != INK_MEDIA_PLAYING;
}

/* Battery as system state: read when the state changes, at each sleep slice and once a minute awake.
 * Low battery (foundation §7.4): a notification, then an interruption; nearly empty, what waits is written and
 * it powers off. Each level is told once on the way down; the charger starts over. Power task. */
static void publish_battery(void) {
    hal_battery_t b;
    hal_power_battery(&b);
    ink_state_set_int(PS.h_level, b.percent);
    ink_state_set_bool(PS.h_charging, b.charging);
    if (!__atomic_load_n(&PS.battery_policy, __ATOMIC_ACQUIRE)) return;
    int level = b.external_power || b.charging ? 0 : b.percent <= BATTERY_OFF ? 3 : b.percent <= BATTERY_VERY_LOW ? 2 : b.percent <= BATTERY_LOW ? 1 : 0;
    ink_state_set_bool(PS.h_low, level > 0);
    if (level > PS.battery_level) {
        PS.battery_level = level;
        tell_battery(level, b.percent);
        ink_probe_count("power.battery_low_told");
    } else if (!level && PS.battery_level) {
        PS.battery_level = 0;
        tell_battery(0, b.percent);
    }
    if (level == 3 && !PS.powering_off) {
        PS.powering_off = true;
        PS.off_at = osal_now_us() + BATTERY_OFF_WAIT_US;
        stop_audio();
        ink_db_flush_all();
    }
    if (PS.powering_off) {
        if (!level) PS.powering_off = false; /* it was plugged in just in time */
        else if ((audio_finished() && ink_db_idle() && ink_storage_pending() == 0) || osal_now_us() >= PS.off_at) {
            bool saved = audio_finished() && ink_db_idle() && ink_storage_pending() == 0;
            if (!saved) ink_probe_count("power.shutdown_unsaved");
            ink_probe_count("power.low_battery_off");
            if (PS.off_picture) PS.off_picture(saved ? "battery" : "battery_unsaved");
            ink_log_flush();
            hal_power_shutdown();
        }
    }
}

static void notify(ink_power_state_t from, ink_power_state_t to, uint64_t cause) {
    hal_power_set_state(hal_state(to));
    ink_state_set_str(PS.h_state, STATE_NAME[to]);
    if (to == INK_PWR_LOCKED) ink_state_set_bool(PS.h_lifted, false);
    publish_battery();
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

static int accel_z(void) {
    int x, y, z;
    return hal_accel_read(&x, &y, &z) ? z : -10000;
}

/* A motion wake while locked: held at a reading angle within a second is a lift, unlocked like the power key. */
static void lifted(void) {
    for (int i = 0; i < LIFT_LOOKS && ink_power_state() == INK_PWR_LOCKED; i++) {
        int z = accel_z();
        if (z >= VIEW_Z_MIN_MG && z <= VIEW_Z_MAX_MG) {
            PS.lift_misses = 0;
            ink_probe_count("power.lift_wake");
            hal_display_wake(1000);
            if (ink_power_unlock(osal_now_us())) ink_state_set_bool(PS.h_lifted, true);
            return;
        }
        osal_sem_take(PS.kick, LIFT_LOOK_US);
    }
    if (ink_power_state() != INK_PWR_LOCKED) return; /* a key came first */
    PS.lift_misses++;
    ink_probe_count("power.lift_ignored");
}

/* Idle, flat and still since still_since, nothing playing, recording, in a call or downloading: lock early. */
static void still_check(uint64_t now) {
    uint32_t moves = hal_accel_motion_count();
    bool flat = accel_z() >= FLAT_Z_MG, quiet_audio = audio_finished() && ink_audio_focus_top() == 0; /* a call holds focus */
    lock();
    bool still = flat && quiet_audio && moves == PS.still_moves && PS.state == INK_PWR_IDLE && !PS.holds[INK_NEED_NET] &&
                 !PS.holds[INK_NEED_AWAKE] && !PS.holds[INK_NEED_NO_LOCK];
    if (still) PS.lock_req = true;
    PS.still_since = now;
    PS.still_moves = moves;
    unlock();
    if (still) ink_probe_count("power.still_lock");
}

/* Sleep while locked, as long as nothing needs the chip. Returns when the state may have changed. */
static void locked_step(void) {
    if (PS.powering_off) {
        osal_sem_take(PS.kick, QUIET_POLL_US);
        return;
    }
    lock();
    bool busy = PS.holds[INK_NEED_AUDIO] > 0 || PS.holds[INK_NEED_NET] > 0 || PS.holds[INK_NEED_AWAKE] > 0;
    unlock();
    if (busy) { /* e.g. an audiobook: stay awake, locked; a release kicks us. It may go on for hours: the battery is looked at. */
        uint64_t now = osal_now_us();
        if (now >= PS.next_battery_us) {
            publish_battery();
            PS.next_battery_us = now + BATTERY_POLL_US;
        }
        osal_sem_take(PS.kick, (int64_t)(PS.next_battery_us - now));
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
    if (__atomic_load_n(&PS.lift, __ATOMIC_RELAXED) && PS.lift_misses < LIFT_MISSES && accel_z() >= FLAT_Z_MG)
        cfg.wake_mask |= HAL_WAKE_MOTION;
    lock();
    bool first = PS.locked_at != 0;
    uint64_t locked_at = PS.locked_at;
    uint64_t wake_at = PS.wake_at;
    unlock();
    uint64_t now = osal_now_us();
    if (wake_at && wake_at <= now + 1000) { /* due now: whoever asked is on its way */
        if (now - wake_at < WAKE_AT_GRACE_US || wake_at > now) {
            osal_sem_take(PS.kick, 20000);
            return;
        }
        ink_probe_count("power.wake_at_ignored"); /* its owner did not come: not worth an empty battery */
    } else if (wake_at && wake_at - now < cfg.max_us) cfg.max_us = wake_at - now;
    lock();
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
    if (why & HAL_WAKE_RTC) hal_rtc_clear_alarm(); /* its interrupt stays low until cleared, and it shares a line with the keys */
    if (why & HAL_WAKE_TIMER) PS.lift_misses = 0;
    if (why & HAL_WAKE_MOTION) lifted();
    publish_battery();
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
        } else if (s == INK_PWR_IDLE && !PS.holds[INK_NEED_NO_LOCK] && now - PS.last_activity >= lock_after()) {
            s = INK_PWR_LOCKED;
        }
        if (s == INK_PWR_LOCKED && PS.state != INK_PWR_LOCKED) {
            PS.locked_at = now;
            PS.lift_misses = 0;
        }
        if (s == INK_PWR_IDLE && PS.state != INK_PWR_IDLE) {
            PS.still_since = now;
            PS.still_moves = hal_accel_motion_count();
        }
        set_state(s);
        ink_power_state_t from = PS.notified;
        uint64_t cause = PS.cause_us;
        uint64_t deadline = s == INK_PWR_ACTIVE ? (PS.holds[INK_NEED_NET] ? 0 : PS.last_activity + IDLE_AFTER_US)
                            : s == INK_PWR_IDLE && !PS.holds[INK_NEED_NO_LOCK] ? PS.last_activity + lock_after()
                                                                                : 0;
        uint64_t still_at = s == INK_PWR_IDLE && __atomic_load_n(&PS.lift, __ATOMIC_RELAXED) && STILL_LOCK_US < lock_after()
                                ? PS.still_since + STILL_LOCK_US : 0;
        unlock();
        if (from != s) notify(from, s, cause);
        PS.notified = s;
        if (PS.powering_off) publish_battery();
        if (s == INK_PWR_LOCKED) {
            locked_step();
            continue;
        }
        now = osal_now_us();
        if (still_at && now >= still_at) {
            still_check(now);
            continue;
        }
        if (still_at && (!deadline || still_at < deadline)) deadline = still_at;
        if (now >= PS.next_battery_us) {
            publish_battery();
            PS.next_battery_us = now + BATTERY_POLL_US;
        }
        uint64_t next = PS.powering_off ? now + 100000 : PS.next_battery_us;
        if (!deadline || next < deadline) deadline = next;
        if (deadline > now) osal_sem_take(PS.kick, (int64_t)(deadline - now));
    }
}

static const ink_state_def_t ST_STATE = {.id = "power.state", .type = INK_ST_STR, .title = "使用状态（活跃 / 空闲 / 锁屏）"};
static const ink_state_def_t ST_LEVEL = {.id = "battery.level", .type = INK_ST_INT, .title = "电量（%）"};
static const ink_state_def_t ST_CHARGING = {.id = "battery.charging", .type = INK_ST_BOOL, .title = "在充电"};
static const ink_state_def_t ST_LOW = {.id = "battery.low", .type = INK_ST_BOOL, .title = "电量低"};
static const ink_state_def_t ST_SAVING = {.id = "power.saving", .type = INK_ST_BOOL, .title = "省电：最多 1 分钟锁屏，不改原设置"};
static const ink_state_def_t ST_LIFTED = {.id = "power.lifted", .type = INK_ST_BOOL, .title = "刚被拿起亮屏（到下次锁屏）"};

static void cap_saving(ink_call_t *c) { /* kept like any setting: it survives a restart */
    bool enabled;
    if (!ink_args_get_bool(&c->args, "enabled", &enabled)) { c->rc.status = INK_RC_INVALID; return; }
    if (ink_setting_set("power.saving", enabled, c->src, &c->rc) != INK_RC_DONE) return;
    snprintf(c->rc.say, sizeof c->rc.say, "%s", enabled ? "已打开省电，1 分钟锁屏，原设置保留" : "已关闭省电，按原设置锁屏");
}

static void apply_saving(void *ctx, int64_t on) {
    (void)ctx;
    __atomic_store_n(&PS.saving, on != 0, __ATOMIC_RELAXED);
    ink_state_set_bool(PS.h_saving, on != 0);
    osal_sem_give(PS.kick);
}

static void apply_lock_after(void *ctx, int64_t s) {
    (void)ctx;
    __atomic_store_n(&PS.lock_after_us, (uint64_t)s * 1000000ull, __ATOMIC_RELAXED);
    osal_sem_give(PS.kick); /* the power task re-plans its deadline */
}

static void apply_lift(void *ctx, int64_t on) {
    (void)ctx;
    __atomic_store_n(&PS.lift, on != 0, __ATOMIC_RELAXED);
    osal_sem_give(PS.kick);
}

static const ink_setting_def_t POWER_SETTINGS[] = {
    {.id = "power.lock_after_s", .title = "自动锁屏（秒）", .type = INK_SET_INT, .min = 60, .max = 3600, .def = (int64_t)(LOCK_AFTER_US / 1000000ull),
     .apply = apply_lock_after, .apply_exec = INK_EXEC_DIRECT},
    {.id = "power.saving", .title = "省电", .type = INK_SET_BOOL, .def = 0, .apply = apply_saving, .apply_exec = INK_EXEC_DIRECT},
    {.id = "power.lift_wake", .title = "拿起亮屏（平放不动 3 分钟先锁屏）", .type = INK_SET_BOOL, .def = 1, .apply = apply_lift,
     .apply_exec = INK_EXEC_DIRECT},
};

static void cap_shutdown(ink_call_t *c) { /* exec.bg */
    int failed = ink_storage_failed();
    stop_audio();
    ink_db_flush_all();
    for (int i = 0; i < 100 && (!audio_finished() || !ink_db_idle() || ink_storage_pending()); i++) osal_task_delay_us(50000);
    if (!audio_finished() || !ink_db_idle() || ink_storage_pending() || ink_storage_failed() != failed) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.say, sizeof c->rc.say, "还有内容没保存，请稍后重试关机");
        return;
    }
    snprintf(c->rc.say, sizeof c->rc.say, "已保存，正在关机");
    PS.off_picture_ok = false;
    if (PS.off_picture) PS.off_picture("shutdown");
    if (!PS.off_picture_ok) { c->rc.status = INK_RC_FAILED; snprintf(c->rc.say, sizeof c->rc.say, "关机画面没显示成功，请重试"); return; }
    if (!ink_log_flush()) { c->rc.status = INK_RC_FAILED; return; }
    hal_power_shutdown();
}

static const ink_cap_t POWER_CAPS[] = {
    {.id = "system.shutdown", .title = "关机", .effect = INK_FX_SYSTEM, .child_ok = true, .while_locked = true, .exec = INK_EXEC_BG, .run = cap_shutdown},
    {.id = "power.set_saving", .title = "开关省电（最多 1 分钟锁屏，不改原设置）",
     .input = "{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"}},\"required\":[\"enabled\"]}",
     .effect = INK_FX_SYSTEM, .exec = INK_EXEC_DIRECT, .run = cap_saving},
};

int ink_power_init(void) {
    PS.lock_after_us = LOCK_AFTER_US;
    PS.h_state = ink_state_define(&ST_STATE);
    PS.h_level = ink_state_define(&ST_LEVEL);
    PS.h_charging = ink_state_define(&ST_CHARGING);
    PS.h_low = ink_state_define(&ST_LOW);
    PS.h_saving = ink_state_define(&ST_SAVING);
    PS.h_lifted = ink_state_define(&ST_LIFTED);
    if (ink_settings_define(POWER_SETTINGS, 3) == 0) PS.lock_after_us = (uint64_t)ink_setting_get("power.lock_after_s") * 1000000ull;
    PS.lift = ink_setting_get("power.lift_wake") != 0;
    PS.saving = ink_setting_get("power.saving") != 0;
    ink_state_set_bool(PS.h_saving, PS.saving);
    ink_cap_register(POWER_CAPS, (int)(sizeof POWER_CAPS / sizeof POWER_CAPS[0]));
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
    ink_state_set_str(PS.h_state, STATE_NAME[INK_PWR_ACTIVE]);
    publish_battery();
    osal_task_cfg_t cfg = {.name = "power", .fn = power_task, .stack_bytes = PW_STACK, .priority = PW_PRIO, .core = -1};
#ifdef ESP_PLATFORM
    /* Vendor standby_lp: core 0, internal 4096-byte stack, idle + 3. */
    cfg.core = 0;
    cfg.priority = OSAL_PRIO_IDLE + 3;
#endif
    return osal_task_create(&cfg, &PS.task) == OSAL_OK ? 0 : -1;
}

void ink_power_off_picture_result(bool ok) { PS.off_picture_ok = ok; }

void ink_power_set_off_picture(void (*fn)(const char *why)) { PS.off_picture = fn; }

void ink_power_battery_policy_start(void) {
    __atomic_store_n(&PS.battery_policy, true, __ATOMIC_RELEASE);
    PS.next_battery_us = 0;
    osal_sem_give(PS.kick);
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

uint64_t ink_power_last_activity(void) {
    if (!PS.lock) return 0;
    lock();
    uint64_t t = PS.last_activity;
    unlock();
    return t;
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

bool ink_power_transition_pending(void) {
    return PS.lock_req || __atomic_load_n(&PS.state, __ATOMIC_SEQ_CST) != PS.notified;
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

void ink_power_wake_at(uint64_t t_us) {
    lock();
    PS.wake_at = t_us;
    unlock();
    osal_sem_give(PS.kick); /* it may be sleeping towards a later time only after the next slice: the wake below is for a task waiting */
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
