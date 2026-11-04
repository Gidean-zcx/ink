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
 *   +0       sim   external|internal none|data|sms|voice  # card in a SIM slot (what its plan allows)
 *   +0       orient portrait|upside_down|landscape_left|landscape_right|face_up|face_down
 *   +0       shake                    # a movement the accelerometer notices
 *   +0       rtc lost                 # the RTC's backup supply failed: its time is invalid
 *   +0       rtc line low             # the RTC's interrupt line reads low all the time (as in the factory log)
 *   +0       hub-clock -3600          # the hub's clock is this many seconds from the true time
 *   +0       sms-in 10086 text...     # a text message arrives (needs a card with SMS)
 *   +0       call-in 13800000000      # an incoming call (needs a card with voice)
 *   +0       remote answer|hangup     # the far end of a call
 *   +0       sd remove|insert|full|free N  # the TF card: taken out, put in, full, N KiB left
 *   +0       usb plug|unplug|eject    # a computer at the other end of the cable; it ejects the USB disk
 *   +0       usb copy-in HOSTFILE PATH  # the computer copies a file onto the USB disk (PATH on the card, from its root)
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

typedef enum { A_TOUCH, A_KEY, A_SNAP, A_MARK, A_NOTE, A_BATTERY, A_POWERCUT, A_END, A_NET, A_OUTAGE, A_MIC, A_MIC_SAY, A_SIM, A_ORIENT, A_SHAKE, A_RTC_LOST, A_SMS_IN, A_CALL_IN, A_REMOTE, A_ENV, A_FAULTS, A_SD, A_USB, A_HUB_CLOCK, A_RTC_LINE } akind_t;

typedef struct {
    uint64_t t;
    akind_t kind;
    int a, b, c;
    char text[96];
    char num[24];
    char path[200];
} action_t;

static action_t *A;
static size_t nA, capA;
static bool has_end;
static uint64_t end_t;

static action_t *push(uint64_t t, akind_t k) {
    if (nA == capA) {
        capA = capA ? capA * 2 : 64;
        A = realloc(A, capA * sizeof *A);
        if (!A) abort();
    }
    action_t *x = &A[nA++];
    memset(x, 0, sizeof *x);
    x->t = t;
    x->kind = k;
    return x;
}

static int parse_dur(const char *s, uint64_t *out) {
    char *end;
    double v = strtod(s, &end);
    if (end == s) return -1;
    double mul = 1e6; /* ms */
    if (!strcmp(end, "us")) mul = 1e3;
    else if (!strcmp(end, "ms") || !*end) mul = 1e6;
    else if (!strcmp(end, "s")) mul = 1e9;
    else if (!strcmp(end, "m")) mul = 60e9;
    else return -1;
    *out = (uint64_t)(v * mul);
    return 0;
}

/* coordinate with unit: px (default), dp, or % of the logical axis */
static int coord(const char *s, bool is_x) {
    char *end;
    double v = strtod(s, &end);
    int lw, lh;
    ink_logical_size(g_profile.panel.width, g_profile.panel.height, g_profile.panel.rotation_deg, &lw, &lh);
    if (!strcmp(end, "dp")) return (int)(v * g_profile.panel.dpi / 160.0 + 0.5);
    if (!strcmp(end, "%")) return (int)(v / 100.0 * (is_x ? lw : lh) + 0.5);
    return (int)v;
}

static hal_key_t key_named(const char *s) {
    static const struct { const char *n; hal_key_t k; } M[] = {
        {"ai", HAL_KEY_AI}, {"power", HAL_KEY_POWER}, {"vol_up", HAL_KEY_VOL_UP}, {"vol_down", HAL_KEY_VOL_DOWN},
        {"home", HAL_KEY_HOME}, {"prev", HAL_KEY_PREV}, {"next", HAL_KEY_NEXT}, {"back", HAL_KEY_BACK},
    };
    for (size_t i = 0; i < sizeof M / sizeof M[0]; i++)
        if (!strcmp(M[i].n, s)) return M[i].k;
    return HAL_KEY_NONE;
}

