/*
 * What the tests of the whole product share: results as the probe counters "test.pass" / "test.fail", capability calls,
 * jobs on the UI task, the person's fingers and the world (in event context, as the script does it) and what is on screen.
 * Include it after the sources it needs, in one test program.
 */
#ifndef WHOLE_PRODUCT_H
#define WHOLE_PRODUCT_H

#include <stdio.h>
#include <string.h>

#include "ink/hal_input.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/confirm.h"
#include "ink/sys/exec.h"
#include "ink/sys/json.h"
#include "ink/sys/state.h"
#include "ink/sys/settings.h"
#include "ink/sys/ui.h"
#include "sim.h"

#pragma GCC diagnostic ignored "-Wunused-function" /* not every test uses all of these */

#define CHECK(cond, ...)                                                 \
    do {                                                                 \
        if (cond) {                                                      \
            ink_probe_count("test.pass");                                \
        } else {                                                         \
            char _m[300];                                                \
            snprintf(_m, sizeof _m, __VA_ARGS__);                        \
            sim_warn("CHECK FAILED (%s:%d) %s", __FILE__, __LINE__, _m); \
            ink_probe_count("test.fail");                                \
        }                                                                \
    } while (0)

/* ------------------------------------------------------------ helpers */

static ink_receipt_t g_rc;
static char g_result[4096];
static volatile int g_done;

static void rc_done(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    g_rc = *rc;
    snprintf(g_result, sizeof g_result, "%s", rc->result ? rc->result : "");
    g_rc.result = NULL;
    __atomic_store_n(&g_done, 1, __ATOMIC_RELEASE);
}

static bool call_as(const char *id, const char *args, ink_source_t src) {
    char *bound = NULL;
    if ((src == INK_SRC_AI || src == INK_SRC_REMOTE) && !strncmp(id, "text.", 5) && strcmp(id, "text.state") &&
        (!args || (args[0] == '{' && !strstr(args, "\"session\"")))) {
        /* Ordinary clients read the editing target; explicit stale-target tests keep their own arguments. */
        if (call_as("text.state", NULL, src)) {
            ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
            int64_t session = 0, revision = 0;
            if (j && ink_json_parse(j, g_result, strlen(g_result)) == 0 &&
                ink_json_int(j, ink_json_find(j, 0, "session"), &session) &&
                ink_json_int(j, ink_json_find(j, 0, "revision"), &revision)) {
                size_t size = (args ? strlen(args) : 2) + 128;
                bound = osal_malloc(size, OSAL_MEM_EXTERNAL);
                if (bound) snprintf(bound, size, "{\"session\":%lld,\"revision\":%lld%s%s", (long long)session, (long long)revision,
                                    args && strlen(args) > 2 ? "," : "", args ? args + 1 : "}");
            }
            osal_free(j);
        }
    }
    ink_call_opts_t o = {.src = src, .args_json = bound ? bound : args, .reply_exec = INK_EXEC_DIRECT, .done = rc_done};
    g_done = 0;
    g_result[0] = 0;
    int accepted = ink_cap_call(id, NULL, &o);
    osal_free(bound);
    if (accepted != 0) return false;
    for (int i = 0; i < 2000 && !__atomic_load_n(&g_done, __ATOMIC_ACQUIRE); i++) osal_task_delay_us(5000);
    return g_done != 0;
}
#define AI(id, args) call_as(id, args, INK_SRC_AI)
#define BY_HAND(id, args) call_as(id, args, INK_SRC_TOUCH)

static bool setting_by_hand(const char *id, int64_t value) {
    ink_receipt_t rc;
    if (ink_setting_set(id, value, INK_SRC_TOUCH, &rc) != INK_RC_PENDING_CONFIRMATION) return false;
    unsigned token = 0;
    sscanf(rc.object, "confirm:%u", &token);
    if (ink_confirm_decide(token, true, INK_SRC_TOUCH) != 0) return false;
    for (int i = 0; i < 200 && ink_setting_get(id) != value; i++) osal_task_delay_us(5000);
    return ink_setting_get(id) == value;
}

static void delay_ms(int ms) { osal_task_delay_us((uint64_t)ms * 1000); }

static const char *active_state(void) {
    static char s[16];
    s[0] = 0;
    ink_state_get_str(ink_state_find("interrupt.active"), s, sizeof s);
    return s;
}

