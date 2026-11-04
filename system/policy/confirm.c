/*
 * The confirmation queue (see ink/sys/confirm.h). A small table under the shared service lock, a shared deadline for
 * the oldest. What the person accepts runs like any call, only as "confirmed": the gate looks again at who is using
 * the device, what a child may do and whether the page is still the one it was asked about.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/confirm.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"

#define MAX_HELD 4
#define EXPIRE_US 60000000ull
#define LIST_CAP 1200

typedef struct {
    bool used;
    uint32_t token;
    const ink_cap_t *cap;
    ink_args_t args;
    ink_source_t src;
    uint32_t page_gen;
    char member[INK_MEMBER_ID_MAX];
    char detail[160];
    uint64_t expires_us;
    bool parent;
} held_t;

static struct {
    bool ready;
    held_t held[MAX_HELD];
    uint32_t next_token;
    ink_svc_deadline_t expiry;
    ink_confirm_presenter_t presenter;
    bool has_presenter;
    ink_exec_job_t present_job;
    int h_pending;
} CF INK_PSRAM_BSS;

/* ---- what the card says ---- */

/* What must not be on a screen: the card shows what was asked, not what was typed. */
static bool hidden_key(const char *k) {
    size_t n = strlen(k);
    return strstr(k, "password") || strstr(k, "secret") || strstr(k, "token") || strstr(k, "psk") || !strcmp(k, "pin") || (n > 4 && !strcmp(k + n - 4, "_pin"));
}

static void put_text(char *out, size_t cap, size_t *n, const char *s) {
    size_t have = strlen(s);
    if (*n + have >= cap) have = cap - *n - 1;
    while (have && (s[have] & 0xC0) == 0x80) have--; /* not in the middle of a character */
    memcpy(out + *n, s, have);
    *n += have;
    out[*n] = 0;
}

static void put_value(char *out, size_t cap, size_t *n, const ink_args_t *a, const char *key) {
    const ink_arg_t *v = ink_args_find(a, key);
    char num[24];
    if (!v) return;
    if (hidden_key(key)) put_text(out, cap, n, "***");
    else if (v->type == INK_ARG_BOOL) put_text(out, cap, n, v->i ? "是" : "否");
    else if (v->type == INK_ARG_INT) {
        snprintf(num, sizeof num, "%lld", (long long)v->i);
        put_text(out, cap, n, num);
    } else if (v->type == INK_ARG_STR || v->type == INK_ARG_JSON) put_text(out, cap, n, a->text + v->off);
}

static void describe(const ink_cap_t *c, const ink_args_t *a, char *out, size_t cap) {
    size_t n = 0;
    out[0] = 0;
    if (c->ask) {
        for (const char *p = c->ask; *p && n < cap - 1;) {
            const char *open = *p == '{' ? strchr(p, '}') : NULL;
            if (open && open - p < 32) {
                char key[32];
                snprintf(key, sizeof key, "%.*s", (int)(open - p - 1), p + 1);
                put_value(out, cap, &n, a, key);
                p = open + 1;
            } else out[n++] = *p++;
        }
        out[n] = 0;
        return;
    }
    for (int i = 0; i < a->n; i++) {
        const char *k = ink_args_key(a, &a->a[i]);
        if (i) put_text(out, cap, &n, "；");
        put_text(out, cap, &n, k);
        put_text(out, cap, &n, "：");
        put_value(out, cap, &n, a, k);
    }
}

/* ---- the table ---- */

static void present_job(void *arg) {
    (void)arg;
    static ink_confirm_view_t view INK_PSRAM_BSS; /* one job node: one at a time */
    /* it makes way for what cannot wait, and a locked screen shows nobody what is being asked */
    char active[16] = "";
    ink_state_get_str(ink_state_find("interrupt.active"), active, sizeof active);
    bool away = ink_power_state() == INK_PWR_LOCKED || (active[0] && strcmp(active, "none"));
    bool any = ink_confirm_list(&view, 1) == 1;
    ink_svc_lock();
    void (*show)(const ink_confirm_view_t *) = CF.has_presenter ? CF.presenter.show : NULL;
    ink_svc_unlock();
    if (show) show(any && !away ? &view : NULL);
}

