#include "product_probe.h"
static volatile int pin_done, pin_result;
static volatile uint32_t pin_wait;
static bool cut_at_reply;
static void power_cut(void *arg) { (void)arg; sim_storage_power_cut(); }
static void told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx;
    pin_result = result; pin_wait = wait_s; pin_done = 1;
    if (cut_at_reply) {
        printf("AUDIT PIN before_cut result=%d wait_s=%u\n", result, wait_s);
        CHECK(result == INK_PIN_WRONG && wait_s > 0, "wait applies before power cut");
        ink_probe_count("test.finished");
        vtk_event_at(vtk_now() + SIM_US(1), power_cut, NULL);
    }
}
static void attempt(const char *pin) {
    pin_done = 0;
    CHECK(ink_member_unlock("owner", pin, told, NULL) == 0 && WAIT_FOR(pin_done, 5000), "PIN attempt completed");
}
static void durable(void) {
    ink_db_flush_all();
    CHECK(WAIT_FOR(ink_db_idle() && !ink_storage_pending(), 6000), "setup durable");
}
static void audit_task(void *arg) {
    (void)arg;
    delay_ms(1800);
    if (getenv("AUDIT_PIN_CUT")) {
        CHECK(ink_member_set_pin("owner", NULL, "123456", told, NULL) == 0 && WAIT_FOR(pin_done, 5000), "fixture PIN");
        durable();
        attempt("000000"); attempt("000000"); durable();
        cut_at_reply = true;
        attempt("000000");
    } else {
        CHECK(ink_member_has_pin("owner") && !ink_member_space_open(), "protected before attempting");
        attempt("123456");
        printf("AUDIT PIN after_cut result=%d wait_s=%u space_open=%d\n", pin_result, pin_wait, ink_member_space_open());
        CHECK(pin_result == INK_PIN_OK && ink_member_space_open(), "reproduce lost wait after power cut");
        ink_probe_count("test.finished");
    }
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c = {.name="audit-pin-cut", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c, NULL);
}
