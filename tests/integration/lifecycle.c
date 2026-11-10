/*
 * What happens at awkward moments: calls, jobs, captures and the remote picture when another task
 * comes in between, a queue is long, a deadline passes or the link drops (the findings of the
 * 2026-11-04 audit, AUD11-001..009). The bus and the remote host are included here unchanged, so that
 * a test can act exactly at their lock boundaries and look at what they hold.
 * Results are the probe counters "test.pass" / "test.fail", as in systest.c.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_cellular.h"
#include "ink/hal_jpeg.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/audio.h"
#include "ink/sys/camera.h"
#include "ink/sys/cap.h"
#include "ink/sys/data.h"
#include "ink/sys/svc.h"
#include "ink/sys/comm.h"
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
#include "ink/sys/ui.h"
#include "sim.h"

static void hooked_unlock(osal_mutex_t *m);
#define osal_mutex_unlock hooked_unlock
#include "../../system/policy/cap.c"
#undef osal_mutex_unlock

static int hooked_jpeg(const uint8_t *src, size_t len, uint8_t *dst, int w, int h);
#define hal_jpeg_decode_gray hooked_jpeg
#include "../../system/apps/remote/remote.c"
#undef hal_jpeg_decode_gray

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

static bool wait_until(bool (*cond)(void), uint64_t max_us) {
    for (uint64_t w = 0; !cond() && w < max_us; w += 1000) osal_task_delay_us(1000);
    return cond();
}

static int pool_used(void) {
    int n = 0;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < POOL_N; i++) n += CB.pool[i].used;
    osal_mutex_unlock(CB.lock);
    return n;
}

/* ------------------------------------------------------------ capabilities under test */

static volatile int g_runs, g_replies, g_bg_runs, g_slept, g_nparked, g_nested_rc;
static volatile uint32_t g_left;
static ink_receipt_t g_last;
static ink_call_t *g_parked[POOL_N];

static void t_reply(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    g_last = *rc;
    g_replies++;
}

static void t_once(ink_call_t *c) { c->rc.revision = (uint32_t)++g_runs; }
static void t_bg(ink_call_t *c) {
    (void)c;
    g_bg_runs++;
}
static void t_sleeper(ink_call_t *c) {
    g_left = ink_cap_time_left_ms(c);
    osal_task_delay_us(200000);
    g_slept++;
}
static void t_parked(ink_call_t *c) {
    g_parked[g_nparked++] = c;
    ink_cap_defer(c);
}
static void t_early(ink_call_t *c) { /* completes while run() is still on the stack, then starts another call */
    ink_cap_defer(c);
    ink_cap_done(c);
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_DIRECT, .done = t_reply};
    g_nested_rc = ink_cap_call("t.parked", NULL, &o);
}
static void t_early_plain(ink_call_t *c) { ink_cap_done(c); } /* without deferring first */

static const ink_cap_t T_CAPS[] = {
    {.id = "t.once", .title = "一次", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = t_once},
    {.id = "t.bg", .title = "后台", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .timeout_ms = 30, .run = t_bg},
    {.id = "t.sleeper", .title = "阻塞", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .timeout_ms = 50, .run = t_sleeper},
    {.id = "t.parked", .title = "不回", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .timeout_ms = 60000, .run = t_parked},
    {.id = "t.early", .title = "提前完成", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = t_early},
    {.id = "t.early_plain", .title = "提前完成2", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = t_early_plain},
};

/* ---- request ids: a copy arriving right at a lock boundary of the first one (AUD11-006) ---- */

enum { HOOK_OFF, HOOK_CLAIMED, HOOK_SETTLED };
static int g_hook, g_hook_rc = -99;
static const char *g_hook_rid;
static const ink_call_opts_t *g_hook_opts;

static void hooked_unlock(osal_mutex_t *m) {
    osal_mutex_unlock(m);
    if (g_hook == HOOK_OFF || m != CB.lock) return;
    for (int i = 0; i < POOL_N; i++) {
        pending_t *p = &CB.pool[i];
        if (!p->used || strcmp(p->rid, g_hook_rid)) continue;
        if ((g_hook == HOOK_CLAIMED && !p->accepted) || (g_hook == HOOK_SETTLED && p->replied && !p->answered)) {
            g_hook = HOOK_OFF;
            g_hook_rc = ink_cap_call("t.once", NULL, g_hook_opts);
            return;
        }
    }
}

