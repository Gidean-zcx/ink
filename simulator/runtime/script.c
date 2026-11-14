/*
 * Scenario scripts: timed physical actions on the simulated device.
 *
 *   # comment
 *   @0       note  boot
 *   +1500ms  tap   340 600            # logical (UI) coordinates, optional hold
 *   +1s      swipe 600 700 80 700 250ms
 *   +800ms   key   next               # optional hold, default 80ms
 *   +2s      snap  after-page-turn    # PNG of the physical panel
 *   +0       mark  reading            # trace marker
 *   +0       battery 15 [charging]
 *   +3s      powercut                 # tear in-flight flash op, save image, stop
 *   +3s      powercut write 2         # cut during the 2nd flash program/erase from now
 *   +1s      mic   speech|silence|noise|wake  # what the microphone hears (wake: the wake word, said like speech)
 *   +0       mic   say 下一页         # speech with these words (the device's command words may know them)
 *   +0       net   wifi | lte | none   # shorthand for the environment: that network appears (none: all gone)
 *   +0       wifi  on|off [rssi]        # a known Wi-Fi network in range, e.g. wifi on -70
 *   +0       wifi  broken               # in range, but joining it never works (a changed password)
 *   +0       cell  on|off [rsrp]        # cellular coverage, e.g. cell on -105
 *   +0       outage 5s                 # link drops for a while (dead connection noticed later)
 *   +0       stall 600ms               # the downlink delivers nothing for a while, then all of it at once (a TCP
 *                                      # retransmission, a radio pause); the connection stays up
 *   +0       sim   external|internal none|data|sms|voice  # card in a SIM slot (what its plan allows)
 *   +0       orient portrait|upside_down|landscape_left|landscape_right|face_up|face_down|reading
 *   +0       shake                    # a movement the accelerometer notices
 *   +0       rtc lost                 # the RTC's backup supply failed: its time is invalid
 *   +0       rtc line stuck           # the RTC's interrupt line held at its active level, so it cannot signal
 *   +0       hub-clock -3600          # the hub's clock is this many seconds from the true time
 *   +0       sms-in 10086 text...     # a text message arrives (needs a card with SMS)
 *   +0       call-in 13800000000      # an incoming call (needs a card with voice)
 *   +0       remote answer|hangup     # the far end of a call
 *   +0       sd remove|insert|full|free N  # the TF card: taken out, put in, full, N KiB left
 *   +0       usb plug|unplug|eject    # a computer at the other end of the cable; it ejects the USB disk
 *   +0       usb copy-in HOSTFILE PATH  # the computer copies a file onto the USB disk (PATH on the card, from its root)
 *   +0       ap add NAME -60 [password] # a Wi-Fi network appears (known or not)
 *   +0       ap del NAME               # it goes away
 *   +0       headset add AA:BB name    # a Bluetooth headset appears, on and in range
 *   +0       headset on|off AA:BB      # it is switched on or off
 *   +0       headset in|out AA:BB      # it comes into range or goes out of it
 *   +0       faults io-expander 1.0   # that chip's I2C transfers time out at this rate; "faults off" ends it
 *   +5s      end
 *
 * Times: "@t" absolute, "+t" relative to the previous line; units us/ms/s/m
 * (default ms). Other commands: press X Y / move X Y / release, longpress X Y.
 * Coordinates: logical pixels ("340"), density-independent "116dp" (1/160 in),
 * or a share of the logical screen "50%" — use dp/% so one script runs on
 * panels of any size and density.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/rotation.h"
#include "sim.h"

#include "script.h"
/* Some simulator executables exercise only HALs, without the capability bus. */
__attribute__((weak)) const ink_cap_t *ink_cap_find(const char *id) { (void)id; return NULL; }
__attribute__((weak)) ink_rc_t ink_cap_invoke_json(const char *id, const char *json, ink_source_t src, uint32_t gen, ink_receipt_t *rc) {
    (void)id; (void)json; (void)src; (void)gen; return rc->status = INK_RC_INVALID;
}
__attribute__((weak)) int ink_exec_post(ink_exec_t e, ink_exec_job_t *job, void (*fn)(void *), void *arg) {
    (void)e; (void)job; (void)fn; (void)arg; return -1;
}
static ink_script_t script;

int sim_script_load(const char *path, char *err, size_t errlen) {
    FILE *f = fopen(path, "r");
    if (!f) { snprintf(err, errlen, "cannot open script %s", path); return -1; }
    ink_logical_size(g_profile.panel.width, g_profile.panel.height, g_profile.panel.rotation_deg, &script.width, &script.height);
    script.dpi = g_profile.panel.dpi;
    script.resize = realloc;
    char line[512];
    int lineno = 0, result = 0;
    while (fgets(line, sizeof line, f)) {
        char reason[96] = "bad cap arguments";
        if ((result = ink_script_line(&script, line, ++lineno, reason, sizeof reason)) != 0) {
            snprintf(err, errlen, "%s:%d: %s", path, lineno, reason);
            break;
        }
    }
    if (ferror(f)) result = -1;
    fclose(f);
    return result;
}
static void run_cap(void *arg) {
    action_t *a = arg;
    const ink_cap_t *c = ink_cap_find(a->text);
    if (!ink_script_cap_allowed(c)) {
        sim_note("autotest: %d cap rejected source=test", a->line);
        return;
    }
    ink_receipt_t rc;
    ink_rc_t result = ink_cap_invoke_json(a->text, a->path, INK_SRC_APP, 0, &rc);
    sim_note("autotest: %d cap source=test result=%d", a->line, result);
}

