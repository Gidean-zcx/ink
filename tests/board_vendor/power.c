/* Actual power loop with a fake charger cadence and a yielding fake light sleep. */
#include "ink/hal_power.h"
#include "ink/probe.h"
#include "sim.h"
static uint32_t test_charger_poll(void);
static uint32_t test_sleep(const hal_sleep_cfg_t *cfg);
static void test_battery(hal_battery_t *out);
#define hal_power_charger_poll test_charger_poll
#define hal_power_sleep test_sleep
#define hal_power_battery test_battery
#include "../../system/services/power/power_service.c"
#undef hal_power_charger_poll
#undef hal_power_sleep
#undef hal_power_battery

#define CHECK(ok, why) do { if (ok) ink_probe_count("test.pass"); else { ink_probe_count("test.fail"); sim_warn("board-vendor power: %s", why); } } while (0)
static unsigned polls, sleeps;
static uint32_t interval = 2000;
static uint64_t last_poll, prior_poll, first_poll, woke_at;
static bool in_sleep, stall, in_poll;
static int battery_mv = 4200;
static void test_battery(hal_battery_t *out) { *out = (hal_battery_t){.percent = 80, .millivolts = battery_mv}; }
static uint32_t test_charger_poll(void) {
    CHECK(!in_sleep, "no charger access during light sleep");
    CHECK(ink_exec_is_current(INK_EXEC_BG), "charger I2C runs on the existing background executor");
    prior_poll = last_poll;
    last_poll = osal_now_us();
    polls++;
    if (!first_poll) first_poll = last_poll;
    if (stall) {
        in_poll = true;
        osal_task_delay_us(700000);
        in_poll = false;
    }
    return interval;
}
static uint32_t test_sleep(const hal_sleep_cfg_t *cfg) {
    (void)cfg;
    unsigned before = polls;
    in_sleep = true;
    sleeps++;
    osal_task_delay_us(15000000);
    CHECK(polls == before, "charger suspended across yielding light sleep");
    in_sleep = false;
    woke_at = osal_now_us();
    ink_power_unlock(woke_at);
    return HAL_WAKE_TIMER;
}
static bool wait_polls(unsigned count, uint64_t limit) {
    uint64_t end = osal_now_us() + limit;
    while (polls < count && osal_now_us() < end) osal_task_delay_us(10000);
    return polls >= count;
}
static void test_task(void *arg) {
    (void)arg;
    CHECK(ink_display_init() == 0 && ink_storage_init() == 0 && ink_cap_init() == 0 && ink_exec_init() == 0 &&
          ink_state_init() == 0 && ink_settings_init() == 0 && ink_power_init() == 0 && ink_light_init() == 0, "services initialize");
    ink_power_hold(INK_NEED_NO_LOCK);
    uint64_t started = PS.last_activity;
    CHECK(wait_polls(3, 6100000), "awake charger polls every 2 s");
    CHECK(first_poll - started >= 2000000 && first_poll - started < 2100000, "boot waits 2 s before first charger check");
    CHECK(last_poll - prior_poll >= 2000000 && last_poll - prior_poll < 2100000, "2 s cadence");
    ink_power_hold(INK_NEED_AWAKE);
    stall = true;
    uint64_t end = osal_now_us() + 2100000;
    while (!in_poll && osal_now_us() < end) osal_task_delay_us(1000);
    CHECK(in_poll, "fake charger I2C is in flight");
    ink_power_lock();
    end = osal_now_us() + 100000;
    while (PS.notified != INK_PWR_LOCKED && osal_now_us() < end) osal_task_delay_us(1000);
    CHECK(PS.notified == INK_PWR_LOCKED && in_poll, "power task handles lock during charger I2C");
    CHECK(ink_power_unlock(osal_now_us()), "unlock during charger I2C");
    stall = false;
    interval = 10000;
    CHECK(wait_polls(polls + 2, 21000000), "background charger honors 10 s interval after pending I2C");
    CHECK(last_poll - prior_poll >= 10000000 && last_poll - prior_poll < 10100000, "10 s cadence");
    ink_power_lock();
    CHECK(wait_polls(polls + 1, 11000000) && ink_power_state() == INK_PWR_LOCKED && !sleeps, "locked awake still polls");
    ink_power_release(INK_NEED_AWAKE);
    battery_mv = 4400;
    end = osal_now_us() + 2000000;
    while (!in_sleep && osal_now_us() < end) osal_task_delay_us(10000);
    CHECK(in_sleep, "enters fake light sleep");
    unsigned before = polls;
    CHECK(wait_polls(before + 1, 26000000), "charger resumes after light sleep and vendor delay");
    CHECK(last_poll - woke_at >= 10000000 && last_poll - woke_at < 10100000, "wake waits 10 s before first charger check");
    CHECK(sleeps >= 1 && last_poll - prior_poll >= 15000000, "sleep did not run intermediate charger polls");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "vendortest", .fn = test_task, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task starts");
}