static void dedupe_checks(void) {
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .request_id = "same-1", .reply_exec = INK_EXEC_DIRECT, .done = t_reply};
    g_hook_opts = &o;
    g_runs = g_replies = 0;
    g_hook = HOOK_SETTLED;
    g_hook_rid = "same-1";
    CHECK(ink_cap_call("t.once", NULL, &o) == 0 && g_hook == HOOK_OFF && g_hook_rc == 0, "the first call and its copy are accepted");
    CHECK(g_runs == 1 && g_replies == 2 && g_last.revision == 1, "a copy at the moment its receipt became final does not run again (%d runs, %d replies)", g_runs,
          g_replies);
    o.request_id = "same-2";
    g_hook_rid = "same-2";
    g_runs = g_replies = 0;
    g_hook = HOOK_CLAIMED;
    CHECK(ink_cap_call("t.once", NULL, &o) == 0 && g_hook == HOOK_OFF && g_hook_rc == 0, "two copies arrive together");
    CHECK(g_runs == 1 && g_replies == 2 && g_last.status == INK_RC_DONE, "one runs, the other hears it is running (%d runs, %d replies)", g_runs, g_replies);
    char too_long[RID_CAP + 8];
    memset(too_long, 'r', sizeof too_long);
    too_long[sizeof too_long - 1] = 0;
    o.request_id = too_long;
    CHECK(ink_cap_call("t.once", NULL, &o) == -1, "a request id too long to compare whole is not accepted");
    CHECK(pool_used() == 0, "every slot is back");
}

/* ---- completing while the handler is still running (AUD11-009) ---- */

static void early_checks(void) {
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_DIRECT, .done = t_reply};
    g_replies = g_nparked = 0;
    g_nested_rc = -99;
    CHECK(ink_cap_call("t.early", NULL, &o) == 0 && g_nested_rc == 0, "a handler completes its call and starts another");
    CHECK(g_replies == 1 && g_nparked == 1, "the first is answered once, the new one is waiting (%d, %d)", g_replies, g_nparked);
    CHECK(pool_used() == 1, "the old handler's return left the new call alone (%d slots)", pool_used());
    ink_cap_done(g_parked[0]);
    CHECK(g_replies == 2 && pool_used() == 0, "the new one answers when it is done (%d, %d slots)", g_replies, pool_used());
    g_replies = 0;
    CHECK(ink_cap_call("t.early_plain", NULL, &o) == 0 && g_replies == 1 && pool_used() == 0, "done without deferring: answered once, slot back (%d)", g_replies);
}

/* ---- deadlines count from acceptance (AUD11-002) ---- */

static ink_exec_job_t g_block_job;
static osal_sem_t *g_block;
static volatile int g_blocking;

static void blocker(void *arg) {
    (void)arg;
    g_blocking = 1;
    osal_sem_take(g_block, OSAL_WAIT_FOREVER);
    g_blocking = 0;
}

static bool is_blocking(void) { return g_blocking; }
static bool is_free(void) { return !g_blocking && !ink_exec_waiting(INK_EXEC_BG); }

static void block_bg(void) {
    ink_exec_post(INK_EXEC_BG, &g_block_job, blocker, NULL);
    wait_until(is_blocking, 1000000);
}

static void unblock_bg(void) {
    osal_sem_give(g_block);
    wait_until(is_free, 1000000);
    osal_task_delay_us(5000);
}

static void deadline_checks(void) {
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_DIRECT, .done = t_reply};
    block_bg();
    g_replies = g_bg_runs = 0;
    CHECK(ink_cap_call("t.bg", NULL, &o) == 0 && g_replies == 0, "accepted while its executor is busy");
    osal_task_delay_us(100000);
    CHECK(g_replies == 1 && g_last.status == INK_RC_FAILED && strstr(g_last.detail, "before it started"), "answered at its deadline, still waiting (%s)", g_last.detail);
    unblock_bg();
    CHECK(g_bg_runs == 0 && g_replies == 1 && pool_used() == 0, "and it never runs (%d runs, %d replies, %d slots)", g_bg_runs, g_replies, pool_used());
    g_replies = g_slept = 0;
    double late = sim_metric_value("cap.late");
    CHECK(ink_cap_call("t.sleeper", NULL, &o) == 0, "a handler that blocks");
    osal_task_delay_us(100000);
    CHECK(g_replies == 1 && g_last.status == INK_RC_FAILED && strstr(g_last.detail, "may still finish") && !g_slept, "is answered at its deadline while it runs (%s)",
          g_last.detail);
    CHECK(g_left > 0 && g_left <= 50, "it could see how long it had (%u ms)", (unsigned)g_left);
    osal_task_delay_us(200000);
    CHECK(g_slept == 1 && g_replies == 1 && pool_used() == 0 && sim_metric_value("cap.late") == late + 1, "its result comes too late, the slot comes back (%d replies)", g_replies);
}

/* ---- nothing is lost while an executor is busy (AUD11-001) ---- */

static volatile int g_applies, g_watched, g_fill_runs, g_fill_order_ok = 1;
static volatile int64_t g_applied;
static ink_exec_job_t g_fill[40];