static const char *power_state(void) {
    static char s[16];
    s[0] = 0;
    ink_state_get_str(ink_state_find("power.state"), s, sizeof s);
    return s;
}

/* Wait for a condition, in virtual time. */
#define WAIT_FOR(cond, max_ms)                                               \
    ({                                                                       \
        int _w = 0;                                                          \
        while (!(cond) && _w < (max_ms)) {                                   \
            delay_ms(20);                                                    \
            _w += 20;                                                        \
        }                                                                    \
        (bool)(cond);                                                        \
    })

/* Run on the UI task and wait. */
static volatile int g_ui_done;
static ink_exec_job_t g_ui_job;
static void (*g_ui_fn)(void);
static void ui_job(void *arg) {
    (void)arg;
    g_ui_fn();
    __atomic_store_n(&g_ui_done, 1, __ATOMIC_RELEASE);
}
static void on_ui(void (*fn)(void)) {
    g_ui_fn = fn;
    g_ui_done = 0;
    ink_exec_post(INK_EXEC_UI, &g_ui_job, ui_job, NULL);
    for (int i = 0; i < 4000 && !__atomic_load_n(&g_ui_done, __ATOMIC_ACQUIRE); i++) delay_ms(5);
}

/* The fingers and the world, in event context as the script does it. */
typedef enum { HW_PRESS, HW_RELEASE, HW_KEY_DOWN, HW_KEY_UP, HW_CALL, HW_HANGUP, HW_SMS, HW_BATTERY, HW_BATTERY_CHARGING, HW_USB_ON, HW_USB_OFF } hw_kind_t;
static struct {
    hw_kind_t kind;
    int a, b;
    char num[24], text[64];
} g_hw;

static void hw_event(void *arg) {
    (void)arg;
    switch (g_hw.kind) {
    case HW_PRESS: sim_input_touch(0, true, g_hw.a, g_hw.b); break;
    case HW_RELEASE: sim_input_touch(0, false, g_hw.a, g_hw.b); break;
    case HW_KEY_DOWN: sim_input_key((hal_key_t)g_hw.a, true); break;
    case HW_KEY_UP: sim_input_key((hal_key_t)g_hw.a, false); break;
    case HW_CALL: sim_net_incoming_call(g_hw.num); break;
    case HW_HANGUP: sim_net_remote_hangup(); break;
    case HW_SMS: sim_net_deliver_sms(g_hw.num, g_hw.text); break;
    case HW_BATTERY: sim_power_set_battery(g_hw.a, false); break;
    case HW_BATTERY_CHARGING: sim_power_set_battery(g_hw.a, true); break;
    case HW_USB_ON: sim_usb_plug(true); break;
    case HW_USB_OFF: sim_usb_plug(false); break;
    }
}

static void hw(hw_kind_t kind, int a, int b) {
    g_hw.kind = kind;
    g_hw.a = a;
    g_hw.b = b;
    vtk_event_at(vtk_now() + SIM_MS(1), hw_event, NULL);
    delay_ms(10);
}

static void tap(int x, int y) {
    hw(HW_PRESS, x, y);
    delay_ms(80);
    hw(HW_RELEASE, x, y);
    delay_ms(200);
}

static void key(hal_key_t k) {
    hw(HW_KEY_DOWN, (int)k, 0);
    delay_ms(80);
    hw(HW_KEY_UP, (int)k, 0);
    delay_ms(200);
}

/* What the screen shows, as the UI task sees it. */
static char g_top[32], g_snap[256];
static int g_btn_x[3], g_btn_y[3], g_btn_n;
static void find_buttons(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_button_class) && g_btn_n < 3) {
            lv_area_t a;
            lv_obj_update_layout(c);
            lv_obj_get_coords(c, &a);
            g_btn_x[g_btn_n] = (a.x1 + a.x2) / 2;
            g_btn_y[g_btn_n] = (a.y1 + a.y2) / 2;
            g_btn_n++;
        }
        find_buttons(c);
    }
}
static void read_screen(void) {
    snprintf(g_top, sizeof g_top, "%s", ink_ui_top_name());
    const char *name = NULL;
    ink_ui_snapshot(g_snap, sizeof g_snap, &name);
    g_btn_n = 0;
    find_buttons(lv_layer_sys());
}

#endif
