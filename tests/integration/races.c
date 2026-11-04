/*
 * What happens when two tasks meet at the worst moment, arranged with semaphores on the whole product, from the audit of the
 * first stage (docs/audits/2026-11-04-codex-services, AUD15-005 to 007). Such a moment is rare; what it does must not depend
 * on that. The services are the product's, built with the moment in them: races_notify.c, races_interrupt.c.
 * Results are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <string.h>

#include "ink/sys/interrupt.h"
#include "ink/sys/svc.h"

#define ink_main product_main
#include "../../system/boot/main.c"
#undef ink_main

#include "whole_product.h"

extern void races_notify_member(char *out, size_t cap);
extern int races_interrupt_top(void);

static osal_sem_t *g_at_moment, *g_other_done, *g_bg_in, *g_bg_go;
static osal_task_t *g_test_task;
static int g_mode, g_notify_skip;
static bool g_notify_armed, g_ring_armed, g_ring_on;

void races_notify_unlock(void) {
    ink_svc_unlock();
    if (g_notify_armed && osal_task_self() == g_test_task && g_notify_skip-- == 0) {
        g_notify_armed = false;
        osal_sem_give(g_at_moment);
        CHECK(osal_sem_take(g_other_done, 2000000) == OSAL_OK, "the other task did its part");
    }
}

void races_audio_ring(bool on) {
    if (g_ring_armed && on && osal_task_self() == g_test_task) {
        g_ring_armed = false;
        osal_sem_give(g_at_moment);
        CHECK(osal_sem_take(g_other_done, 2000000) == OSAL_OK, "the other task did its part");
    }
    ink_audio_ring(on);
    g_ring_on = on;
}

static void other_task(void *arg) {
    (void)arg;
    for (;;) {
        osal_sem_take(g_at_moment, OSAL_WAIT_FOREVER);
        if (g_mode == 1) {
            CHECK(ink_member_switch("kid") == 0, "someone else takes the device");
            char member[16] = "";
            for (int i = 0; i < 300 && strcmp(member, "kid"); i++) {
                races_notify_member(member, sizeof member);
                if (strcmp(member, "kid")) delay_ms(5);
            }
            CHECK(!strcmp(member, "kid"), "the notification service knows");
        } else if (g_mode == 2) {
            ink_interrupt_end(races_interrupt_top());
        }
        osal_sem_give(g_other_done);
    }
}

static bool in_inbox(const char *title) {
    ink_db_cursor_t cursor = {0};
    ink_notify_view_t v;
    while (ink_notify_next(&cursor, &v) == 1)
        if (!strcmp(v.title, title)) return true;
    return false;
}

static bool told_to_ai(int from, const char *title) {
    for (int i = from; i < sim_hub_events_seen(); i++) {
        const char *e = sim_hub_event_at(i);
        CHECK(e != NULL, "the hub still has it");
        if (e && strstr(e, title)) return true;
    }
    return false;
}

/* AUD15-005, AUD16-007: for one member, and the member changes on the way: at each of the moments there are on the way. */
static void notification_for_someone(void) {
    int moments = 0;
    for (int k = 0;; k++) {
        char title[32];
        snprintf(title, sizeof title, "只给主人的 %d", k);
        g_mode = 1;
        g_notify_skip = k;
        g_notify_armed = true;
        ink_notify_t n = {.member = "owner", .title = title, .prio = INK_NOTIFY_LOW};
        int from = sim_hub_events_seen();
        CHECK(ink_notify_post(&n, NULL) == 0, "posted");
        if (g_notify_armed) { /* there was no k-th moment: all of it went by the one it is for, with nobody in between */
            g_notify_armed = false;
            CHECK(WAIT_FOR(told_to_ai(from, title), 2000), "with nobody in between, their AI does hear of it");
            break;
        }
        moments++;
        CHECK(!strcmp(ink_member_current()->id, "kid"), "the other came in between (%d)", k);
        delay_ms(300);
        CHECK(!in_inbox(title), "it is not in the inbox of who came (%d)", k);
        CHECK(!told_to_ai(from, title), "and not told to their AI (%d)", k);
        ink_member_test_reset();
        delay_ms(500);
        CHECK(in_inbox(title), "it is in the inbox of who it is for (%d)", k);
    }
    CHECK(moments >= 3, "there were moments to try (%d)", moments);
}