static void t_apply(void *ctx, int64_t v) {
    (void)ctx;
    g_applied = v;
    g_applies++;
}
static const ink_setting_def_t T_SETTINGS[] = {
    {.id = "t.apply", .title = "应用", .type = INK_SET_INT, .min = 0, .max = 100, .def = 0, .child_may_change = true, .apply = t_apply, .apply_exec = INK_EXEC_BG},
};
static const ink_state_def_t T_STATE = {.id = "t.state", .type = INK_ST_INT, .title = "状态"};
static int64_t g_seen_state;
static int g_h_state;
static void t_watch(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)id;
    ink_state_get_int(h, &g_seen_state);
    g_watched++;
}
static void t_fill(void *arg) {
    if ((intptr_t)arg != g_fill_runs) g_fill_order_ok = 0;
    g_fill_runs++;
}

static void pressure_checks(void) {
    CHECK(ink_settings_define(T_SETTINGS, 1) == 0, "a setting applied on exec.bg");
    g_h_state = ink_state_define(&T_STATE);
    CHECK(g_h_state >= 0 && ink_state_watch("t.state", t_watch, NULL, INK_EXEC_BG) == 0, "a state watched on exec.bg");
    block_bg();
    for (intptr_t i = 0; i < 40; i++) ink_exec_post(INK_EXEC_BG, &g_fill[i], t_fill, (void *)i);
    ink_receipt_t rc;
    for (int v = 1; v <= 20; v++) {
        CHECK(ink_setting_set("t.apply", v, INK_SRC_SYSTEM, &rc) == INK_RC_DONE, "stored");
        ink_state_set_int(g_h_state, v);
    }
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_BG, .done = t_reply};
    g_replies = 0;
    int accepted = 0;
    for (int i = 0; i < 12; i++) accepted += ink_cap_call("t.once", NULL, &o) == 0;
    CHECK(accepted == 12 && g_replies == 0, "12 calls accepted, their receipts wait for exec.bg (%d)", accepted);
    unblock_bg();
    CHECK(g_fill_runs == 40 && g_fill_order_ok, "40 jobs posted while it was busy all ran, in order (%d)", g_fill_runs);
    CHECK(g_replies == 12 && pool_used() == 0, "every receipt arrived (%d, %d slots)", g_replies, pool_used());
    CHECK(g_applied == 20 && g_applies == 1, "the setting's last value was applied, once for the burst (%lld, %d)", (long long)g_applied, g_applies);
    CHECK(g_seen_state == 20 && g_watched == 1, "the watcher saw the last change (%lld, %d)", (long long)g_seen_state, g_watched);
    CHECK(ink_setting_set("t.apply", 20, INK_SRC_SYSTEM, &rc) == INK_RC_DONE, "the same value set again");
    osal_task_delay_us(5000);
    CHECK(g_applies == 2, "reaches its owner again (it may have to correct the hardware; %d)", g_applies);
}

/* ---- the same on the UI task, whose message queue can be full ---- */

static ink_exec_job_t g_ui_block_job, g_ui_fill[40];
static osal_sem_t *g_ui_block;
static volatile int g_ui_blocking, g_ui_fill_runs, g_ui_order_ok = 1;

static void ui_blocker(void *arg) {
    (void)arg;
    g_ui_blocking = 1;
    osal_sem_take(g_ui_block, OSAL_WAIT_FOREVER);
    g_ui_blocking = 0;
}
static bool ui_blocking(void) { return g_ui_blocking; }
static void ui_fill(void *arg) {
    if ((intptr_t)arg != g_ui_fill_runs) g_ui_order_ok = 0;
    g_ui_fill_runs++;
}
static bool ui_filled(void) { return g_ui_fill_runs == 40; }

static void ui_pressure_checks(void) {
    g_ui_block = osal_sem_create(0, 1);
    ink_exec_post(INK_EXEC_UI, &g_ui_block_job, ui_blocker, NULL);
    CHECK(wait_until(ui_blocking, 2000000), "the UI task is busy");
    uint64_t t0 = osal_now_us();
    for (intptr_t i = 0; i < 40; i++) ink_exec_post(INK_EXEC_UI, &g_ui_fill[i], ui_fill, (void *)i); /* more than its queue holds */
    CHECK(osal_now_us() - t0 < 100000, "posting does not wait for it (%llu us)", (unsigned long long)(osal_now_us() - t0));
    CHECK(ink_ui_busy(), "it counts as busy while jobs wait");
    osal_sem_give(g_ui_block);
    CHECK(wait_until(ui_filled, 2000000) && g_ui_order_ok, "all 40 ran, in order (%d)", g_ui_fill_runs);
}

/* ---- a full pool is said, not hidden (the promise of ink_cap_call) ---- */

