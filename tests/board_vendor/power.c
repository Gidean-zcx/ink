/* Actual power loop with a fake charger cadence and a yielding fake light sleep. */
#include "ink/hal_power.h"
#include "ink/probe.h"
#include "sim.h"
static uint32_t test_charger_poll(void);
static uint32_t test_sleep(const hal_sleep_cfg_t *cfg);
#define hal_power_charger_poll test_charger_poll
#define hal_power_sleep test_sleep
#include "../../system/services/power/power_service.c"
#undef hal_power_charger_poll
#undef hal_power_sleep

#define CHECK(ok, why) do { if (ok) ink_probe_count("test.pass"); else { ink_probe_count("test.fail"); sim_warn("board-vendor power: %s", why); } } while (0)
static unsigned polls, sleeps;
static uint32_t interval = 2000;
static uint64_t last_poll, prior_poll;
static bool in_sleep;
static uint32_t test_charger_poll(void) {
    CHECK(!in_sleep, "no charger access during light sleep");
    prior_poll = last_poll;
    last_poll = osal_now_us();
    polls++;
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
    CHECK(wait_polls(3, 6000000), "awake power task polls every 2 s");
    CHECK(last_poll - prior_poll >= 2000000 && last_poll - prior_poll < 2100000, "2 s cadence");
    interval = 10000;
    CHECK(wait_polls(polls + 2, 13000000), "power task honors 10 s interval");
    CHECK(last_poll - prior_poll >= 10000000 && last_poll - prior_poll < 10100000, "10 s cadence");
    ink_power_hold(INK_NEED_AWAKE);
    ink_power_lock();
    CHECK(wait_polls(polls + 1, 11000000) && ink_power_state() == INK_PWR_LOCKED && !sleeps, "locked awake still polls");
    ink_power_release(INK_NEED_AWAKE);
    uint64_t end = osal_now_us() + 2000000;
    while (!in_sleep && osal_now_us() < end) osal_task_delay_us(10000);
    CHECK(in_sleep, "enters fake light sleep");
    unsigned before = polls;
    CHECK(wait_polls(before + 1, 16000000), "charger resumes immediately after light sleep");
    CHECK(sleeps >= 1 && last_poll - prior_poll >= 15000000, "sleep did not run intermediate charger polls");
    ink_probe_count("test.finished");
}
void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "vendortest", .fn = test_task, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task starts");
}