static void present(void) {
    ink_svc_lock();
    bool has = CF.has_presenter;
    ink_exec_t e = CF.presenter.exec;
    ink_svc_unlock();
    if (has) ink_exec_post(e, &CF.present_job, present_job, NULL);
}

static void expiry_cb(void *arg);

/* The shared deadline for the first to expire, and the count. Outside the lock. */
static void changed(void) {
    uint64_t next = 0;
    int n = 0;
    ink_svc_lock();
    for (int i = 0; i < MAX_HELD; i++)
        if (CF.held[i].used) {
            n++;
            if (!next || CF.held[i].expires_us < next) next = CF.held[i].expires_us;
        }
    ink_svc_unlock();
    ink_state_set_int(CF.h_pending, n);
    if (next) ink_svc_at(&CF.expiry, next, expiry_cb, NULL);
    else ink_svc_cancel(&CF.expiry);
    present();
}

static void tell(uint32_t token, const char *cap, const char *decision, const ink_receipt_t *rc) {
    const char *prev = osal_mem_tag("confirm");
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!a) {
        ink_probe_count("confirm.result_lost");
        return;
    }
    ink_args_init(a);
    ink_args_int(a, "token", token);
    ink_args_str(a, "cap", cap);
    ink_args_str(a, "decision", decision);
    if (rc) {
        ink_args_str(a, "status", ink_rc_name(rc->status));
        if (rc->detail[0]) ink_args_str(a, "detail", rc->detail);
        if (rc->say[0]) ink_args_str(a, "say", rc->say);
        if (rc->object[0]) ink_args_str(a, "object", rc->object);
    }
    ink_cap_emit("confirm.result", a);
    osal_free(a);
    char key[32];
    snprintf(key, sizeof key, "confirm.%s", decision);
    ink_probe_count(key);
}

/* Out of the table, whole: whoever got it is the only one who decides it. */
static bool take(uint32_t token, held_t *out) {
    bool hit = false;
    ink_svc_lock();
    for (int i = 0; i < MAX_HELD && !hit; i++)
        if (CF.held[i].used && CF.held[i].token == token) {
            *out = CF.held[i];
            CF.held[i].used = false;
            hit = true;
        }
    ink_svc_unlock();
    return hit;
}

static void expiry_cb(void *arg) { /* the timer task */
    (void)arg;
    uint64_t now = osal_now_us();
    for (;;) {
        uint32_t token = 0;
        const char *cap = NULL;
        ink_svc_lock();
        for (int i = 0; i < MAX_HELD && !token; i++)
            if (CF.held[i].used && CF.held[i].expires_us <= now) {
                CF.held[i].used = false;
                token = CF.held[i].token;
                cap = CF.held[i].cap->id;
            }
        ink_svc_unlock();
        if (!token) break;
        tell(token, cap, "expired", NULL);
    }
    changed();
}