static void pool_checks(void) {
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_DIRECT, .done = t_reply};
    o.request_id = "";
    g_runs = 0;
    CHECK(ink_cap_call("t.once", NULL, &o) == 0 && ink_cap_call("t.once", NULL, &o) == 0 && g_runs == 2, "an empty request id is no request id (%d)", g_runs);
    o.request_id = NULL;
    g_nparked = g_replies = 0;
    int n = 0;
    for (int i = 0; i < POOL_N; i++) n += ink_cap_call("t.parked", NULL, &o) == 0;
    CHECK(n == POOL_N && pool_used() == POOL_N, "the pool fills (%d)", n);
    CHECK(ink_cap_call("t.once", NULL, &o) == -1 && g_replies == 0, "a new call is not accepted, nothing is promised");
    o.request_id = "same-1";
    CHECK(ink_cap_call("t.once", NULL, &o) == -1 && g_replies == 0, "nor the repeat of an old one: it could not be answered");
    o.request_id = NULL;
    for (int i = 0; i < POOL_N; i++) ink_cap_done(g_parked[i]);
    CHECK(g_replies == POOL_N && pool_used() == 0, "all answered, all back (%d)", g_replies);
    o.request_id = "same-1";
    g_runs = 99;
    CHECK(ink_cap_call("t.once", NULL, &o) == 0 && g_runs == 99 && g_last.revision == 1, "now the repeat gets the first receipt");
}

/* ---- a capture ending while the next one wants to begin (AUD11-004) ---- */

static volatile int g_c1_frames, g_c1_ends, g_c2_frames, g_c2_ends, g_restart = -99, g_c2_seq_ok = 1;
static volatile uint32_t g_c2_next;

static void cap2(void *ctx, const uint8_t *data, size_t len, uint32_t seq) {
    (void)ctx;
    (void)data;
    if (!len) {
        g_c2_ends++;
        return;
    }
    if (seq != g_c2_next) g_c2_seq_ok = 0;
    g_c2_next = seq + 1;
    g_c2_frames++;
}

static void cap1(void *ctx, const uint8_t *data, size_t len, uint32_t seq) {
    (void)ctx;
    (void)data;
    (void)seq;
    if (len) g_c1_frames++;
    else {
        g_c1_ends++;
        g_restart = ink_audio_capture_begin(cap2, NULL); /* from inside the end's delivery */
    }
}

static void capture_round(const char *how) {
    g_c1_frames = g_c1_ends = g_c2_frames = g_c2_ends = 0;
    g_restart = -99;
    g_c2_next = 0;
    CHECK(ink_audio_capture_begin(cap1, NULL) == 0, "%s: a capture begins", how);
    osal_task_delay_us(500000);
    CHECK(g_c1_frames > 0, "%s: its frames arrive (%d)", how, g_c1_frames);
    ink_audio_capture_end();
    CHECK(ink_audio_capture_begin(cap2, NULL) == -1, "%s: while it is ending the next one is told to wait", how);
    osal_task_delay_us(200000);
    CHECK(g_c1_ends == 1 && g_restart == -1 && !g_c2_frames, "%s: its end is delivered once; a begin from inside that is refused cleanly (%d, %d)", how, g_c1_ends, g_restart);
    int c1 = g_c1_frames;
    CHECK(ink_audio_capture_begin(cap2, NULL) == 0, "%s: then the next one begins", how);
    osal_task_delay_us(500000);
    ink_audio_capture_end();
    osal_task_delay_us(200000);
    CHECK(g_c2_frames > 0 && g_c2_seq_ok && g_c2_ends == 1, "%s: with its own encoder and its own end (%d, %d)", how, g_c2_frames, g_c2_ends);
    CHECK(g_c1_frames == c1 && g_c1_ends == 1, "%s: nothing goes to the old one any more", how);
}

static void capture_checks(void) {
    capture_round("alone");
    CHECK(ink_audio_can_listen() && ink_audio_listen(true, NULL, NULL) == 0, "the front end listens");
    capture_round("listening");
    CHECK(ink_audio_listen(false, NULL, NULL) == 0, "and stops");
}

/* ---- the card follows the setting: from a saved value, after drift (AUD11-003) ---- */

static bool state_is(const char *id, const char *want) {
    char s[32] = "";
    return ink_state_get_str(ink_state_find(id), s, sizeof s) > 0 && !strcmp(s, want);
}
static bool on_internal(void) { return state_is("cell.sim", "internal"); }

