/*
 * Which network "auto" uses as networks come and go (system/services/net/net_service.c, audit
 * AUD12-001). The script says what is around; each check is about a moment in between.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_cellular.h"
#include "ink/hal_net.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/display.h"
#include "ink/sys/exec.h"
#include "ink/sys/hub.h"
#include "ink/sys/members.h"
#include "ink/sys/net.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/rt.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "sim.h"

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

static void until_s(int s) {
    uint64_t t = (uint64_t)s * 1000000u, now = osal_now_us();
    if (t > now) osal_task_delay_us(t - now);
}

static const char *link_name(hal_link_type_t t) { return t == HAL_LINK_WIFI ? "wifi" : t == HAL_LINK_LTE ? "lte" : "none"; }

static bool on_link(hal_link_type_t want, bool up) {
    hal_link_status_t st;
    hal_net_link_status(&st);
    char s[16] = "";
    int64_t online = -1;
    ink_state_get_str(ink_state_find("net.type"), s, sizeof s);
    ink_state_get_int(ink_state_find("net.online"), &online);
    return st.type == want && st.up == up && !strcmp(s, link_name(want)) && online == (up ? 1 : 0);
}

static void set_prefer(int64_t v) {
    ink_receipt_t rc;
    CHECK(ink_setting_set("net.prefer", v, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "net.prefer set to %d", (int)v);
}

static void echo_frame(void *ctx, int sid, uint8_t kind, const uint8_t *p, size_t len, uint16_t seq, uint32_t t_ms, uint8_t flags) {
    (void)ctx;
    (void)sid;
    (void)kind;
    (void)p;
    (void)len;
    (void)seq;
    (void)t_ms;
    (void)flags;
}

static void net_task(void *arg) {
    (void)arg;
    if (ink_display_init() != 0 || ink_storage_init() != 0 || ink_cap_init() != 0 || ink_exec_init() != 0 || ink_res_init() != 0 || ink_state_init() != 0 ||
        ink_settings_init() != 0 || ink_power_init() != 0 || ink_net_init() != 0 || ink_hub_init("wss://hub.local/v1/device") != 0 || ink_rt_init() != 0) {
        CHECK(0, "the system services start");
        return;
    }
    ink_members_init();
    static const ink_rt_handler_t H = {.on_frame = echo_frame};

    until_s(5);
    CHECK(ink_setting_get("net.prefer") == 0, "auto by default");
    CHECK(on_link(HAL_LINK_LTE, true), "only cellular around: on LTE");

    until_s(14); /* @10 Wi-Fi appears */
    CHECK(on_link(HAL_LINK_WIFI, true), "Wi-Fi appeared: moved to it and joined");
    ink_hub_demand();
    until_s(18);
    CHECK(ink_hub_online(), "the hub is reachable on it");

    until_s(23); /* @20 Wi-Fi gone */
    CHECK(on_link(HAL_LINK_LTE, true), "Wi-Fi gone: on LTE at once");

    until_s(32); /* @25 Wi-Fi back */
    CHECK(on_link(HAL_LINK_LTE, true), "Wi-Fi back soon after: no hurry, a network at the edge of its range does not flap");
    until_s(85); /* 60 s after the last switch */
    CHECK(on_link(HAL_LINK_WIFI, true), "then back on Wi-Fi");

    until_s(92); /* @88 gone, @90 back */
    CHECK(on_link(HAL_LINK_LTE, true), "gone again: LTE");
    until_s(146);
    int sid = ink_rt_open("echo", NULL, &H);
    CHECK(sid > 0, "a realtime session opens");
    until_s(156);
    CHECK(on_link(HAL_LINK_LTE, true), "the wait is over, but a switch would cut the session: still LTE");
    ink_rt_close(sid);
    until_s(164);
    CHECK(on_link(HAL_LINK_WIFI, true), "the session closed: Wi-Fi");

    set_prefer(2);
    until_s(166);
    CHECK(on_link(HAL_LINK_LTE, true), "chosen LTE: at once");
    until_s(175);
    CHECK(on_link(HAL_LINK_LTE, true), "and it stays, with Wi-Fi around");
    until_s(183); /* @180 no cellular coverage */
    CHECK(on_link(HAL_LINK_LTE, false), "chosen LTE and no coverage: offline, as chosen");
    set_prefer(0);
    until_s(188);
    CHECK(on_link(HAL_LINK_WIFI, true), "back to auto: at once to what is there");

    /* cellular back @190; @195 Wi-Fi in range but it cannot be joined any more */
    until_s(212);
    CHECK(on_link(HAL_LINK_LTE, true), "the known Wi-Fi network stopped working: after a while, LTE");
    until_s(274); /* 60 s after that switch: it tries again */
    hal_link_status_t st;
    hal_net_link_status(&st);
    CHECK(st.type == HAL_LINK_WIFI && !st.up, "it tries Wi-Fi again (still around)");
    until_s(294); /* 15 s later: it did not come up */
    CHECK(on_link(HAL_LINK_LTE, true), "a join that does not work: back on LTE");
    until_s(380);
    CHECK(on_link(HAL_LINK_LTE, true), "and the next try waits longer");
    until_s(415); /* 120 s after the failed try */
    hal_net_link_status(&st);
    CHECK(st.type == HAL_LINK_WIFI, "then it is tried again");

    /* @420 the network works again; the card goes through a switch, and registers only after Wi-Fi is gone */
    until_s(435);
    CHECK(on_link(HAL_LINK_WIFI, true), "the known network works again");
    until_s(440);
    CHECK(hal_cell_select_sim(HAL_CELL_SLOT_INTERNAL, 1) != 0 || !hal_net_available(HAL_LINK_LTE), "the card is registering (its radio just came back)");
    until_s(452); /* @441.5, radio off: Wi-Fi gone with nothing to go to; the card registers about 3 s later */
    CHECK(on_link(HAL_LINK_LTE, true), "when the card has registered, auto uses it: no other change is needed to notice");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "nettest", .fn = net_task, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create the test task");
}
