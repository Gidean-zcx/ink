#include "product_probe.h"
#include "ink/sys/hub.h"
#include "ink/sys/settings.h"
static volatile int pin_done, pin_result;
static void told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx; (void)wait_s;
    pin_result = result; pin_done = 1;
}
static void mic_event(void *arg) { sim_audio_mic_source(arg); }
static void mic(const char *what) {
    vtk_event_at(vtk_now() + SIM_MS(1), mic_event, (void *)what);
    delay_ms(5);
}
static void audit_task(void *arg) {
    (void)arg;
    delay_ms(1800);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "hub online");
    CHECK(ink_member_set_pin("owner", NULL, "1234", told, NULL) == 0 && WAIT_FOR(pin_done, 5000) && pin_result == INK_PIN_OK, "synthetic PIN configured");
    ink_receipt_t rc;
    CHECK(ink_setting_set("voice.handsfree", 1, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "always listen");
    delay_ms(500);
    mic("wake"); delay_ms(700); mic("speech"); delay_ms(1500);
    CHECK(sim_metric_value("ai.wakes") > 0, "a normal authorized wake");
    ink_power_lock();
    CHECK(WAIT_FOR(!ink_member_space_open(), 2000), "adult space closed");
    delay_ms(1000); /* Let any in-flight audio/transport frames drain. */
    double before = sim_metric_value("hub.binary_bytes");
    delay_ms(2200);
    double after = sim_metric_value("hub.binary_bytes");
    printf("AUDIT listen space_open=%d upload_bytes_while_closed=%.0f\n", ink_member_space_open(), after - before);
    CHECK(after > before, "reproduce continued microphone upload after adult space closed");
    mic("silence");
    delay_ms(1800);
    ink_probe_count("test.finished");
}
void ink_main(void) {
    product_main();
    osal_task_cfg_t c = {.name="audit-listen", .fn=audit_task, .stack_bytes=16384, .priority=OSAL_PRIO_NORMAL, .core=-1, .flags=OSAL_TASK_STACK_EXTERNAL};
    osal_task_create(&c, NULL);
}