static int last_x, last_y;

static void run_action(void *arg) { /* ISR context */
    action_t *a = arg;
    switch (a->kind) {
    case A_CAP:
        break;
    case A_TOUCH:
        if (a->a == 0) {
            last_x = a->b;
            last_y = a->c;
            sim_input_touch(0, true, a->b, a->c);
        } else {
            sim_input_touch(0, false, a->b >= 0 ? a->b : last_x, a->c >= 0 ? a->c : last_y);
        }
        break;
    case A_KEY: sim_input_key((hal_key_t)a->a, a->b != 0); break;
    case A_SNAP: sim_display_snapshot(a->text); break;
    case A_MARK: sim_trace_instant("marks", a->text, vtk_isr_now(), NULL); break;
    case A_NOTE: sim_note("script: %s", a->text); break;
    case A_BATTERY: sim_power_set_battery(a->a, a->b != 0); break;
    case A_POWERCUT:
        if (a->c > 0) sim_storage_arm_power_cut(a->c);
        else sim_storage_power_cut();
        break;
    case A_END: vtk_stop(); break;
    case A_NET: sim_net_set_link(a->text); break;
    case A_OUTAGE: sim_net_outage(SIM_MS(a->c)); break;
    case A_STALL: sim_net_stall(SIM_MS(a->c)); break;
    case A_MIC: sim_audio_mic_source(a->text); break;
    case A_MIC_SAY: sim_audio_mic_say(a->text); break;
    case A_SIM: sim_net_set_card(a->a, a->b > 0, a->b >= 2, a->b == 3); break;
    case A_ORIENT: sim_accel_orient(a->text); break;
    case A_SHAKE: sim_accel_shake(); break;
    case A_RTC_LOST: sim_rtc_lose_time(); break;
    case A_RTC_LINE: sim_rtc_line_stuck(); break;
    case A_HUB_CLOCK: sim_hub_clock_offset(a->c); break;
    case A_AP:
        if (a->a) sim_wifi_ap_add(a->num, a->b, a->text);
        else sim_wifi_ap_del(a->num);
        break;
    case A_HEADSET:
        if (a->a == 0) sim_headset_add(a->num, a->text);
        else if (a->a <= 2) sim_headset_power(a->num, a->a == 1);
        else sim_headset_range(a->num, a->a == 3);
        break;
    case A_SMS_IN:
        if (!sim_net_deliver_sms(a->num, a->text)) sim_note("script: sms to the device not delivered (no SMS service)");
        break;
    case A_CALL_IN:
        if (!sim_net_incoming_call(a->num)) sim_note("script: call to the device not put through");
        break;
    case A_ENV: sim_net_env(a->a, a->b != 0, a->c); break;
    case A_FAULTS: sim_i2c_set_faults(a->c / 1000.0, a->text[0] ? a->text : NULL); break;
    case A_REMOTE:
        if (a->a) sim_net_remote_answer();
        else sim_net_remote_hangup();
        break;
    case A_SD:
        if (a->a <= 1) sim_sd_set_present(a->a == 1);
        else sim_sd_set_free(a->a == 2 ? 0 : (int64_t)a->c * 1024);
        break;
    case A_USB:
        if (a->a <= 1) sim_usb_plug(a->a == 0);
        else if (a->a == 2) sim_usb_eject();
        else sim_usb_copy_in(a->path, a->text);
        break;
    }
}

/* Capability calls must leave ISR context; only scripts using cap need this short-lived runner. */
static void cap_task(void *unused) {
    (void)unused;
    for (;;) {
        action_t *a = NULL;
        for (size_t i = 0; i < script.count; i++)
            if (script.actions[i].kind == A_CAP && !script.actions[i].c && (!a || script.actions[i].t < a->t)) a = &script.actions[i];
        if (!a) return;
        if (a->t > vtk_now()) vtk_sleep_ns(a->t - vtk_now());
        a->c = 1;
        if (ink_exec_post(INK_EXEC_UI, &a->job, run_cap, a)) sim_note("autotest: %d cap skipped", a->line);
    }
}
void sim_script_start(void) {
    bool caps = false;
    for (size_t i = 0; i < script.count; i++) {
        if (script.actions[i].kind == A_CAP) caps = true;
        else vtk_event_at(script.actions[i].t, run_action, &script.actions[i]);
    }
    if (caps && !vtk_task_create("script-cap", cap_task, NULL, OSAL_PRIO_LOW, -1, 0, 4096)) abort();
}

bool sim_script_has_end(void) { return script.has_end; }
