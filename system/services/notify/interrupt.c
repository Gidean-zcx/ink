/*
 * The interruption channel (see ink/sys/interrupt.h). Raising does the three things
 * right here, in whatever task raised it: the screen comes on (the device stays
 * "locked underneath": it locks again when the last one ends), the sound starts in
 * the audio task, and a job goes to the shell. So a slow app, a full refresh in
 * progress or a network wait cannot hold up an incoming call or an alarm.
 *
 * The table is under the shared service lock; nothing else is done under it.
 */
#include <stdio.h>
#include <string.h>

#include "ink/hal_display.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/audio.h"
#include "ink/sys/cap.h"
#include "ink/sys/input.h"
#include "ink/sys/interrupt.h"
#include "ink/sys/json.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "notify_internal.h"

#define MAX_INT 4
#define WAKE_HOLD_MS 1000
#define LIST_CAP 1200

static const char *const KIND_NAME[] = {"low_battery", "alarm", "call"};

static struct {
    ink_interrupt_t it[MAX_INT];
    int n, next_id;
    bool holding;  /* the screen and the chip are kept up for us */
    bool relock;   /* it was locked underneath: it locks again when the last one ends */
    bool ring_on;  /* what the audio service was last told */
    bool applying, apply_again;
    ink_int_presenter_t presenter;
    bool has_presenter;
    ink_exec_job_t show_job, power_key_job;
    int h_active;
} IT INK_PSRAM_BSS;

/* Under the lock: the one that should be on screen. */
static ink_interrupt_t *top_locked(void) {
    ink_interrupt_t *top = NULL;
    for (int i = 0; i < IT.n; i++)
        if (!top || IT.it[i].kind > top->kind || (IT.it[i].kind == top->kind && IT.it[i].id > top->id)) top = &IT.it[i];
    return top;
}

static void show_job(void *arg) { /* on the presenter's executor: the newest state, whenever it runs */
    (void)arg;
    static ink_interrupt_t copy INK_PSRAM_BSS; /* one job node: this runs once at a time */
    ink_svc_lock();
    const ink_interrupt_t *top = top_locked();
    bool any = top != NULL;
    if (any) copy = *top;
    void (*show)(const ink_interrupt_t *) = IT.has_presenter ? IT.presenter.show : NULL;
    ink_svc_unlock();
    if (show) show(any ? &copy : NULL);
}

/* What the table says now: power, sound, state, the shell, all outside the lock and never waiting. One task at a time does
 * it: one that comes while another is at it says there is more and goes (audit AUD15-006), so these never get applied in
 * the other order and what is left in the end is what the table says. */
static void apply(void) {
    ink_svc_lock();
    if (IT.applying) {
        IT.apply_again = true;
        ink_svc_unlock();
        return;
    }
    IT.applying = true;
    for (;;) {
        IT.apply_again = false;
        const ink_interrupt_t *top = top_locked();
        bool any = top != NULL, want_ring = top && top->ringing;
        ink_int_kind_t kind = any ? top->kind : INK_INT_LOW_BATTERY;
        bool hold = any && !IT.holding, release = !any && IT.holding, relock = release && IT.relock;
        bool ring_changed = want_ring != IT.ring_on;
        if (hold) IT.holding = true;
        if (release) {
            IT.holding = false;
            IT.relock = false;
        }
        IT.ring_on = want_ring;
        ink_exec_t exec = IT.has_presenter ? IT.presenter.exec : INK_EXEC_UI;
        bool present = IT.has_presenter;
        ink_svc_unlock();
        if (hold) {
            ink_power_hold(INK_NEED_AWAKE);
            ink_power_hold(INK_NEED_NO_LOCK);
        }
        if (ring_changed) ink_audio_ring(want_ring);
        ink_state_set_str(IT.h_active, any ? KIND_NAME[kind] : "none");
        if (present) ink_exec_post(exec, &IT.show_job, show_job, NULL);
        if (release) {
            ink_power_release(INK_NEED_NO_LOCK);
            ink_power_release(INK_NEED_AWAKE);
            if (relock) ink_power_lock();
        }
        ink_svc_lock();
        if (!IT.apply_again) break;
    }
    IT.applying = false;
    ink_svc_unlock();
}