static void sim_checks(void) {
    CHECK(ink_setting_get("cell.sim_slot") == 1, "the saved card was read");
    CHECK(wait_until(on_internal, 10000000), "the modem is on it soon after start");
    CHECK(sim_metric_value("comm.sim_switched") == 1, "by one online switch");
    osal_task_delay_us(200000); /* the service has finished talking to the modem */
    CHECK(hal_cell_select_sim(HAL_CELL_SLOT_EXTERNAL, 5000) == 0, "the modem is changed behind the setting's back");
    ink_receipt_t rc;
    CHECK(ink_setting_set("cell.sim_slot", 1, INK_SRC_TOUCH, &rc) == INK_RC_DONE, "the same value set again");
    osal_task_delay_us(1000000);
    for (int w = 0; w < 200 && sim_metric_value("comm.sim_switched") < 2; w++) osal_task_delay_us(100000);
    osal_task_delay_us(200000);
    hal_cell_status_t st;
    CHECK(sim_metric_value("comm.sim_switched") == 2 && hal_cell_status(&st) && st.slot == HAL_CELL_SLOT_INTERNAL, "corrects it");
}

/* ---- a text message arrives as it was written (AUD12-002) ---- */

static void sms_checks(void) {
    static char text[400], sent[400];
    char number[24];
    memset(text, 0, sizeof text);
    for (int i = 0; i < 106; i++) memcpy(text + i * 3, "中", 3);
    memcpy(text + 318, "abc", 3); /* 109 characters, 321 bytes */
    ink_args_t a;
    ink_args_init(&a);
    CHECK(ink_args_str(&a, "to", "10086") == 0 && ink_args_str(&a, "text", text) == 0, "a long message fits the arguments");
    ink_call_opts_t o = {.src = INK_SRC_TOUCH, .reply_exec = INK_EXEC_DIRECT, .done = t_reply};
    int replies = g_replies;
    CHECK(ink_cap_call("message.send", &a, &o) == 0, "it is accepted");
    for (int w = 0; w < 3000 && g_replies == replies; w++) osal_task_delay_us(10000);
    CHECK(g_last.status == INK_RC_DONE, "and sent (%s, %s)", ink_rc_name(g_last.status), g_last.detail);
    int n = sim_net_sent_sms(0, number, sizeof number, sent, sizeof sent);
    CHECK(n == 1 && !strcmp(sent, text), "the modem has all of it (%d messages, %d bytes)", n, (int)strlen(sent));

    for (int i = 0; i < 118; i++) memcpy(text + i * 3, "中", 3); /* the old limit */
    text[118 * 3] = 0;
    ink_args_init(&a);
    CHECK(ink_args_str(&a, "to", "10086000000000000000") == 0 && ink_args_str(&a, "text", text) == 0, "what the schema allows fits the arguments, with the longest number");
    replies = g_replies;
    CHECK(ink_cap_call("message.send", &a, &o) == 0, "and is accepted");
    for (int w = 0; w < 3000 && g_replies == replies; w++) osal_task_delay_us(10000);
    n = sim_net_sent_sms(1, number, sizeof number, sent, sizeof sent);
    CHECK(g_last.status == INK_RC_DONE && n == 2 && !strcmp(sent, text), "and arrives whole (%s, %d bytes)", ink_rc_name(g_last.status), (int)strlen(sent));

    static char longest[1100]; /* sms-multipart: no longer 118, but 5 parts of 67; one character more is refused */
    for (int i = 0; i < 5 * 67 + 1; i++) memcpy(longest + i * 3, "中", 4);
    ink_args_init(&a);
    ink_args_str(&a, "to", "10086");
    CHECK(ink_args_str(&a, "text", longest) == 0, "one character more still fits the table");
    replies = g_replies;
    CHECK(ink_cap_call("message.send", &a, &o) == 0, "the service decides");
    for (int w = 0; w < 300 && g_replies == replies; w++) osal_task_delay_us(10000);
    CHECK(g_last.status == INK_RC_INVALID && sim_net_sent_sms(2, number, sizeof number, sent, sizeof sent) == 2, "it is refused, nothing goes out (%s)", g_last.detail);
}

/* ---- the remote picture: the device's and the hub's stay the same (AUD11-007, AUD11-008) ---- */

static volatile int g_jpeg_hook;

static int hooked_jpeg(const uint8_t *src, size_t len, uint8_t *dst, int w, int h) {
    if (g_jpeg_hook) { /* the page is closed and opened again while this region is being decoded */
        g_jpeg_hook = 0;
        osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
        RS.gen++;
        osal_mutex_unlock(RS.lock);
    }
    return hal_jpeg_decode_gray(src, len, dst, w, h);
}

/* Text and interface are lossless at 16 levels; the photograph's bands (a stand-in JPEG) are skipped. */
static int picture_differs(void) {
    int w = 0, h = 0, n = 0;
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    int sid = RS.sid;
    osal_mutex_unlock(RS.lock);
    const uint8_t *hub = sim_remote_shown(sid, &w, &h);
    if (!hub || !RS.px || w != RS.w || h != RS.h) return -1;
    int py = h / 2 - 40;
    for (int y = 0; y < h; y++) {
        int band = y / 16 * 16;
        bool photo = band >= py && band < py + 200;
        for (int x = 0; x < w; x++) {
            if (photo && x >= 24 && x < w - 24) continue;
            int want = (hub[(size_t)y * (size_t)w + (size_t)x] + 8) / 17 * 17;
            n += RS.px[(size_t)y * (size_t)w + (size_t)x] != want;
        }
    }
    return n;
}