ink_rc_t ink_confirm_hold(const ink_cap_t *c, const ink_args_t *args, ink_source_t src, uint32_t page_gen, ink_receipt_t *rc) {
    rc->status = INK_RC_PENDING_CONFIRMATION;
    snprintf(rc->detail, sizeof rc->detail, "needs confirmation on the device");
    if (!CF.ready) return rc->status; /* no card in this build: nothing will come of it */
    for (int i = 0; i < args->n; i++)
        if (args->a[i].type == INK_ARG_BLOB) { /* the caller's buffer: gone by the time anyone answers */
            rc->status = INK_RC_REJECTED;
            snprintf(rc->detail, sizeof rc->detail, "cannot wait for a confirmation");
            return rc->status;
        }
    const char *prev = osal_mem_tag("confirm");
    held_t *h = osal_calloc(1, sizeof *h, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!h) {
        rc->status = INK_RC_FAILED;
        snprintf(rc->detail, sizeof rc->detail, "out of memory");
        return rc->status;
    }
    h->used = true;
    h->parent = c->parent_pin && ink_member_any_pin();
    h->cap = c;
    h->args = *args;
    h->src = src;
    h->page_gen = page_gen;
    h->expires_us = osal_now_us() + EXPIRE_US;
    snprintf(h->member, sizeof h->member, "%s", ink_member_current()->id);
    describe(c, args, h->detail, sizeof h->detail);
    bool full = true;
    ink_svc_lock();
    for (int i = 0; i < MAX_HELD && full; i++)
        if (!CF.held[i].used) {
            h->token = ++CF.next_token;
            CF.held[i] = *h;
            full = false;
        }
    ink_svc_unlock();
    uint32_t token = h->token;
    osal_free(h);
    if (full) {
        ink_probe_count("confirm.queue_full");
        rc->status = INK_RC_REJECTED;
        snprintf(rc->detail, sizeof rc->detail, "the confirmation queue is full");
        return rc->status;
    }
    snprintf(rc->object, sizeof rc->object, "confirm:%u", (unsigned)token);
    snprintf(rc->say, sizeof rc->say, "这件事要你在设备上确认一下。");
    changed();
    return rc->status;
}

int ink_confirm_list(ink_confirm_view_t *out, int max) {
    int n = 0;
    ink_svc_lock();
    bool picked[MAX_HELD] = {false};
    while (n < max) {
        const held_t *first = NULL;
        int at = -1;
        for (int i = 0; i < MAX_HELD; i++)
            if (CF.held[i].used && !picked[i] && (!first || CF.held[i].token < first->token)) {
                first = &CF.held[i];
                at = i;
            }
        if (!first) break;
        picked[at] = true;
        ink_confirm_view_t *v = &out[n++];
        memset(v, 0, sizeof *v);
        v->token = first->token;
        snprintf(v->cap, sizeof v->cap, "%s", first->cap->id);
        snprintf(v->title, sizeof v->title, "%s", first->cap->title);
        snprintf(v->detail, sizeof v->detail, "%s", first->detail);
        v->src = (uint8_t)first->src;
        v->parent = first->parent;
        v->expires_us = first->expires_us;
    }
    ink_svc_unlock();
    return n;
}

typedef struct {
    uint32_t token;
    char cap[48];
} ran_t;

static void confirmed_done(void *ctx, const ink_receipt_t *rc) {
    ran_t *r = ctx;
    tell(r->token, r->cap, "accepted", rc);
    osal_free(r);
}

static int decide(uint32_t token, bool accept);

int ink_confirm_decide(uint32_t token, bool accept, ink_source_t src) {
    if (src != INK_SRC_TOUCH && src != INK_SRC_KEY) {
        ink_probe_count("confirm.not_a_person");
        return -1;
    }
    if (accept) { /* a yes that needs a parent comes with a PIN */
        bool parent = false;
        ink_svc_lock();
        for (int i = 0; i < MAX_HELD; i++)
            if (CF.held[i].used && CF.held[i].token == token) parent = CF.held[i].parent;
        ink_svc_unlock();
        if (parent) {
            ink_probe_count("confirm.needs_pin");
            return -1;
        }
    }
    return decide(token, accept);
}

typedef struct {
    uint32_t token;
    ink_pin_done_fn done;
    void *ctx;
} pin_answer_t;

static void pin_checked(void *ctx, int result, uint32_t wait_s) { /* exec.bg */
    pin_answer_t *a = ctx;
    if (result == INK_PIN_OK) decide(a->token, true);
    if (a->done) a->done(a->ctx, result, wait_s);
    osal_free(a);
}

int ink_confirm_accept_with_pin(uint32_t token, const char *pin, ink_pin_done_fn done, void *ctx) {
    pin_answer_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (!a) return -1;
    *a = (pin_answer_t){token, done, ctx};
    if (ink_member_check_parent(pin, pin_checked, a) != 0) {
        osal_free(a);
        return -1;
    }
    return 0;
}

