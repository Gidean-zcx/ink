#define ink_main original_time_main
#include "../../../../tests/integration/timesvc.c"
#undef ink_main
static void audit_time(void *arg) {
    (void)arg;
    CHECK(ink_display_init() == 0 && ink_storage_init() == 0 && ink_cap_init() == 0 && ink_exec_init() == 0 && ink_res_init() == 0 &&
          ink_state_init() == 0 && ink_settings_init() == 0 && ink_svc_init() == 0 && ink_data_init() == 0 && ink_power_init() == 0 &&
          ink_net_init() == 0 && ink_hub_init("wss://hub.local/v1/device") == 0, "init services");
    ink_members_init();
    CHECK(ink_time_init() == 0, "time init");
    const char *phase = getenv("INK_AUDIT_PHASE");
    if (phase && !strcmp(phase, "write")) {
        ink_alarm_t a = {.kind = INK_ALARM, .hour = 9, .repeat = INK_REPEAT_WEEKDAYS, .weekdays = 0x7f};
        snprintf(a.title, sizeof a.title, "daily at nine");
        CHECK(ink_alarm_add(&a) > 0, "add alarm");
        set_zone("UTC");
        int cur = 0;
        CHECK(ink_alarm_next(&cur, INK_ALARM, &a) == 1, "read alarm");
        ink_tm_t t;
        ink_time_to_local(a.next_s, &t);
        printf("AUDIT time before reboot zone=%s next_local_hour=%d next_s=%lld\n", ink_time_zone(), t.hour, (long long)a.next_s);
        CHECK(t.hour == 9, "nine before reboot");
        ink_db_flush_all();
        for (int i = 0; i < 500 && (!ink_db_idle() || ink_storage_pending()); i++) osal_task_delay_us(10000);
        CHECK(ink_db_idle(), "durable alarms");
        osal_task_delay_us(2000000); /* settings debounce also reaches flash */
    } else {
        int cur = 0;
        ink_alarm_t a;
        CHECK(ink_alarm_next(&cur, INK_ALARM, &a) == 1, "restored alarm");
        ink_tm_t t;
        ink_time_to_local(a.next_s, &t);
        printf("AUDIT time after reboot zone=%s rule_hour=%d next_local_hour=%d next_s=%lld\n", ink_time_zone(), a.hour, t.hour, (long long)a.next_s);
        CHECK(!strcmp(ink_time_zone(), "UTC") && a.hour == 9 && t.hour == 1, "reproduce stale persisted next time");
    }
    ink_probe_count("test.finished");
}
void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "audit-time", .fn = audit_time, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create test");
}