static int g_want_updates;
static bool shown_enough(void) { return (int)RS.updates >= g_want_updates && !ink_ui_busy() && !ink_exec_waiting(INK_EXEC_MEDIA); }

static bool wait_picture(int more) {
    g_want_updates = (int)RS.updates + more;
    return wait_until(shown_enough, 8000000);
}

static void remote_tap(int x, int y) {
    uint8_t in[5] = {1, (uint8_t)x, (uint8_t)(x >> 8), (uint8_t)y, (uint8_t)(y >> 8)};
    ink_rt_send(RS.sid, INK_RT_INPUT, in, sizeof in, 0);
}

static ink_exec_job_t g_ui_job;
static void pop_job(void *arg) {
    (void)arg;
    ink_ui_pop();
}
static void open_remote(void) {
    ink_args_t a;
    ink_args_init(&a);
    ink_args_str(&a, "url", "about:home");
    ink_call_opts_t o = {.src = INK_SRC_SYSTEM, .reply_exec = INK_EXEC_DIRECT};
    ink_cap_call("remote.open", &a, &o);
}
static void close_remote(void) { ink_exec_post(INK_EXEC_UI, &g_ui_job, pop_job, NULL); }

/* Where the note about a lost connection is: the AI's answer sheet covers the lower 58% of the screen (AUD13-004). */
static volatile int g_note_bottom = -1, g_note_visible;
static ink_exec_job_t g_note_job;
static void note_job(void *arg) {
    (void)arg;
    lv_area_t a;
    lv_obj_get_coords(RS.note, &a);
    g_note_bottom = a.y2;
    g_note_visible = !lv_obj_has_flag(RS.note, LV_OBJ_FLAG_HIDDEN) && a.y2 > a.y1;
}

static bool net_lost(void) { return !ink_hub_online(); }
static bool is_ready(void) { return RS.sid && ink_rt_ready(RS.sid); }

static void stale_region_check(void) { /* no page is open: the host's parts stand alone */
    RS.lock = RS.lock ? RS.lock : osal_mutex_create();
    RS.w = RS.h = 4;
    RS.active = true;
    RS.back = osal_malloc(16, OSAL_MEM_EXTERNAL);
    memset(RS.back, 0xFF, 16);
    static const uint8_t data[] = {0, 0, 0, 0, 2, 0, 2, 0, 2, 0, 'S', 'J', 2, 0, 2, 0, 0}; /* 2x2 black, the last of its update */
    region_job_t *j = osal_malloc(sizeof *j + sizeof data, OSAL_MEM_EXTERNAL);
    memset(j, 0, sizeof *j);
    j->gen = RS.gen;
    j->flags = INK_RT_F_LAST;
    j->len = sizeof data;
    memcpy(j->data, data, sizeof data);
    g_jpeg_hook = 1;
    decode_region(j);
    int changed = 0;
    for (int i = 0; i < 16; i++) changed += RS.back[i] != 0xFF;
    CHECK(!g_jpeg_hook && !changed && !RS.ndirty && !ink_exec_waiting(INK_EXEC_UI), "a region whose picture became old while it was decoded changes nothing and shows nothing (%d)", changed);
    RS.active = false;
    release_canvas(NULL);
    CHECK(!RS.back && !RS.px, "the canvases go back");
    RS.w = RS.h = 0;
}

/* A whole update is shown, one for each credit (AUD12-006): two come in while the UI task is busy. */
static ink_exec_job_t g_overlap_setup, g_overlap_block, g_overlap_show;
static osal_sem_t *g_overlap_gate;
static volatile int g_overlap_ready;

static void overlap_noop(lv_timer_t *t) { (void)t; }

static void overlap_setup(void *arg) {
    (void)arg;
    RS.img = lv_obj_create(lv_screen_active());
    RS.credit_timer = lv_timer_create(overlap_noop, 1000, NULL);
    lv_timer_pause(RS.credit_timer);
    RS.spot_timer = lv_timer_create(overlap_noop, 1000, NULL);
    lv_timer_pause(RS.spot_timer);
    RS.first_shown = true;
    RS.owed = 0;
    g_overlap_ready = 1;
}

static void overlap_block(void *arg) { /* the UI task is busy for a while */
    (void)arg;
    g_overlap_ready = 2;
    osal_sem_take(g_overlap_gate, OSAL_WAIT_FOREVER);
}