static int decide(uint32_t token, bool accept) {
    const char *prev = osal_mem_tag("confirm");
    held_t *h = osal_malloc(sizeof *h, OSAL_MEM_EXTERNAL);
    ran_t *r = osal_malloc(sizeof *r, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!h || !r || !take(token, h)) {
        osal_free(h);
        osal_free(r);
        return -1;
    }
    changed();
    r->token = token;
    snprintf(r->cap, sizeof r->cap, "%s", h->cap->id);
    if (!accept) tell(token, r->cap, "rejected", NULL);
    else if (strcmp(h->member, ink_member_current()->id)) tell(token, r->cap, "member_changed", NULL);
    else {
        ink_call_opts_t o = {.src = INK_SRC_CONFIRMED, .page_gen = h->page_gen, .reply_exec = INK_EXEC_DIRECT, .done = confirmed_done, .ctx = r};
        int started = ink_cap_call(r->cap, &h->args, &o);
        osal_free(h);
        if (started == 0) return 0; /* confirmed_done frees it */
        ink_receipt_t busy = {.status = INK_RC_FAILED};
        snprintf(busy.detail, sizeof busy.detail, "could not be started");
        tell(token, r->cap, "accepted", &busy);
        osal_free(r);
        return 0;
    }
    osal_free(h);
    osal_free(r);
    return 0;
}

int ink_confirm_withdraw(uint32_t token, ink_source_t src) {
    bool mine = false, person = src == INK_SRC_TOUCH || src == INK_SRC_KEY;
    uint32_t got = 0;
    const char *cap = NULL;
    ink_svc_lock();
    for (int i = 0; i < MAX_HELD; i++)
        if (CF.held[i].used && CF.held[i].token == token) {
            mine = person || CF.held[i].src == src;
            if (mine) {
                CF.held[i].used = false;
                got = token;
                cap = CF.held[i].cap->id;
            }
        }
    ink_svc_unlock();
    if (!got) return -1;
    changed();
    tell(token, cap, "withdrawn", NULL);
    return 0;
}

void ink_confirm_set_presenter(const ink_confirm_presenter_t *p) {
    ink_svc_lock();
    CF.has_presenter = p != NULL;
    if (p) CF.presenter = *p;
    ink_svc_unlock();
    present();
}

/* ---- what moves it ---- */

static void on_member(void *ctx, int h, const char *id) { /* wherever the switch happened: what they asked for ends with them */
    (void)ctx;
    (void)h;
    (void)id;
    const char *now = ink_member_current()->id;
    for (;;) {
        uint32_t token = 0;
        const char *cap = NULL;
        ink_svc_lock();
        for (int i = 0; i < MAX_HELD && !token; i++)
            if (CF.held[i].used && strcmp(CF.held[i].member, now)) {
                CF.held[i].used = false;
                token = CF.held[i].token;
                cap = CF.held[i].cap->id;
            }
        ink_svc_unlock();
        if (!token) return;
        changed();
        tell(token, cap, "member_changed", NULL);
    }
}

static void on_screen_changed(void *ctx, int h, const char *id) { /* with nothing waiting nothing is on screen: the UI sleeps on */
    (void)ctx;
    (void)h;
    (void)id;
    bool any = false;
    ink_svc_lock();
    for (int i = 0; i < MAX_HELD; i++) any = any || CF.held[i].used;
    ink_svc_unlock();
    if (any) present();
}

/* ---- on the bus ---- */

static void fail(ink_call_t *c, ink_rc_t st, const char *why) {
    c->rc.status = st;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
}

