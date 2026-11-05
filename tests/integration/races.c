/*
 * What happens when two tasks meet at the worst moment, arranged with semaphores on the whole product, from the audit of the
 * first stage (docs/audits/2026-11-04-codex-services, AUD15-005 to 007). Such a moment is rare; what it does must not depend
 * on that. The services are the product's, built with the moment in them: races_notify.c, races_interrupt.c.
 * Results are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <string.h>

#include "ink/sys/interrupt.h"
#include "ink/sys/file.h"
#include "ink/hal_storage.h"
#include "ink/sys/time.h"
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
        const char *audience = sim_hub_event_audience(i);
        if (e && audience && !strcmp(audience, ink_member_current()->id) && strstr(e, title)) return true;
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

/* AUD17-004: an event is about whoever was there when it happened, also if it waited for the network. */
extern void sim_net_set_link(const char *);
static void net_none(void *arg) {
    (void)arg;
    sim_net_set_link("none");
}
static void net_wifi(void *arg) {
    (void)arg;
    sim_net_set_link("wifi");
}

static void events_stay_with_who_they_are_about(void) {
    vtk_event_at(vtk_now() + SIM_MS(1), net_none, NULL);
    CHECK(WAIT_FOR(!ink_hub_online(), 40000), "offline");
    CHECK(ink_fs_write_atomic("/me/only-the-owner.txt", "x", 1) == 0, "the owner's file");
    CHECK(ink_member_switch("kid") == 0, "someone else");
    CHECK(ink_fs_write_atomic("/me/the-childs.txt", "x", 1) == 0, "the child's file");
    int from = sim_hub_events_seen();
    vtk_event_at(vtk_now() + SIM_MS(1), net_wifi, NULL);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 60000), "online");
    CHECK(WAIT_FOR(told_to_ai(from, "the-childs"), 5000), "what is theirs is told to their AI");
    delay_ms(500);
    CHECK(!told_to_ai(from, "only-the-owner"), "what was the owner's is not");
    bool says = false;
    for (int i = from; i < sim_hub_events_seen(); i++) says = says || (sim_hub_event_at(i) && strstr(sim_hub_event_at(i), "the-childs") && strstr(sim_hub_event_at(i), "\"member\":\"kid\""));
    CHECK(says, "and it says whose it is");
    ink_member_test_reset();
    delay_ms(500);

    /* a copy that ends after someone else came */
    static char block[4096];
    hal_file_t *big = hal_fs_open("/content/.m/owner/big-for-race.bin", HAL_O_WRITE | HAL_O_CREATE | HAL_O_TRUNC);
    for (int i = 0; big && i < 100; i++) hal_fs_write(big, block, sizeof block);
    if (big) hal_fs_close(big);
    hal_fs_remove("/content/.m/owner/copied-for-race.bin"); /* an earlier run's */
    int job = ink_fs_copy_start("/me/big-for-race.bin", "/me/copied-for-race.bin");
    CHECK(job > 0, "a copy (%d)", job);
    from = sim_hub_events_seen();
    CHECK(ink_member_switch("kid") == 0, "someone else");
    ink_fs_job_status_t st;
    for (int i = 0; i < 500 && ink_fs_job_status(job, &st) == 0 && st.state == INK_FS_JOB_RUNNING; i++) delay_ms(20);
    CHECK(st.state == INK_FS_JOB_DONE, "it ends (%d)", st.state);
    delay_ms(500);
    CHECK(!told_to_ai(from, "copied-for-race"), "the next one's AI does not hear where it went");
    CHECK(!told_to_ai(from, "file.job_done"), "nor that it ended");
    CHECK(!told_to_ai(from, "job.finished"), "nor in the list's words");
    ink_member_test_reset();
    delay_ms(500);

    /* what someone asked for and did not get an answer to */
    CHECK(AI("notify.clear", NULL) && g_rc.status == INK_RC_PENDING_CONFIRMATION, "a card asks (%s)", ink_rc_name(g_rc.status));
    from = sim_hub_events_seen();
    CHECK(ink_member_switch("kid") == 0, "someone else");
    delay_ms(1000);
    CHECK(!told_to_ai(from, "confirm.result"), "the next one's AI does not hear how that ended");
    ink_member_test_reset();
    delay_ms(500);

    /* the owner's reminder, with someone else there */
    char args[96];
    ink_tm_t t;
    ink_time_to_local(ink_time_now_s() + 60, &t);
    snprintf(args, sizeof args, "{\"at\":\"%04d-%02d-%02d %02d:%02d\",\"title\":\"给主人的提醒\"}", t.year, t.month, t.day, t.hour, t.min);
    CHECK(AI("reminder.add", args) && g_rc.status == INK_RC_DONE, "a reminder (%s)", g_rc.detail);
    from = sim_hub_events_seen();
    double fired = sim_metric_value("event.reminder.fired");
    CHECK(ink_member_switch("kid") == 0, "someone else");
    CHECK(WAIT_FOR(sim_metric_value("event.reminder.fired") > fired, 70000), "it goes off");
    delay_ms(500);
    CHECK(!told_to_ai(from, "给主人的提醒"), "the next one's AI does not hear of it");
    ink_member_test_reset();
    delay_ms(500);
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