static void touch(uint64_t t, int phase, int x, int y) {
    action_t *a = push(t, A_TOUCH);
    a->a = phase; /* 0 down/move, 1 up */
    a->b = x;
    a->c = y;
}

int sim_script_load(const char *path, char *err, size_t errlen) {
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(err, errlen, "cannot open script %s", path);
        return -1;
    }
    char line[512];
    int lineno = 0;
    uint64_t prev = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char *tok[12];
        int n = 0;
        for (char *p = strtok(line, " \t\r\n"); p && n < 12; p = strtok(NULL, " \t\r\n")) tok[n++] = p;
        if (!n) continue;
        uint64_t t, d;
        if (tok[0][0] == '@' && !parse_dur(tok[0] + 1, &d)) t = d;
        else if (tok[0][0] == '+' && !parse_dur(tok[0] + 1, &d)) t = prev + d;
        else {
            snprintf(err, errlen, "%s:%d: time must look like @1s or +200ms", path, lineno);
            fclose(f);
            return -1;
        }
        prev = t;
        if (n < 2) {
            snprintf(err, errlen, "%s:%d: missing command", path, lineno);
            fclose(f);
            return -1;
        }
        const char *cmd = tok[1];
        bool ok = true;
        if (!strcmp(cmd, "tap") || !strcmp(cmd, "longpress")) {
            uint64_t hold = !strcmp(cmd, "tap") ? SIM_MS(80) : SIM_MS(800);
            ok = n >= 4 && (n < 5 || !parse_dur(tok[4], &hold));
            if (ok) {
                touch(t, 0, coord(tok[2], true), coord(tok[3], false));
                touch(t + hold, 1, coord(tok[2], true), coord(tok[3], false));
            }
        } else if (!strcmp(cmd, "press") || !strcmp(cmd, "move")) {
            ok = n >= 4;
            if (ok) touch(t, 0, coord(tok[2], true), coord(tok[3], false));
        } else if (!strcmp(cmd, "release")) {
            touch(t, 1, -1, -1);
        } else if (!strcmp(cmd, "swipe")) {
            uint64_t dur = SIM_MS(300);
            ok = n >= 6 && (n < 7 || !parse_dur(tok[6], &dur));
            if (ok) {
                int x1 = coord(tok[2], true), y1 = coord(tok[3], false), x2 = coord(tok[4], true), y2 = coord(tok[5], false);
                int steps = (int)(dur / SIM_MS(8));
                if (steps < 2) steps = 2;
                for (int i = 0; i <= steps; i++) {
                    double frac = (double)i / steps;
                    touch(t + (uint64_t)(frac * (double)dur), 0, x1 + (int)(frac * (x2 - x1)), y1 + (int)(frac * (y2 - y1)));
                }
                touch(t + dur + SIM_MS(4), 1, x2, y2);
            }
        } else if (!strcmp(cmd, "key")) {
            uint64_t hold = SIM_MS(80);
            hal_key_t k = n >= 3 ? key_named(tok[2]) : HAL_KEY_NONE;
            ok = k != HAL_KEY_NONE && (n < 4 || !parse_dur(tok[3], &hold));
            if (ok) {
                action_t *a = push(t, A_KEY);
                a->a = k;
                a->b = 1;
                a = push(t + hold, A_KEY);
                a->a = k;
                a->b = 0;
            }
        } else if (!strcmp(cmd, "snap") || !strcmp(cmd, "mark") || !strcmp(cmd, "note")) {
            ok = n >= 3;
            if (ok) {
                action_t *a = push(t, !strcmp(cmd, "snap") ? A_SNAP : !strcmp(cmd, "mark") ? A_MARK : A_NOTE);
                a->text[0] = 0;
                for (int i = 2; i < n; i++) {
                    if (i > 2) strncat(a->text, " ", sizeof a->text - strlen(a->text) - 1);
                    strncat(a->text, tok[i], sizeof a->text - strlen(a->text) - 1);
                }
            }
        } else if (!strcmp(cmd, "battery")) {
            ok = n >= 3;
            if (ok) {
                action_t *a = push(t, A_BATTERY);
                a->a = atoi(tok[2]);
                a->b = n >= 4 && !strcmp(tok[3], "charging");
            }
        } else if (!strcmp(cmd, "net")) {
            ok = n >= 3 && (!strcmp(tok[2], "lte") || !strcmp(tok[2], "wifi") || !strcmp(tok[2], "none"));
            if (ok) snprintf(push(t, A_NET)->text, 16, "%s", tok[2]);
        } else if (!strcmp(cmd, "mic") && n >= 4 && !strcmp(tok[2], "say")) { /* mic say <words>: speech the device may recognise */
            ok = true;
            snprintf(push(t, A_MIC_SAY)->text, 96, "%s", tok[3]);
        } else if (!strcmp(cmd, "mic")) {
            ok = n >= 3 && (!strcmp(tok[2], "speech") || !strcmp(tok[2], "silence") || !strcmp(tok[2], "noise") || !strcmp(tok[2], "wake"));
            if (ok) snprintf(push(t, A_MIC)->text, 16, "%s", tok[2]);
        } else if (!strcmp(cmd, "sim")) {
            static const char *const kinds[] = {"none", "data", "sms", "voice"};
            int slot = n >= 3 ? (!strcmp(tok[2], "external") ? 0 : !strcmp(tok[2], "internal") ? 1 : -1) : -1, kind = -1;
            for (int k = 0; k < 4 && n >= 4; k++)
                if (!strcmp(tok[3], kinds[k])) kind = k;
            ok = slot >= 0 && kind >= 0;
            if (ok) {
                action_t *a = push(t, A_SIM);
                a->a = slot;
                a->b = kind;
            }
        } else if (!strcmp(cmd, "orient")) {
            static const char *const o[] = {"portrait", "upside_down", "landscape_left", "landscape_right", "face_up", "face_down"};
            ok = false;
            for (int k = 0; k < 6 && n >= 3; k++)
                if (!strcmp(tok[2], o[k])) ok = true;
            if (ok) snprintf(push(t, A_ORIENT)->text, 16, "%s", tok[2]);
        } else if (!strcmp(cmd, "shake")) {
            push(t, A_SHAKE);
        } else if (!strcmp(cmd, "rtc")) {
            ok = n >= 3 && (!strcmp(tok[2], "lost") || (!strcmp(tok[2], "line") && n >= 4 && !strcmp(tok[3], "low")));
            if (ok) push(t, !strcmp(tok[2], "lost") ? A_RTC_LOST : A_RTC_LINE);
        } else if (!strcmp(cmd, "hub-clock")) {
            ok = n >= 3;
            if (ok) push(t, A_HUB_CLOCK)->c = atoi(tok[2]);
        } else if (!strcmp(cmd, "wifi") || !strcmp(cmd, "cell")) {
            bool broken = !strcmp(cmd, "wifi") && n >= 3 && !strcmp(tok[2], "broken");
            ok = n >= 3 && (!strcmp(tok[2], "on") || !strcmp(tok[2], "off") || broken);
            if (ok) {
                action_t *a = push(t, A_ENV);
                a->a = broken ? 2 : !strcmp(cmd, "cell");
                a->b = broken || !strcmp(tok[2], "on");
                a->c = n >= 4 ? atoi(tok[3]) : 0;
            }
        } else if (!strcmp(cmd, "sms-in")) {
            ok = n >= 4;
            if (ok) {
                action_t *a = push(t, A_SMS_IN);
                snprintf(a->num, sizeof a->num, "%s", tok[2]);
                a->text[0] = 0;
                for (int k = 3; k < n; k++) {
                    size_t l = strlen(a->text);
                    snprintf(a->text + l, sizeof a->text - l, "%s%s", k > 3 ? " " : "", tok[k]);
                }
            }
        } else if (!strcmp(cmd, "call-in")) {
            ok = n >= 3;
            if (ok) snprintf(push(t, A_CALL_IN)->num, 24, "%s", tok[2]);
        } else if (!strcmp(cmd, "remote")) {
            ok = n >= 3 && (!strcmp(tok[2], "answer") || !strcmp(tok[2], "hangup"));
            if (ok) push(t, A_REMOTE)->a = !strcmp(tok[2], "answer");
        } else if (!strcmp(cmd, "outage")) {
            uint64_t dur;
            ok = n >= 3 && !parse_dur(tok[2], &dur);
            if (ok) push(t, A_OUTAGE)->c = (int)(dur / 1000000ull); /* ms */
        } else if (!strcmp(cmd, "faults")) {
            ok = n >= 3 && (!strcmp(tok[2], "off") || (n >= 4 && atof(tok[3]) >= 0 && atof(tok[3]) <= 1));
            if (ok) {
                action_t *a = push(t, A_FAULTS);
                if (strcmp(tok[2], "off")) {
                    snprintf(a->text, sizeof a->text, "%s", tok[2]);
                    a->c = (int)(atof(tok[3]) * 1000 + 0.5); /* per mille */
                }
            }
        } else if (!strcmp(cmd, "sd")) {
            ok = n >= 3 && (!strcmp(tok[2], "remove") || !strcmp(tok[2], "insert") || !strcmp(tok[2], "full") || (!strcmp(tok[2], "free") && n >= 4));
            if (ok) {
                action_t *a = push(t, A_SD);
                a->a = !strcmp(tok[2], "remove") ? 0 : !strcmp(tok[2], "insert") ? 1 : !strcmp(tok[2], "full") ? 2 : 3;
                a->c = n >= 4 ? atoi(tok[3]) : 0;
            }
        } else if (!strcmp(cmd, "usb")) {
            ok = n >= 3 && (!strcmp(tok[2], "plug") || !strcmp(tok[2], "unplug") || !strcmp(tok[2], "eject") || (!strcmp(tok[2], "copy-in") && n >= 5));
            if (ok) {
                action_t *a = push(t, A_USB);
                a->a = !strcmp(tok[2], "plug") ? 0 : !strcmp(tok[2], "unplug") ? 1 : !strcmp(tok[2], "eject") ? 2 : 3;
                if (a->a == 3) {
                    snprintf(a->path, sizeof a->path, "%s", tok[3]);
                    snprintf(a->text, sizeof a->text, "%s", tok[4]);
                }
            }
        } else if (!strcmp(cmd, "powercut")) {
            action_t *a = push(t, A_POWERCUT);
            a->c = (n >= 3 && !strcmp(tok[2], "write")) ? (n >= 4 ? atoi(tok[3]) : 1) : 0;
        } else if (!strcmp(cmd, "end")) {
            push(t, A_END);
            has_end = true;
            end_t = t;
        } else {
            ok = false;
        }
        if (!ok) {
            snprintf(err, errlen, "%s:%d: bad command or arguments: %s", path, lineno, cmd);
            fclose(f);
            return -1;
        }
    }
    fclose(f);
    return 0;
}

static int last_x, last_y;

static void run_action(void *arg) { /* ISR context */
    action_t *a = arg;
    switch (a->kind) {
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
    case A_MIC: sim_audio_mic_source(a->text); break;
    case A_MIC_SAY: sim_audio_mic_say(a->text); break;
    case A_SIM: sim_net_set_card(a->a, a->b > 0, a->b >= 2, a->b == 3); break;
    case A_ORIENT: sim_accel_orient(a->text); break;
    case A_SHAKE: sim_accel_shake(); break;
    case A_RTC_LOST: sim_rtc_lose_time(); break;
    case A_RTC_LINE: sim_rtc_line_stuck_low(); break;
    case A_HUB_CLOCK: sim_hub_clock_offset(a->c); break;
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

void sim_script_start(void) {
    for (size_t i = 0; i < nA; i++) vtk_event_at(A[i].t, run_action, &A[i]);
}

bool sim_script_has_end(void) { return has_end; }
