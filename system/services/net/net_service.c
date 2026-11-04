/*
 * Network service (see ink/sys/net.h): how connected the device is, as
 * system state, and which link people prefer, as a setting. Joining Wi-Fi and
 * registering on LTE stay inside the platform (hal_net_use).
 *
 * "auto" is decided here: Wi-Fi when it is around (the card's data costs
 * money), otherwise LTE. Going to the other network happens at once when the
 * one in use is gone, after a while when it stays down although it is around
 * (a join that does not work). Going back to Wi-Fi from a working LTE waits:
 * not during a realtime session (a switch would cut it), and not soon after
 * the last switch (a network at the edge of its range must not flap; each
 * failed try doubles the wait).
 *
 * All decisions are made in one place, the timer's callback; the switch
 * itself runs on the background executor.
 */
#include "ink/hal_net.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/exec.h"
#include "ink/sys/net.h"
#include "ink/sys/rt.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"

#define RECHECK_US 5000000ull   /* while something may still have to change */
#define STUCK_US 15000000ull    /* down this long although its network is around */
#define SETTLE_US 60000000ull   /* the least time on a link before going back to Wi-Fi */
#define SETTLE_MAX_US (3600ull * 1000000ull)

static struct {
    osal_timer_t *timer;
    ink_exec_job_t use_job;
    int use_link;
    int fresh; /* someone chose just now: no waiting for the last switch to settle */
    /* the timer's callback only */
    uint64_t down_since, switched_at, settle_us;
    bool joining_wifi; /* went to Wi-Fi and it has not come up yet */
} NS;

static int h_online = -1, h_type = -1, h_signal = -1;

static const ink_state_def_t ST_ONLINE = {.id = "net.online", .type = INK_ST_BOOL, .title = "在线"};
static const ink_state_def_t ST_TYPE = {.id = "net.type", .type = INK_ST_STR, .title = "网络"};
static const ink_state_def_t ST_SIGNAL = {.id = "net.signal", .type = INK_ST_INT, .title = "信号（dBm）", .min_interval_ms = 30000};

static void publish(const hal_link_status_t *st) {
    ink_state_set_bool(h_online, st->up);
    ink_state_set_str(h_type, st->type == HAL_LINK_WIFI ? "wifi" : st->type == HAL_LINK_LTE ? "lte" : "none");
    ink_state_set_int(h_signal, st->signal_dbm);
}

static void decide_soon(void) { osal_timer_start(NS.timer, 1000, 0); }

static void on_link(void *ctx, const hal_link_status_t *st) { /* the platform's event task */
    (void)ctx;
    publish(st);
    decide_soon();
}

static void use_job(void *arg) { /* exec.bg: the platform may take a moment */
    (void)arg;
    hal_net_use((hal_link_type_t)__atomic_load_n(&NS.use_link, __ATOMIC_ACQUIRE));
}

static hal_link_type_t auto_choice(const hal_link_status_t *st, uint64_t now, bool *later) {
    bool wifi = hal_net_available(HAL_LINK_WIFI), lte = hal_net_available(HAL_LINK_LTE);
    if (st->type == HAL_LINK_NONE) return wifi ? HAL_LINK_WIFI : lte ? HAL_LINK_LTE : HAL_LINK_NONE;
    if (!st->up) {
        hal_link_type_t other = st->type == HAL_LINK_WIFI ? HAL_LINK_LTE : HAL_LINK_WIFI;
        if (!(other == HAL_LINK_WIFI ? wifi : lte)) return st->type;
        if (!(st->type == HAL_LINK_WIFI ? wifi : lte) || now - NS.down_since >= STUCK_US) return other;
        *later = true; /* it is around: give it time to come up */
        return st->type;
    }
    if (st->type == HAL_LINK_LTE && wifi) {
        if ((!NS.switched_at || now - NS.switched_at >= NS.settle_us) && ink_rt_open_count() == 0) return HAL_LINK_WIFI;
        *later = true;
    }
    return st->type;
}

static void decide(void *arg) { /* timer task */
    (void)arg;
    uint64_t now = osal_now_us();
    hal_link_status_t st;
    hal_net_link_status(&st);
    if (__atomic_exchange_n(&NS.fresh, 0, __ATOMIC_ACQ_REL)) NS.switched_at = 0;
    if (st.up) {
        NS.down_since = 0;
        if (st.type == HAL_LINK_WIFI && NS.joining_wifi) { /* it works: the next time, the usual wait */
            NS.joining_wifi = false;
            NS.settle_us = SETTLE_US;
        }
    } else if (!NS.down_since) NS.down_since = now;
    int64_t prefer = ink_setting_get("net.prefer"); /* 0 auto, 1 Wi-Fi, 2 LTE */
    bool later = false;
    hal_link_type_t want = prefer == 1 ? HAL_LINK_WIFI : prefer == 2 ? HAL_LINK_LTE : auto_choice(&st, now, &later);
    if (want != st.type) {
        if (st.type == HAL_LINK_WIFI && NS.joining_wifi) { /* it never came up: longer before the next try */
            NS.joining_wifi = false;
            NS.settle_us = NS.settle_us * 2 < SETTLE_MAX_US ? NS.settle_us * 2 : SETTLE_MAX_US;
        }
        NS.joining_wifi = want == HAL_LINK_WIFI;
        NS.switched_at = now ? now : 1;
        NS.down_since = 0;
        __atomic_store_n(&NS.use_link, want, __ATOMIC_RELEASE);
        ink_probe_count("net.switches");
        ink_exec_post(INK_EXEC_BG, &NS.use_job, use_job, NULL);
    }
    if (later) osal_timer_start(NS.timer, RECHECK_US, 0);
}

static void apply_prefer(void *ctx, int64_t v) {
    (void)ctx;
    (void)v; /* the decision reads the newest */
    __atomic_store_n(&NS.fresh, 1, __ATOMIC_RELEASE);
    decide_soon();
}

static const char *const PREFER[] = {"auto", "wifi", "lte", NULL};
static const ink_setting_def_t NET_SETTINGS[] = {
    {.id = "net.prefer", .title = "上网方式", .type = INK_SET_ENUM, .def = 0, .choices = PREFER, .child_may_change = false, .apply = apply_prefer,
     .apply_exec = INK_EXEC_DIRECT},
};

int ink_net_init(void) {
    NS.settle_us = SETTLE_US;
    NS.timer = osal_timer_create("net", decide, NULL);
    if (!NS.timer) return -1;
    h_online = ink_state_define(&ST_ONLINE);
    h_type = ink_state_define(&ST_TYPE);
    h_signal = ink_state_define(&ST_SIGNAL);
    ink_settings_define(NET_SETTINGS, 1);
    hal_link_status_t st;
    hal_net_link_status(&st);
    publish(&st);
    hal_net_set_link_callback(on_link, NULL);
    decide_soon(); /* the saved choice */
    return 0;
}