int ink_interrupt_raise(const ink_interrupt_t *it) {
    uint64_t now = osal_now_us();
    ink_svc_lock();
    if (IT.n >= MAX_INT) {
        ink_svc_unlock();
        ink_probe_count("interrupt.table_full");
        return -1;
    }
    ink_interrupt_t *e = &IT.it[IT.n++];
    *e = *it;
    int id = e->id = ++IT.next_id;
    e->raised_us = now;
    ink_svc_unlock();
    ink_probe_count("interrupt.raised");
    if (ink_power_state() == INK_PWR_LOCKED) {
        ink_svc_lock();
        IT.relock = true;
        ink_svc_unlock();
        hal_display_wake(WAKE_HOLD_MS); /* the panel is powered before anything is drawn */
        ink_power_unlock(now);
    }
    apply();
    return id;
}

void ink_interrupt_end(int id) {
    ink_svc_lock();
    bool hit = false;
    for (int i = 0; i < IT.n && !hit; i++)
        if (IT.it[i].id == id) {
            memmove(&IT.it[i], &IT.it[i + 1], sizeof IT.it[0] * (size_t)(IT.n - i - 1));
            IT.n--;
            hit = true;
        }
    ink_svc_unlock();
    if (hit) apply();
}

void ink_interrupt_update(int id, const char *body, const ink_int_action_t *actions, int nactions, bool ringing) {
    ink_svc_lock();
    for (int i = 0; i < IT.n; i++)
        if (IT.it[i].id == id) {
            if (body) snprintf(IT.it[i].body, sizeof IT.it[i].body, "%s", body);
            if (actions && nactions <= 3) {
                memcpy(IT.it[i].actions, actions, sizeof actions[0] * (size_t)nactions);
                IT.it[i].nactions = nactions;
            }
            IT.it[i].ringing = ringing;
        }
    ink_svc_unlock();
    apply();
}

int notify_interrupt_find(ink_int_kind_t kind, uint32_t ref) {
    int id = 0;
    ink_svc_lock();
    for (int i = 0; i < IT.n && !id; i++)
        if (IT.it[i].kind == kind && IT.it[i].ref == ref) id = IT.it[i].id;
    ink_svc_unlock();
    return id;
}

static void action_done(void *ctx, const ink_receipt_t *rc) {
    (void)ctx;
    if (rc->status != INK_RC_DONE) ink_probe_count("interrupt.action_failed");
}

int ink_interrupt_action(int id, int index, uint8_t src) {
    char cap[48] = "", args[64] = "";
    bool found = false;
    ink_svc_lock();
    for (int i = 0; i < IT.n; i++)
        if (IT.it[i].id == id && index >= 0 && index < IT.it[i].nactions) {
            found = true;
            if (IT.it[i].actions[index].cap) {
                snprintf(cap, sizeof cap, "%s", IT.it[i].actions[index].cap);
                snprintf(args, sizeof args, "%s", IT.it[i].actions[index].args);
            }
        }
    ink_svc_unlock();
    if (!found) return -1;
    if (!cap[0]) {
        ink_interrupt_end(id);
        return 0;
    }
    ink_call_opts_t o = {.src = (ink_source_t)src, .args_json = args[0] ? args : NULL, .reply_exec = INK_EXEC_DIRECT, .done = action_done};
    return ink_cap_call(cap, NULL, &o);
}

void ink_interrupt_set_presenter(const ink_int_presenter_t *p) {
    ink_svc_lock();
    IT.has_presenter = p != NULL;
    if (p) IT.presenter = *p;
    ink_svc_unlock();
    apply(); /* something may already be up */
}

bool ink_interrupt_active(void) {
    ink_svc_lock();
    bool any = IT.n > 0;
    ink_svc_unlock();
    return any;
}

/* What the power key does here: nothing it does hangs up a call by accident. Not on the input task: its stack
 * is small and a capability's call is not. */
static void power_key_job(void *arg) { /* exec.bg */
    (void)arg;
    ink_svc_lock();
    ink_interrupt_t *top = top_locked();
    int id = top ? top->id : 0;
    ink_int_kind_t kind = top ? top->kind : INK_INT_LOW_BATTERY;
    bool silenced = top && kind == INK_INT_CALL && top->ringing;
    if (silenced) top->ringing = false; /* the call stays on screen, quiet */
    ink_svc_unlock();
    if (silenced) apply();
    else if (id && kind == INK_INT_ALARM) ink_interrupt_action(id, 1, INK_SRC_KEY); /* the second button: snooze */
    else if (id && kind == INK_INT_LOW_BATTERY) ink_interrupt_end(id);
}