/* AUD15-006: it ends while the task that raised it is on its way to the audio service. */
static void interruption_ended_at_once(void) {
    g_mode = 2;
    g_ring_armed = true;
    ink_interrupt_t it = {.kind = INK_INT_ALARM, .ringing = true};
    snprintf(it.title, sizeof it.title, "一响就停");
    CHECK(ink_interrupt_raise(&it) > 0, "raised");
    CHECK(!g_ring_armed, "and ended at the worst moment");
    CHECK(WAIT_FOR(!g_ring_on, 1000), "the ringing has stopped");
    CHECK(!ink_interrupt_active() && !strcmp(active_state(), "none"), "and everything says it is over (%s)", active_state());
    ink_power_activity(osal_now_us());
}

/* AUD15-007: what a person said yes to is waiting its turn when someone else takes the device. */
static void hold_bg(void *arg) {
    (void)arg;
    osal_sem_give(g_bg_in);
    osal_sem_take(g_bg_go, OSAL_WAIT_FOREVER);
}

static int g_confirm_results;
static char g_confirm_status[16];
static void on_confirm_result(void *ctx, const char *id, const ink_args_t *a) {
    (void)ctx;
    (void)id;
    ink_args_get_str(a, "status", g_confirm_status, sizeof g_confirm_status);
    g_confirm_results++;
}

static void yes_then_someone_else(void) {
    CHECK(ink_cap_subscribe("confirm.result", on_confirm_result, NULL, INK_EXEC_DIRECT) == 0, "listening");
    CHECK(ink_fs_write_atomic("/me/racing.txt", "owner", 5) == 0 && ink_fs_write_atomic("/members/kid/racing.txt", "kid", 3) == 0, "each has a file of that name");
    CHECK(AI("file.delete", "{\"path\":\"/me/racing.txt\"}") && g_rc.status == INK_RC_PENDING_CONFIRMATION, "the AI asks to delete the owner's");
    unsigned token = 0;
    sscanf(g_rc.object, "confirm:%u", &token);
    static ink_exec_job_t held;
    CHECK(ink_exec_post(INK_EXEC_BG, &held, hold_bg, NULL) == 0 && osal_sem_take(g_bg_in, 2000000) == OSAL_OK, "the background is busy");
    int results = g_confirm_results;
    CHECK(ink_confirm_decide(token, true, INK_SRC_TOUCH) == 0, "the owner says yes");
    CHECK(ink_member_switch("kid") == 0, "and the child takes the device before it starts");
    osal_sem_give(g_bg_go);
    CHECK(WAIT_FOR(g_confirm_results > results, 2000), "told how it went");
    ink_fs_stat_t st;
    CHECK(ink_fs_stat("/me/racing.txt", &st) == 0, "the child's file of that name is still there");
    ink_member_test_reset();
    delay_ms(500);
    CHECK(ink_fs_stat("/me/racing.txt", &st) == 0, "and the owner's: it can be asked for again");
    CHECK(ink_fs_delete_start("/members/kid/racing.txt") > 0, "tidied up");
}

static void test_task(void *arg) {
    (void)arg;
    g_test_task = osal_task_self();
    delay_ms(1500);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    notification_for_someone();
    interruption_ended_at_once();
    yes_then_someone_else();
    ink_probe_count("test.finished");
}

void ink_main(void) {
    product_main();
    g_at_moment = osal_sem_create(0, 1);
    g_other_done = osal_sem_create(0, 1);
    g_bg_in = osal_sem_create(0, 1);
    g_bg_go = osal_sem_create(0, 1);
    osal_task_cfg_t other = {.name = "other", .fn = other_task, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&other, NULL) != OSAL_OK) osal_panic("other task");
    osal_task_cfg_t cfg = {.name = "racestest", .fn = test_task, .stack_bytes = 16384, .priority = OSAL_PRIO_NORMAL, .core = -1, .flags = OSAL_TASK_STACK_EXTERNAL};
    if (osal_task_create(&cfg, NULL) != OSAL_OK) osal_panic("test task");
}