static void cap_list(ink_call_t *c) {
    static const char *const SRC[] = {"touch", "key", "ai", "system", "app", "remote", "confirmed"};
    char *out = osal_malloc(LIST_CAP, OSAL_MEM_EXTERNAL);
    ink_confirm_view_t *v = osal_malloc(sizeof *v * MAX_HELD, OSAL_MEM_EXTERNAL);
    if (!out || !v) {
        fail(c, INK_RC_FAILED, "out of memory");
    } else {
        int n = ink_confirm_list(v, MAX_HELD);
        uint64_t now = osal_now_us();
        ink_jw_t w;
        ink_jw_init(&w, out, LIST_CAP);
        ink_jw_raw(&w, "[");
        for (int i = 0; i < n; i++) {
            ink_jw_raw(&w, i ? ",{\"token\":" : "{\"token\":");
            ink_jw_int(&w, v[i].token);
            ink_jw_raw(&w, ",\"cap\":");
            ink_jw_str(&w, v[i].cap);
            ink_jw_raw(&w, ",\"title\":");
            ink_jw_str(&w, v[i].title);
            ink_jw_raw(&w, ",\"detail\":");
            ink_jw_str(&w, v[i].detail);
            ink_jw_raw(&w, ",\"asked_by\":");
            ink_jw_str(&w, SRC[v[i].src]);
            ink_jw_raw(&w, ",\"expires_in_s\":");
            ink_jw_int(&w, v[i].expires_us > now ? (int64_t)((v[i].expires_us - now) / 1000000) : 0);
            ink_jw_raw(&w, "}");
        }
        ink_jw_raw(&w, "]");
        if (w.overflow) fail(c, INK_RC_FAILED, "too long");
        else ink_cap_result(c, out);
    }
    osal_free(v);
    osal_free(out);
}

static void cap_withdraw(ink_call_t *c) {
    int64_t token = 0;
    ink_args_get_int(&c->args, "token", &token);
    if (ink_confirm_withdraw((uint32_t)token, c->src) != 0) fail(c, INK_RC_FAILED, "not waiting, or someone else's");
}

static void cap_accept(ink_call_t *c) {
    int64_t token = 0;
    ink_args_get_int(&c->args, "token", &token);
    if (ink_confirm_decide((uint32_t)token, true, c->src) != 0) fail(c, INK_RC_FAILED, "not waiting any more");
}

static void cap_reject(ink_call_t *c) {
    int64_t token = 0;
    ink_args_get_int(&c->args, "token", &token);
    if (ink_confirm_decide((uint32_t)token, false, c->src) != 0) fail(c, INK_RC_FAILED, "not waiting any more");
}

#define TOKEN_ARG "{\"type\":\"object\",\"properties\":{\"token\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"token\"]}"

static const ink_cap_t CONFIRM_CAPS[] = {
    {.id = "confirm.list", .title = "有什么在等人确认", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_list},
    {.id = "confirm.withdraw", .title = "撤回自己发起、还在等确认的请求", .input = TOKEN_ARG, .effect = INK_FX_NAVIGATE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_withdraw},
    {.id = "confirm.accept", .title = "确认（只有人在设备上能按）", .input = TOKEN_ARG, .effect = INK_FX_NAVIGATE, .child_ok = true, .expose = INK_EXPOSE_OWNER, .exec = INK_EXEC_DIRECT, .run = cap_accept},
    {.id = "confirm.reject", .title = "取消（只有人在设备上能按）", .input = TOKEN_ARG, .effect = INK_FX_NAVIGATE, .child_ok = true, .expose = INK_EXPOSE_OWNER, .exec = INK_EXEC_DIRECT, .run = cap_reject},
    {.id = "confirm.result", .title = "一个待确认的请求结束了（accepted、rejected、expired、withdrawn、member_changed，确认后还有回执）", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

static const ink_state_def_t ST_PENDING = {.id = "confirm.pending", .type = INK_ST_INT, .title = "等人确认的请求数"};

int ink_confirm_init(void) {
    CF.h_pending = ink_state_define(&ST_PENDING);
    ink_state_set_int(CF.h_pending, 0);
    if (ink_cap_register(CONFIRM_CAPS, (int)(sizeof CONFIRM_CAPS / sizeof CONFIRM_CAPS[0])) != 0) return -1;
    if (ink_state_watch("member.current", on_member, NULL, INK_EXEC_DIRECT) != 0 || ink_state_watch("power.state", on_screen_changed, NULL, INK_EXEC_DIRECT) != 0 ||
        ink_state_watch("interrupt.active", on_screen_changed, NULL, INK_EXEC_DIRECT) != 0)
        return -1;
    CF.ready = true;
    return 0;
}