static void spot_job(void *arg) { /* a tap's feedback on (1, 0), then its timer runs out */
    (void)arg;
    RS.spot = (lv_area_t){1, 0, 1, 0};
    RS.spot_on = true;
    RS.px[1] ^= 0xFF;
    spot_expired(RS.spot_timer);
    g_overlap_ready = 4;
}

static void overlap_clean(void *arg) {
    (void)arg;
    lv_timer_delete(RS.credit_timer);
    lv_timer_delete(RS.spot_timer);
    RS.credit_timer = RS.spot_timer = NULL;
    lv_obj_delete(RS.img);
    RS.img = NULL;
    g_overlap_ready = 3;
}

static void queue_region(int x, bool last) { /* one black pixel at (x, 0) */
    uint8_t p[11] = {(uint8_t)x, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0};
    region_job_t *j = osal_calloc(1, sizeof *j + sizeof p, OSAL_MEM_EXTERNAL);
    j->gen = RS.gen;
    j->len = sizeof p;
    j->flags = last ? INK_RT_F_LAST : 0;
    memcpy(j->data, p, sizeof p);
    osal_mutex_lock(RS.lock, OSAL_WAIT_FOREVER);
    if (g_tail) g_tail->next = j;
    else g_head = j;
    g_tail = j;
    osal_mutex_unlock(RS.lock);
    ink_exec_post(INK_EXEC_MEDIA, &RS.drain_job, drain, NULL);
}

static void overlap_check(void) { /* no page is open: the host's parts stand alone */
    RS.lock = RS.lock ? RS.lock : osal_mutex_create();
    g_overlap_gate = osal_sem_create(0, 1);
    RS.w = RS.h = 4;
    RS.active = true;
    RS.gen++;
    RS.px = osal_malloc(16, OSAL_MEM_EXTERNAL);
    RS.back = osal_malloc(16, OSAL_MEM_EXTERNAL);
    memset(RS.px, 0xFF, 16);
    memset(RS.back, 0xFF, 16);
    ink_exec_post(INK_EXEC_UI, &g_overlap_setup, overlap_setup, NULL);
    while (g_overlap_ready != 1) osal_task_delay_us(1000);
    ink_exec_post(INK_EXEC_UI, &g_overlap_block, overlap_block, NULL);
    while (g_overlap_ready != 2) osal_task_delay_us(1000);
    unsigned updates = RS.updates;

    queue_region(0, true);  /* update A, whole */
    queue_region(1, false); /* the first region of update B */
    osal_task_delay_us(50000);
    CHECK(RS.back[1] == 0xFF, "while A waits to be shown B is not decoded into its picture");
    osal_sem_give(g_overlap_gate);
    osal_task_delay_us(50000);
    CHECK(RS.px[0] == 0 && RS.px[1] == 0xFF && RS.updates == updates + 1 && RS.owed == 1, "A is shown, nothing of B (%d %d, %u, owed %d)", RS.px[0], RS.px[1], RS.updates - updates, RS.owed);
    /* B's first region is in `back` now, not complete: a tap's feedback going away must not show it (AUD13-003) */
    CHECK(RS.back[1] == 0, "B's first region has been decoded");
    ink_exec_post(INK_EXEC_UI, &g_overlap_show, spot_job, NULL);
    while (g_overlap_ready != 4) osal_task_delay_us(1000);
    CHECK(RS.px[1] == 0xFF, "the spot under a tap comes back as it was shown, nothing of B (%d)", RS.px[1]);
    queue_region(2, true); /* the rest of B */
    osal_task_delay_us(50000);
    CHECK(RS.px[1] == 0 && RS.px[2] == 0 && RS.updates == updates + 2 && RS.owed == 2, "then all of B, each with its credit (%d %d, %u, owed %d)", RS.px[1], RS.px[2], RS.updates - updates, RS.owed);
    static ink_exec_job_t clean_job;
    ink_exec_post(INK_EXEC_UI, &clean_job, overlap_clean, NULL);
    while (g_overlap_ready != 3) osal_task_delay_us(1000);
    RS.active = false;
    release_canvas(NULL);
    RS.w = RS.h = 0;
    RS.updates = 0;
    RS.owed = 0;
    RS.first_shown = false;
}

