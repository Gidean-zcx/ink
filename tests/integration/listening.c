/*
 * Hands-free listening (D-52), on the whole product with the voice front end modelled: the three settings of voice.handsfree,
 * what listening while locked costs (the chip stays up), and who the wake word gets through to. The front end's words and timing
 * are estimates (detailed design §13). Results are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/sys/confirm.h"
#include "ink/sys/hub.h"
#include "ink/sys/settings.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

enum { SCREEN_ON = 0, ALWAYS = 1, OFF = 2 };

static void mic_event(void *arg) { sim_audio_mic_source(arg); }

static void mic(const char *what) {
    vtk_event_at(vtk_now() + SIM_MS(1), mic_event, (void *)what);
    delay_ms(5);
}

static bool listening(void) {
    int64_t v = 0;
    ink_state_get_int(ink_state_find("audio.listening"), &v);
    return v != 0;
}

static void set_mode(int mode) {
    ink_receipt_t rc;
    CHECK(ink_setting_set("voice.handsfree", mode, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "setting %d (%s)", mode, rc.detail);
    delay_ms(100);
}

/* The wake word, a question and the end of it. */
static void say_something(void) {
    mic("wake");
    delay_ms(700);
    mic("speech");
    delay_ms(1500);
    mic("silence");
    delay_ms(1500);
}

static const char *mic_state(void) {
    static char s[16];
    s[0] = 0;
    ink_state_get_str(ink_state_find("audio.mic"), s, sizeof s);
    return s;
}

static double wakes(void) { return sim_metric_value("ai.wakes"); }
static double questions(void) { return sim_metric_value("hub.requests.ai.listen"); }

static void lock(void) {
    ink_power_lock();
    CHECK(WAIT_FOR(!strcmp(power_state(), "locked"), 2000), "locked (%s)", power_state());
}

static void unlock(void) {
    ink_power_unlock(osal_now_us());
    CHECK(WAIT_FOR(strcmp(power_state(), "locked") != 0, 70000), "unlocked (%s)", power_state());
    ink_power_activity(osal_now_us());
}

static volatile int g_pin_done, g_pin_result;
static void pin_told(void *ctx, int result, uint32_t wait_s) {
    (void)ctx;
    (void)wait_s;
    g_pin_result = result;
    g_pin_done++;
}

