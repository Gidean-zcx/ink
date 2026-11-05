#include "product.h"

static int64_t ringing(void) {
    int64_t n = 0;
    ink_state_get_int(ink_state_find("alarm.ringing"), &n);
    return n;
}
static void run(void *arg) {
    (void)arg;
    delay_ms(1800);
    char undo[160], other_undo[160], args[180];
    CHECK(AI("alarm.set", "{\"hour\":7,\"minute\":30,\"weekdays\":127,\"title\":\"SYNTH_CONTROL\"}") && g_rc.status == INK_RC_DONE && g_rc.undo_ms > 0, "create control alarm");
    CHECK(g_rc.revision > 0, "alarm inverse has a version");
    snprintf(undo, sizeof undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, g_rc.revision);
    CHECK(AI("alarm.set", "{\"hour\":8,\"minute\":30,\"weekdays\":127,\"title\":\"SYNTH_OTHER\"}") && g_rc.status == INK_RC_DONE, "another alarm is independent");
    snprintf(other_undo, sizeof other_undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, g_rc.revision);
    CHECK(AI("system.undo", undo) && g_rc.status == INK_RC_UNDONE, "untouched alarm inverse works");
    CHECK(AI("alarm.list", NULL) && !strstr(g_result, "SYNTH_CONTROL"), "control alarm removed");
    CHECK(AI("system.undo", other_undo) && g_rc.status == INK_RC_UNDONE, "moving a table slot preserves its version");
    CHECK(AI("reminder.add", "{\"at\":\"2099-12-01 12:00\",\"title\":\"SYNTH_CONTROL\"}") && g_rc.status == INK_RC_DONE && g_rc.undo_ms > 0, "create control reminder");
    CHECK(g_rc.revision > 0, "reminder inverse has a version");
    snprintf(undo, sizeof undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, g_rc.revision);
    CHECK(AI("system.undo", undo) && g_rc.status == INK_RC_UNDONE, "untouched reminder inverse works");

    int64_t target = ink_time_now_s() / 60 * 60 + 120;
    ink_time_set(target - 2, false);
    ink_tm_t local;
    ink_time_to_local(target, &local);
    snprintf(args, sizeof args, "{\"hour\":%d,\"minute\":%d,\"weekdays\":127,\"title\":\"SYNTH_SNOOZED\"}", local.hour, local.min);
    CHECK(AI("alarm.set", args) && g_rc.status == INK_RC_DONE, "create soon ringing alarm");
    unsigned alarm = 0;
    CHECK(sscanf(g_rc.object, "alarm:%u", &alarm) == 1, "alarm identity");
    snprintf(undo, sizeof undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, g_rc.revision);
    CHECK(WAIT_FOR(ringing() > 0, 4000), "alarm actually rings");
    snprintf(args, sizeof args, "{\"id\":%u,\"minutes\":9}", alarm);
    CHECK(BY_HAND("alarm.snooze", args) && g_rc.status == INK_RC_DONE, "person snoozes alarm");
    CHECK(WAIT_FOR(!ink_ui_overlay_up(), 2000), "ringing overlay closed");
    CHECK(AI("system.undo", undo) && g_rc.status == INK_RC_STALE, "old inverse preserves snoozed alarm");
    CHECK(AI("alarm.list", NULL) && strstr(g_result, "SYNTH_SNOOZED"), "person's snoozed alarm remains");

    target = ink_time_now_s() / 60 * 60 + 120;
    ink_time_set(target - 2, false);
    ink_time_to_local(target, &local);
    snprintf(args, sizeof args, "{\"at\":\"%04d-%02d-%02d %02d:%02d\",\"repeat\":\"daily\",\"title\":\"SYNTH_REPEATED\"}", local.year, local.month, local.day, local.hour, local.min);
    double fired = sim_metric_value("time.reminders_fired");
    CHECK(AI("reminder.add", args) && g_rc.status == INK_RC_DONE, "create soon repeating reminder");
    snprintf(undo, sizeof undo, "{\"object\":\"%s\",\"revision\":%u}", g_rc.object, g_rc.revision);
    CHECK(WAIT_FOR(sim_metric_value("time.reminders_fired") > fired, 4000), "reminder actually fires");
    CHECK(AI("system.undo", undo) && g_rc.status == INK_RC_STALE, "old inverse preserves rescheduled reminder");
    CHECK(AI("reminder.list", NULL) && strstr(g_result, "SYNTH_REPEATED"), "repeating reminder remains");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "review3-schedule", .fn = run, .stack_bytes = 16384,
        .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "test task");
}