bool ink_interrupt_power_key(void) {
    if (!ink_interrupt_active()) return false;
    ink_exec_post(INK_EXEC_BG, &IT.power_key_job, power_key_job, NULL);
    return true;
}

/* Something locked the device with one of these up (a capability, a race with the last one ending). */
static void on_power(void *ctx, ink_power_state_t from, ink_power_state_t to, uint64_t cause_us) { /* power task */
    (void)ctx;
    (void)from;
    (void)cause_us;
    if (to != INK_PWR_LOCKED) return;
    ink_svc_lock();
    bool any = IT.n > 0;
    if (any) IT.relock = true;
    ink_svc_unlock();
    if (any) {
        ink_probe_count("interrupt.lock_undone");
        hal_display_wake(WAKE_HOLD_MS);
        ink_power_unlock(osal_now_us());
    }
}

/* ---- on the bus: what is on screen, and closing what only asks to be read ---- */

static void cap_ack(ink_call_t *c) {
    int id = notify_interrupt_find(INK_INT_LOW_BATTERY, 0);
    if (id) ink_interrupt_end(id);
    else {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "nothing to acknowledge");
    }
}

static void cap_list(ink_call_t *c) {
    char *out = osal_malloc(LIST_CAP, OSAL_MEM_EXTERNAL);
    ink_interrupt_t *copy = osal_malloc(sizeof IT.it, OSAL_MEM_EXTERNAL);
    if (!out || !copy) {
        osal_free(out);
        osal_free(copy);
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "out of memory");
        return;
    }
    ink_svc_lock();
    int n = IT.n;
    memcpy(copy, IT.it, sizeof IT.it);
    ink_svc_unlock();
    ink_jw_t w;
    ink_jw_init(&w, out, LIST_CAP);
    ink_jw_raw(&w, "[");
    for (int i = 0; i < n; i++) {
        ink_jw_raw(&w, i ? ",{\"kind\":" : "{\"kind\":");
        ink_jw_str(&w, KIND_NAME[copy[i].kind]);
        ink_jw_raw(&w, ",\"title\":");
        ink_jw_str(&w, copy[i].title);
        ink_jw_raw(&w, ",\"body\":");
        ink_jw_str(&w, copy[i].body);
        ink_jw_raw(&w, copy[i].ringing ? ",\"ringing\":true,\"actions\":[" : ",\"ringing\":false,\"actions\":[");
        for (int a = 0; a < copy[i].nactions; a++) {
            ink_jw_raw(&w, a ? ",{\"label\":" : "{\"label\":");
            ink_jw_str(&w, copy[i].actions[a].label);
            ink_jw_raw(&w, ",\"cap\":");
            ink_jw_str(&w, copy[i].actions[a].cap ? copy[i].actions[a].cap : "");
            if (copy[i].actions[a].args[0]) {
                ink_jw_raw(&w, ",\"args\":");
                ink_jw_raw(&w, copy[i].actions[a].args);
            }
            ink_jw_raw(&w, "}");
        }
        ink_jw_raw(&w, "]}");
    }
    ink_jw_raw(&w, "]");
    if (w.overflow) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "too long");
    } else ink_cap_result(c, out);
    osal_free(copy);
    osal_free(out);
}

static const ink_cap_t INT_CAPS[] = {
    {.id = "interrupt.list", .title = "现在有什么在打断（来电、闹钟、低电量），各有什么按钮", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .while_locked = true, .exec = INK_EXEC_DIRECT, .run = cap_list},
    {.id = "interrupt.ack", .title = "知道了（关掉低电量提示）", .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .exec = INK_EXEC_DIRECT, .run = cap_ack},
};

static const ink_state_def_t ST_ACTIVE = {.id = "interrupt.active", .type = INK_ST_STR, .title = "正在打断（none、call、alarm、low_battery）"};

int notify_interrupt_init(void) {
    IT.h_active = ink_state_define(&ST_ACTIVE);
    ink_state_set_str(IT.h_active, "none");
    ink_input_set_power_key_hook(ink_interrupt_power_key);
    if (ink_power_subscribe(on_power, NULL) != 0) return -1;
    return ink_cap_register(INT_CAPS, (int)(sizeof INT_CAPS / sizeof INT_CAPS[0]));
}