static void test_task(void *arg) {
    (void)arg;
    delay_ms(1500);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");

    /* by default: while the screen is on and unlocked */
    CHECK(listening(), "it listens from the start");
    CHECK(!strcmp(mic_state(), "local"), "on the device only, the microphone says (%s)", mic_state());
    double was = wakes(), asked = questions();
    say_something();
    CHECK(wakes() == was + 1 && questions() >= asked + 1, "the wake word starts a turn (%.0f, %.0f)", wakes() - was, questions() - asked);
    delay_ms(12000);
    lock();
    CHECK(WAIT_FOR(!listening(), 2000), "locked: it stops");
    int sleeps = sim_power_sleeps();
    delay_ms(70000);
    CHECK(sim_power_sleeps() > sleeps, "and the chip sleeps");
    unlock();
    CHECK(WAIT_FOR(listening(), 2000), "and listens again when unlocked");

    /* always: the chip stays up for it */
    set_mode(ALWAYS);
    lock();
    delay_ms(2000);
    CHECK(listening(), "always: locked too");
    sleeps = sim_power_sleeps();
    double tries = sim_metric_value("power.sleep_refused");
    delay_ms(70000);
    CHECK(sim_power_sleeps() == sleeps && !strcmp(power_state(), "locked"), "the chip stays up, the screen locked (%d, %s)", sim_power_sleeps() - sleeps, power_state());
    CHECK(sim_metric_value("power.sleep_refused") == tries, "because it was asked to, not because it keeps trying and failing (%.0f)", sim_metric_value("power.sleep_refused") - tries);
    was = wakes();
    asked = questions();
    say_something();
    CHECK(wakes() == was + 1 && questions() >= asked + 1, "the wake word works on a locked device (%.0f, %.0f)", wakes() - was, questions() - asked);
    delay_ms(12000);
    CHECK(listening(), "and it goes on listening");
    unlock();

    /* off: nothing listens, nothing wakes */
    set_mode(OFF);
    CHECK(WAIT_FOR(!listening(), 2000), "off: not even unlocked");
    CHECK(WAIT_FOR(!strcmp(mic_state(), "off"), 200), "the microphone is off (%s)", mic_state());
    was = wakes();
    say_something();
    CHECK(wakes() == was, "the wake word means nothing (%.0f)", wakes() - was);
    set_mode(ALWAYS);
    CHECK(WAIT_FOR(listening(), 2000), "and on again");

    /* it does not get past a card that is asking something */
    CHECK(AI("notify.clear", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "a card asks");
    unsigned token = 0;
    sscanf(g_rc.object, "confirm:%u", &token);
    delay_ms(1000);
    was = wakes();
    asked = questions();
    double refused = sim_metric_value("ai.wake_not_now");
    say_something();
    CHECK(wakes() == was && questions() == asked && sim_metric_value("ai.wake_not_now") > refused, "the wake word waits for it (%.0f, %.0f)", wakes() - was, questions() - asked);
    CHECK(ink_confirm_decide(token, false, INK_SRC_TOUCH) == 0, "answered");
    delay_ms(1500);
    was = wakes();
    say_something();
    CHECK(wakes() == was + 1, "then it works again");
    delay_ms(12000);

    /* nor into a space that is closed: an adult's, with a PIN, locked. What was said goes nowhere */
    int d = g_pin_done;
    CHECK(ink_member_set_pin("owner", NULL, "1234", pin_told, NULL) == 0 && WAIT_FOR(g_pin_done > d, 5000) && g_pin_result == INK_PIN_OK, "a PIN");
    lock();
    CHECK(WAIT_FOR(!ink_member_space_open(), 2000), "the space is closed");
    delay_ms(2000);
    CHECK(listening(), "it listens");
    was = wakes();
    asked = questions();
    say_something();
    CHECK(wakes() == was && questions() == asked, "and nobody gets to the owner's AI (%.0f, %.0f)", wakes() - was, questions() - asked);
    unlock();
    d = g_pin_done;
    CHECK(ink_member_unlock("owner", "1234", pin_told, NULL) == 0 && WAIT_FOR(g_pin_done > d, 5000) && g_pin_result == INK_PIN_OK, "the PIN");
    delay_ms(1500);
    was = wakes();
    say_something();
    CHECK(wakes() == was + 1, "with it, the wake word works");
    delay_ms(40000); /* the answer is over */

    /* the space closes in the middle of a turn (AUD16-008): what the microphone hears from then on goes nowhere */
    was = wakes();
    mic("wake");
    delay_ms(700);
    mic("speech");
    delay_ms(1500);
    CHECK(wakes() == was + 1, "someone is asking something (%.0f, %.0f barge, %.0f not now)", wakes() - was, sim_metric_value("ai.barge_ins"), sim_metric_value("ai.wake_not_now"));
    CHECK(!strcmp(mic_state(), "sending"), "the voice goes out, the microphone says (%s)", mic_state());
    double closed = sim_metric_value("ai.closed_with_the_space");
    double sent = sim_metric_value("hub.binary_bytes");
    delay_ms(500);
    CHECK(sim_metric_value("hub.binary_bytes") > sent, "and it goes to the hub");
    lock();
    CHECK(WAIT_FOR(!ink_member_space_open(), 2000), "the space is closed");
    CHECK(WAIT_FOR(sim_metric_value("ai.closed_with_the_space") == closed + 1, 1000), "the turn is over");
    delay_ms(1000); /* what was on its way */
    sent = sim_metric_value("hub.binary_bytes");
    delay_ms(2200);
    CHECK(sim_metric_value("hub.binary_bytes") == sent, "and nothing more goes out (%.0f bytes)", sim_metric_value("hub.binary_bytes") - sent);
    CHECK(!strcmp(mic_state(), "local"), "the microphone says so too (%s)", mic_state());
    mic("silence");
    delay_ms(500);

    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    osal_task_cfg_t cfg = {.name = "listentest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
