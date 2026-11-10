/* Exercise the actual time service; substitute only the port's callback registration. */
#include "ink/hal_device.h"
#include "ink/probe.h"
#include "ink/sys/exec.h"
#include "ink/sys/display.h"
#include "ink/sys/res.h"
#include "ink/sys/storage.h"
#include "ink/sys/net.h"
#include "sim.h"
static void (*network_cb)(void *, int64_t);
static void *network_ctx;
static int rtc_writes;
static bool expect_bg;
static bool test_rtc_set(int64_t utc) {
    if (expect_bg && !ink_exec_is_current(INK_EXEC_BG)) {
        ink_probe_count("test.fail");
        sim_warn("SNTP wrote RTC outside exec.bg");
    }
    rtc_writes++;
    return hal_rtc_set(utc);
}
#define hal_rtc_set test_rtc_set
#define hal_net_set_time_callback test_network_callback
#ifndef INK_TEST_TIME_SOURCE
#define INK_TEST_TIME_SOURCE "../../system/services/time/time_service.c"
#endif
#include INK_TEST_TIME_SOURCE
#undef hal_rtc_set
void test_network_callback(void (*cb)(void *, int64_t), void *ctx) { network_cb = cb; network_ctx = ctx; }
#define CHECK(ok) do { if (ok) ink_probe_count("test.pass"); else { ink_probe_count("test.fail"); sim_warn("time callback check at %d", __LINE__); } } while (0)
static bool delivered;
static ink_exec_job_t barrier;
static void mark_delivered(void *arg) { (void)arg; delivered = true; }
static void receive_time(int64_t utc) {
    delivered = false;
    expect_bg = true;
    network_cb(network_ctx, utc);
    CHECK(ink_exec_post(INK_EXEC_BG, &barrier, mark_delivered, NULL) == 0);
    for (int i = 0; i < 1000 && !delivered; i++) osal_task_delay_us(1000);
    CHECK(delivered);
    expect_bg = false;
}
static void test_task(void *arg) {
    (void)arg;
    CHECK(ink_display_init() == 0 && ink_storage_init() == 0 && ink_cap_init() == 0 && ink_exec_init() == 0 && ink_res_init() == 0 &&
          ink_state_init() == 0 && ink_settings_init() == 0 && ink_svc_init() == 0 && ink_data_init() == 0 && ink_power_init() == 0 &&
          ink_net_init() == 0 && ink_hub_init("wss://example.com/v1/device") == 0 && ink_tools_init() == 0);
    ink_members_init();
    CHECK(ink_time_init() == 0 && network_cb);
    if (!network_cb) return;
    const int64_t utc = 1794268800;
    receive_time(utc);
    int64_t rtc = 0;
    CHECK(ink_time_valid() && ink_time_now_s() >= utc && ink_time_now_s() <= utc + 1);
    CHECK(rtc_writes == 1 && hal_rtc_get(&rtc) && rtc >= utc && rtc <= utc + 1);
    int64_t synced = 0;
    CHECK(ink_state_get_int(ink_state_find("time.synced"), &synced) && synced);
    receive_time(utc + 1);
    CHECK(rtc_writes == 1); /* same two-second threshold as hub time */
    ink_receipt_t rc;
    CHECK(ink_setting_set("time.auto", 0, INK_SRC_SYSTEM, &rc) == INK_RC_DONE);
    receive_time(utc + 500);
    CHECK(rtc_writes == 1 && ink_time_now_s() < utc + 10);
    __atomic_store_n(&T.valid, false, __ATOMIC_RELEASE);
    receive_time(utc + 500);
    CHECK(rtc_writes == 2 && ink_time_valid() && ink_time_now_s() >= utc + 500);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "sntptest", .fn = test_task, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK);
}
