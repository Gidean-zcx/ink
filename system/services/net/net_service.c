/*
 * Network service (see ink/sys/net.h): how connected the device is, as
 * system state, and which link people prefer, as a setting. Joining Wi-Fi,
 * registering on LTE and choosing the link stay inside the platform
 * (hal_net_use); "auto" leaves the choice to it.
 */
#include "ink/hal_net.h"
#include "ink/sys/net.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"

static int h_online = -1, h_type = -1, h_signal = -1;

static const ink_state_def_t ST_ONLINE = {.id = "net.online", .type = INK_ST_BOOL, .title = "在线"};
static const ink_state_def_t ST_TYPE = {.id = "net.type", .type = INK_ST_STR, .title = "网络"};
static const ink_state_def_t ST_SIGNAL = {.id = "net.signal", .type = INK_ST_INT, .title = "信号（dBm）", .min_interval_ms = 30000};

static void publish(const hal_link_status_t *st) {
    ink_state_set_bool(h_online, st->up);
    ink_state_set_str(h_type, st->type == HAL_LINK_WIFI ? "wifi" : st->type == HAL_LINK_LTE ? "lte" : "none");
    ink_state_set_int(h_signal, st->signal_dbm);
}

static void on_link(void *ctx, const hal_link_status_t *st) { /* the platform's event task */
    (void)ctx;
    publish(st);
}

static void apply_prefer(void *ctx, int64_t v) { /* exec.bg */
    (void)ctx;
    if (v == 1) hal_net_use(HAL_LINK_WIFI);
    else if (v == 2) hal_net_use(HAL_LINK_LTE);
}

static const char *const PREFER[] = {"auto", "wifi", "lte", NULL};
static const ink_setting_def_t NET_SETTINGS[] = {
    {.id = "net.prefer", .title = "上网方式", .type = INK_SET_ENUM, .def = 0, .choices = PREFER, .child_may_change = false, .apply = apply_prefer,
     .apply_exec = INK_EXEC_BG},
};

int ink_net_init(void) {
    h_online = ink_state_define(&ST_ONLINE);
    h_type = ink_state_define(&ST_TYPE);
    h_signal = ink_state_define(&ST_SIGNAL);
    ink_settings_define(NET_SETTINGS, 1);
    hal_link_status_t st;
    hal_net_link_status(&st);
    publish(&st);
    hal_net_set_link_callback(on_link, NULL);
    int64_t prefer = ink_setting_get("net.prefer");
    if (prefer) apply_prefer(NULL, prefer); /* the saved choice */
    return 0;
}