static void remote_checks(void) {
    stale_region_check();
    overlap_check();
    open_remote();
    g_want_updates = 1;
    CHECK(wait_until(shown_enough, 10000000), "the first page is shown");
    CHECK(picture_differs() == 0, "as the hub has it (%d pixels differ)", picture_differs());
    remote_tap(560, 85);
    CHECK(wait_picture(1) && picture_differs() == 0, "a link: the next page, the same on both sides (%d)", picture_differs());

    /* the script drops the link: that hub forgets the session, the next one starts from white */
    for (int round = 0; round < 2; round++) {
        double before = sim_metric_value("remote.resynced");
        int updates = (int)RS.updates;
        for (int w = 0; w < 30000 && !net_lost(); w++) osal_task_delay_us(1000);
        CHECK(net_lost(), "round %d: the link drops", round);
        remote_tap(560, 85); /* lost, said so */
        if (!round) {
            for (int w = 0; w < 15000 && !RS.down_shown; w++) osal_task_delay_us(1000);
            ink_exec_post(INK_EXEC_UI, &g_note_job, note_job, NULL);
            osal_task_delay_us(20000);
            CHECK(RS.down_shown && g_note_visible && g_note_bottom >= 0 && g_note_bottom < RS.h * 42 / 100, "the page says so, above where the answer sheet reaches (%d of %d)", g_note_bottom, RS.h);
        }
        for (int w = 0; w < 30000 && (sim_metric_value("remote.resynced") == before || !is_ready()); w++) osal_task_delay_us(1000);
        g_want_updates = updates + 1;
        CHECK(sim_metric_value("remote.resynced") == before + 1 && wait_until(shown_enough, 8000000), "round %d: reconnected, it pulls again and a picture comes", round);
        CHECK(picture_differs() == 0, "round %d: all of it the hub's new picture (%d)", round, picture_differs());
        remote_tap(560, 85);
        CHECK(wait_picture(1) && picture_differs() == 0, "round %d: links work again (%d)", round, picture_differs());
    }

    /* a region could not be kept: the picture starts over */
    int updates = (int)RS.updates;
    static ink_exec_job_t restart;
    ink_exec_post(INK_EXEC_UI, &restart, session_restart, NULL);
    g_want_updates = updates + 1;
    CHECK(wait_until(shown_enough, 8000000) && picture_differs() == 0, "after a lost region: a new session, the hub's picture (%d)", picture_differs());

    /* closed with an update on its way and opened again: the new page has nothing of the old */
    remote_tap(560, 85);
    osal_task_delay_us(250000);
    close_remote();
    osal_task_delay_us(20000);
    open_remote();
    osal_task_delay_us(20000);
    g_want_updates = 1;
    for (int w = 0; w < 500 && !is_ready(); w++) osal_task_delay_us(10000);
    CHECK(wait_until(shown_enough, 10000000) && picture_differs() == 0, "reopened: its own picture only (%d)", picture_differs());
    close_remote();
    osal_task_delay_us(500000);
    CHECK(!RS.active && !RS.px && !RS.back && !RS.tmp, "closed: the canvases are back");
    CHECK(ink_rt_open_count() == 0, "and the session is closed");
}

/* ------------------------------------------------------------ */

static void home_create(lv_obj_t *scr, void *arg) {
    (void)scr;
    (void)arg;
}
static const ink_activity_t T_HOME = {.name = "t-home", .create = home_create};

static void lifecycle_task(void *arg) {
    (void)arg;
    bool ok = ink_display_init() == 0 && ink_storage_init() == 0 && ink_cap_init() == 0 && ink_exec_init() == 0 && ink_res_init() == 0 && ink_state_init() == 0;
    /* a choice saved before this boot */
    static const char saved[] = "{\"cell.sim_slot\":1}";
    ok = ok && ink_storage_write_atomic(INK_SETTINGS_PATH, saved, sizeof saved - 1) == 0;
    while (ink_storage_pending() > 0) osal_task_delay_us(20000);
    if (!ok || ink_settings_init() != 0 || ink_svc_init() != 0 || ink_data_init() != 0 || ink_power_init() != 0 || ink_net_init() != 0 || ink_hub_init("wss://hub.local/v1/device") != 0 || ink_rt_init() != 0 ||
        ink_tools_init() != 0 || ink_audio_init() != 0 || ink_comm_init() != 0 || ink_remote_init() != 0 || ink_ui_start(&T_HOME) != 0) {
        CHECK(0, "the system services start");
        return;
    }
    ink_members_init();
    g_block = osal_sem_create(0, 1);
    CHECK(ink_cap_register(T_CAPS, (int)(sizeof T_CAPS / sizeof T_CAPS[0])) == 0, "test capabilities register");
    dedupe_checks();
    early_checks();
    deadline_checks();
    pressure_checks();
    ui_pressure_checks();
    pool_checks();
    capture_checks();
    sim_checks();
    sms_checks();
    remote_checks();
    ink_probe_mark("lifecycle-done");
    ink_probe_count("test.finished");
}

void ink_main(void) {
    osal_task_cfg_t cfg = {.name = "lifecycle", .fn = lifecycle_task, .stack_bytes = 8192, .priority = OSAL_PRIO_NORMAL, .core = -1};
    CHECK(osal_task_create(&cfg, NULL) == OSAL_OK, "create the test task");
}