/* AUD18-3: someone else comes while the background has something else to do: what follows on it has not caught up yet. */
static ink_receipt_t g_queued_rc;
static volatile int g_queued_done;
static void queued_done(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    g_queued_rc = *rc;
    g_queued_rc.result = NULL;
    g_queued_done++;
}

static void someone_else_while_the_background_is_busy(void) {
    static ink_exec_job_t held;
    uint32_t id = 0;
    ink_notify_t n = {.member = "owner", .title = "主人留的", .prio = INK_NOTIFY_LOW};
    CHECK(ink_notify_post(&n, &id) == 0 && id, "the owner has a notification");
    delay_ms(300);
    /* the inbox has not changed hands yet */
    CHECK(ink_exec_post(INK_EXEC_BG, &held, hold_bg, NULL) == 0 && osal_sem_take(g_bg_in, 2000000) == OSAL_OK, "the background is busy");
    CHECK(ink_member_switch("kid") == 0, "someone else");
    CHECK(AI("notify.list", NULL) && g_rc.status == INK_RC_DONE && !strstr(g_result, "主人留的"), "the next one does not see it (%s)", g_result);
    char args[40];
    snprintf(args, sizeof args, "{\"id\":%u}", (unsigned)id);
    CHECK(AI("notify.dismiss", args) && g_rc.status == INK_RC_FAILED, "nor can take it away (%s)", ink_rc_name(g_rc.status));
    CHECK(ink_notify_channel_set("ai", false) != 0, "nor change the last one's channels");
    osal_sem_give(g_bg_go);
    delay_ms(500);
    CHECK(AI("notify.list", NULL) && g_rc.status == INK_RC_DONE && !strstr(g_result, "主人留的"), "nor after (%s)", g_result);
    ink_member_test_reset();
    delay_ms(500);
    CHECK(in_inbox("主人留的"), "it is still the owner's");
    CHECK(ink_notify_channel_on("ai"), "and so are their channels");
    /* a call that was waiting its turn */
    hal_fs_remove("/content/.m/owner/queued-for-race.txt");
    hal_fs_remove("/content/.m/kid/queued-for-race.txt");
    CHECK(ink_exec_post(INK_EXEC_BG, &held, hold_bg, NULL) == 0 && osal_sem_take(g_bg_in, 2000000) == OSAL_OK, "the background is busy");
    ink_call_opts_t o = {.src = INK_SRC_AI, .args_json = "{\"path\":\"/me/queued-for-race.txt\",\"text\":\"x\"}", .reply_exec = INK_EXEC_DIRECT, .done = queued_done};
    int done = g_queued_done;
    CHECK(ink_cap_call("file.write", NULL, &o) == 0, "the owner's AI asks to write");
    CHECK(ink_member_switch("kid") == 0, "someone else, before it runs");
    osal_sem_give(g_bg_go);
    CHECK(WAIT_FOR(g_queued_done > done, 3000) && g_queued_rc.status == INK_RC_STALE, "it does not run for them (%s)", ink_rc_name(g_queued_rc.status));
    CHECK(hal_fs_size("/content/.m/kid/queued-for-race.txt") < 0 && hal_fs_size("/content/.m/owner/queued-for-race.txt") < 0, "nothing is written");
    ink_member_test_reset();
    delay_ms(500);
    /* an event that was waiting for whoever takes it up */
    CHECK(ink_exec_post(INK_EXEC_BG, &held, hold_bg, NULL) == 0 && osal_sem_take(g_bg_in, 2000000) == OSAL_OK, "the background is busy");
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    ink_args_init(a);
    ink_args_str(a, "from", "王老师");
    ink_args_str(a, "text", "明天开会");
    ink_cap_emit("message.received", a);
    osal_free(a);
    CHECK(ink_member_switch("kid") == 0, "someone else, before it is taken up");
    osal_sem_give(g_bg_go);
    delay_ms(800);
    CHECK(!in_inbox("王老师"), "it does not go in their inbox");
    ink_member_test_reset();
    delay_ms(800);
    CHECK(in_inbox("王老师"), "it went in the inbox of who was there");
}

static void test_task(void *arg) {
    (void)arg;
    g_test_task = osal_task_self();
    delay_ms(1500);
    ink_hub_demand();
    CHECK(WAIT_FOR(ink_hub_online(), 30000), "online");
    notification_for_someone();
    events_stay_with_who_they_are_about();
    interruption_ended_at_once();
    yes_then_someone_else();
    someone_else_while_the_background_is_busy();
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
