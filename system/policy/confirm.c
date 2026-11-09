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
#include "ink/sys/data.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "ink/sys/ui.h"
#include "ink/sys/sha256.h"

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
    uint32_t epoch, connection;
    char digest[65], request_id[40];
    ink_exec_job_t job;
    bool ui;
} held_t;

static struct {
    bool ready;
    held_t held[MAX_HELD];
    uint32_t next_token;
    uint32_t limit;
    ink_db_t *db;
    ink_svc_deadline_t expiry;
    ink_confirm_presenter_t presenter;
    bool has_presenter;
    ink_exec_job_t present_job;
    int h_pending;
} CF INK_PSRAM_BSS;

static const ink_db_def_t RESULTS = {.name = "confirm.results", .title = "确认和自动动作的结果", .scope = INK_DB_DEVICE, .schema = 1, .json = true};
static ink_cap_t HUB_CARD[2]; /* Private card templates, never registered as tools. */

/* Only bindings and receipts go to the existing store, never the arguments. */
static uint32_t journal(uint32_t token, const char *state, const ink_receipt_t *rc, const held_t *h, char *digest) {
    if (rc) state = rc->status == INK_RC_DONE || rc->status == INK_RC_UNDONE || rc->status == INK_RC_PARTIAL ? "done" :
        rc->status == INK_RC_QUEUED || rc->status == INK_RC_PENDING_CONFIRMATION ? "running" : "failed";
    struct scratch { ink_json_t j; char out[2048], receipt[1024]; };
    struct scratch *s = osal_calloc(1, sizeof *s, OSAL_MEM_EXTERNAL);
    if (!s) return 0;
    ink_jw_t w = {.buf = s->out, .cap = sizeof s->out};
    if (h) {
        /* Member/capability ids and the hexadecimal digest are system-owned. Escape the request id. */
        int n = snprintf(s->out, sizeof s->out, "{\"binding\":{\"member\":\"%s\",\"cap\":\"%s\",\"digest\":\"%s\",\"page_gen\":%u,\"epoch\":%u,\"connection\":%u,\"expires_us\":%llu,\"request_id\":",
                         h->member, h->cap->id, h->digest, (unsigned)h->page_gen, (unsigned)h->epoch, (unsigned)h->connection, (unsigned long long)h->expires_us);
        if (n < 0 || (size_t)n >= w.cap) { osal_free(s); return 0; }
        w.len = (size_t)n;
        ink_jw_str(&w, h->request_id); ink_jw_raw(&w, "}");
    } else {
        ink_rec_t rec;
        if (ink_db_get(CF.db, token, &rec, s->out, sizeof s->out - 1) != 0 ||
            ink_json_parse(&s->j, s->out, rec.len) != 0) { osal_free(s); return 0; }
        int b = ink_json_find(&s->j, 0, "binding");
        if (b < 0) { osal_free(s); return 0; }
        if (digest) ink_json_str(&s->j, ink_json_find(&s->j, b, "digest"), digest, 65);
        if (!strcmp(state, "accepted") && ink_json_eq(&s->j, ink_json_find(&s->j, 0, "state"), state)) { uint32_t ver = rec.ver; osal_free(s); return ver; }
        w.len = (size_t)s->j.tok[b].end;
        s->out[w.len] = 0;
    }
    ink_jw_raw(&w, ",\"state\":"); ink_jw_str(&w, state);
    if (rc) {
        if (ink_receipt_json(rc, s->receipt, sizeof s->receipt) < 0) w.overflow = true;
        else { ink_jw_raw(&w, ",\"receipt\":"); ink_jw_raw(&w, s->receipt); }
    }
    ink_jw_raw(&w, "}");
    uint32_t ver = 0;
    ink_db_write_t put = {.id = token, .flags = !strcmp(state, "pending") ? INK_DB_LAZY : 0, .sort = (int64_t)token * 4, .data = s->out, .len = w.len, .written_ver = &ver};
    if (w.overflow || ink_db_put(CF.db, &put, NULL) != 0) ver = 0;
    osal_free(s);
    return ver;
}

static uint32_t new_token(void) {
    ink_svc_lock();
    uint32_t token = CF.next_token < CF.limit ? ++CF.next_token : 0;
    ink_svc_unlock();
    return token;
}

static bool bind_args(held_t *h) {
    for (int i = 1; i < h->args.n; i++)
        for (int k = i; k > 0 && strcmp(ink_args_key(&h->args, &h->args.a[k - 1]), ink_args_key(&h->args, &h->args.a[k])) > 0; k--) {
            ink_arg_t a = h->args.a[k]; h->args.a[k] = h->args.a[k - 1]; h->args.a[k - 1] = a;
        }
    char *encoded = osal_malloc(INK_ARGS_TEXT * 2, OSAL_MEM_EXTERNAL); uint8_t digest[32];
    if (!encoded) return false;
    int n = ink_args_to_json(&h->args, encoded, INK_ARGS_TEXT * 2);
    ink_sha256(encoded, n > 0 ? (size_t)n : 0, digest);
    osal_free(encoded);
    for (int i = 0; i < 32; i++) snprintf(h->digest + i * 2, 3, "%02x", digest[i]);
    h->epoch = ink_member_epoch();
    return n > 0;
}

/* ---- what the card says ---- */

/* What must not be on a screen: the card shows what was asked, not what was typed. */
static bool hidden_key(const char *k) {
    size_t n = strlen(k);
    return strstr(k, "password") || strstr(k, "secret") || strstr(k, "token") || strstr(k, "psk") || !strcmp(k, "pin") || (n > 4 && !strcmp(k + n - 4, "_pin"));
}

static bool put_text(char *out, size_t cap, size_t *n, const char *s) {
    size_t have = strlen(s);
    bool complete = *n + have < cap;
    if (*n + have >= cap) have = cap - *n - 1;
    while (have && (s[have] & 0xC0) == 0x80) have--; /* not in the middle of a character */
    memcpy(out + *n, s, have);
    *n += have;
    out[*n] = 0;
    return complete;
}

static bool put_value(char *out, size_t cap, size_t *n, const ink_args_t *a, const char *key) {
    const ink_arg_t *v = ink_args_find(a, key);
    char num[24];
    if (!v) return true;
    if (hidden_key(key)) return put_text(out, cap, n, "***");
    else if (v->type == INK_ARG_BOOL) return put_text(out, cap, n, v->i ? "是" : "否");
    else if (v->type == INK_ARG_INT) {
        snprintf(num, sizeof num, "%lld", (long long)v->i);
        return put_text(out, cap, n, num);
    } else if (v->type == INK_ARG_STR || v->type == INK_ARG_JSON) return put_text(out, cap, n, a->text + v->off);
    return true;
}

static bool describe(const ink_cap_t *c, const ink_args_t *a, char *out, size_t cap) {
    size_t n = 0;
    bool complete = true;
    out[0] = 0;
    if (c->ask) {
        for (const char *p = c->ask; *p;) {
            const char *open = *p == '{' ? strchr(p, '}') : NULL;
            if (open && open - p < 32) {
                char key[32];
                snprintf(key, sizeof key, "%.*s", (int)(open - p - 1), p + 1);
                complete &= put_value(out, cap, &n, a, key);
                p = open + 1;
            } else {
                const char *end = p + 1;
                while ((*end & 0xc0) == 0x80) end++;
                char ch[8];
                snprintf(ch, sizeof ch, "%.*s", (int)(end - p), p);
                complete &= put_text(out, cap, &n, ch);
                p = end;
            }
        }
        if (!strcmp(c->id, "system.factory_reset")) {
            bool media = false; ink_args_get_bool(a, "clear_media", &media);
            complete &= put_text(out, cap, &n, media ? "；同时清空存储卡上的书和音频" : "；存储卡上的书和音频保留");
        }
        out[n] = 0;
        return complete;
    }
    for (int i = 0; i < a->n; i++) {
        const char *k = ink_args_key(a, &a->a[i]);
        if (i) complete &= put_text(out, cap, &n, "；");
        complete &= put_text(out, cap, &n, k);
        complete &= put_text(out, cap, &n, "：");
        complete &= put_value(out, cap, &n, a, k);
    }
    return complete;
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

void ink_confirm_result(uint32_t token, const char *cap, const char *decision, const ink_receipt_t *rc, const char *who) {
    char digest[65] = "";
    uint32_t ver = journal(token, decision, rc, NULL, digest);
    const char *prev = osal_mem_tag("confirm");
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!a) {
        ink_probe_count("confirm.result_lost");
        return;
    }
    ink_args_init(a);
    ink_args_int(a, "token", token);
    ink_args_int(a, "result_ver", ver);
    ink_args_str(a, "cap", cap);
    ink_args_str(a, "decision", decision);
    if (!strcmp(cap, "hub.confirm")) ink_args_str(a, "digest", digest);
    ink_args_str(a, "member", who); /* whose it was: not told to whoever came after */
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
        char who[INK_MEMBER_ID_MAX] = "";
        ink_svc_lock();
        for (int i = 0; i < MAX_HELD && !token; i++)
            if (CF.held[i].used && CF.held[i].expires_us <= now) {
                CF.held[i].used = false;
                token = CF.held[i].token;
                cap = CF.held[i].cap->id;
                snprintf(who, sizeof who, "%s", CF.held[i].member);
            }
        ink_svc_unlock();
        if (!token) break;
        ink_confirm_result(token, cap, "expired", NULL, who);
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
    h->page_gen = page_gen ? page_gen : (c->activity ? ink_ui_content_version() : 0);
    if (!bind_args(h)) { osal_free(h); rc->status = INK_RC_FAILED; return rc->status; }
    h->expires_us = osal_now_us() + EXPIRE_US;
    if (c == HUB_CARD || c == HUB_CARD + 1) {
        int64_t ms = 0;
        ink_args_get_str(args, "digest", h->digest, sizeof h->digest);
        ink_args_get_int(args, "expires_ms", &ms);
        h->expires_us = osal_now_us() + (uint64_t)ms * 1000;
        h->connection = ink_hub_connection();
    }
    snprintf(h->member, sizeof h->member, "%s", ink_member_current()->id);
    if (!describe(c, args, h->detail, sizeof h->detail)) {
        osal_free(h);
        rc->status = INK_RC_REJECTED;
        snprintf(rc->detail, sizeof rc->detail, "confirmation detail is too long");
        snprintf(rc->say, sizeof rc->say, "说明太长，不能完整显示，请分成几次操作。");
        return rc->status;
    }
    h->token = new_token();
    bool full = true;
    ink_svc_lock();
    for (int i = 0; h->token && i < MAX_HELD; i++)
        if (!CF.held[i].used) {
            CF.held[i] = *h;
            full = false;
            break;
        }
    ink_svc_unlock();
    uint32_t token = h->token;
    if (!full && !journal(token, "pending", NULL, h, NULL)) { take(token, h); full = true; }
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
        if (first->cap == HUB_CARD || first->cap == HUB_CARD + 1) ink_args_get_str(&first->args, "title", v->title, sizeof v->title);
        snprintf(v->detail, sizeof v->detail, "%s", first->detail);
        v->src = (uint8_t)first->src;
        v->parent = first->parent;
        v->expires_us = first->expires_us;
    }
    ink_svc_unlock();
    return n;
}

static void confirmed_done(void *ctx, const ink_receipt_t *rc) {
    held_t *h = ctx;
    ink_confirm_result(h->token, h->cap->id, "accepted", rc, h->member);
    osal_free(h);
}

static void accept_job(void *arg) { /* Commit off UI, then return to the caller's executor. */
    held_t *h = arg;
    if (strcmp(h->member, ink_member_current()->id) || h->epoch != ink_member_epoch()) {
        ink_confirm_result(h->token, h->cap->id, "member_changed", NULL, h->member);
        osal_free(h);
        return;
    }
    ink_receipt_t rc = {.status = INK_RC_FAILED};
    bool hub = h->cap == HUB_CARD || h->cap == HUB_CARD + 1;
    if (!ink_ui_is_current()) {
        uint32_t ver = journal(h->token, hub ? "accepted" : "running", NULL, NULL, NULL);
        int64_t remaining = (int64_t)h->expires_us - (int64_t)osal_now_us();
        if (!ver || remaining <= 0 || ink_db_wait(CF.db, ver, remaining) != 0) {
            snprintf(rc.detail, sizeof rc.detail, "confirmation could not be committed");
            confirmed_done(h, &rc); return;
        }
    }
    const char *invalid = strcmp(h->member, ink_member_current()->id) || h->epoch != ink_member_epoch() ? "member_changed" :
        osal_now_us() >= h->expires_us || (hub && !ink_hub_member_matches(h->member, h->epoch, h->connection)) ? "expired" : NULL;
    if (invalid) { ink_confirm_result(h->token, h->cap->id, invalid, NULL, h->member); osal_free(h); return; }
    if (h->ui && !ink_ui_is_current()) { ink_exec_post(INK_EXEC_UI, &h->job, accept_job, h); return; }
    if (hub) { ink_confirm_result(h->token, h->cap->id, "accepted", NULL, h->member); osal_free(h); return; }
    ink_call_opts_t o = {.src = INK_SRC_CONFIRMED, .page_gen = h->page_gen, .member = h->member,
                        .reply_exec = INK_EXEC_DIRECT, .done = confirmed_done, .ctx = h};
    if (ink_cap_call(h->cap->id, &h->args, &o) == 0) return;
    confirmed_done(h, &rc);
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
    held_t *h = osal_malloc(sizeof *h, OSAL_MEM_EXTERNAL);
    if (!h || !take(token, h)) { osal_free(h); return -1; }
    h->ui = ink_ui_is_current();
    changed();
    if (!accept) { ink_confirm_result(token, h->cap->id, "rejected", NULL, h->member); osal_free(h); return 0; }
    if (!h->ui) { accept_job(h); return 0; }
    if (ink_exec_post(INK_EXEC_BG, &h->job, accept_job, h) == 0) return 0;
    osal_free(h);
    return -1;
}

int ink_confirm_withdraw(uint32_t token, ink_source_t src) {
    char who[INK_MEMBER_ID_MAX] = "";
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
                snprintf(who, sizeof who, "%s", CF.held[i].member);
            }
        }
    ink_svc_unlock();
    if (!got) return -1;
    changed();
    ink_confirm_result(token, cap, "withdrawn", NULL, who);
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
        char who[INK_MEMBER_ID_MAX] = "";
        ink_svc_lock();
        for (int i = 0; i < MAX_HELD && !token; i++)
            if (CF.held[i].used && strcmp(CF.held[i].member, now)) {
                CF.held[i].used = false;
                token = CF.held[i].token;
                cap = CF.held[i].cap->id;
                snprintf(who, sizeof who, "%s", CF.held[i].member);
            }
        ink_svc_unlock();
        if (!token) return;
        changed();
        ink_confirm_result(token, cap, "member_changed", NULL, who);
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

static bool read_result(uint32_t *token, const char *rid, char *out, ink_json_t *j, ink_rec_t *rec) {
    ink_db_cursor_t at = {0};
    do {
        int found = *token ? ink_db_get(CF.db, *token, rec, out, 2047) == 0 : ink_db_next(CF.db, &at, rec, out, 2047) == 1;
        if (!found) return false;
        out[rec->len] = 0;
        if (rec->id == 1 || ink_json_parse(j, out, rec->len) != 0) continue;
        char saved_rid[40];
        if (ink_json_eq(j, ink_json_find(j, 0, "binding.member"), ink_member_current()->id) &&
            (*token || (rid && *rid && ink_json_str(j, ink_json_find(j, 0, "binding.request_id"), saved_rid, sizeof saved_rid) >= 0 && !strcmp(saved_rid, rid)))) {
            *token = rec->id; return true;
        }
        if (*token) return false;
    } while (!*token);
    return false;
}

uint32_t ink_confirm_track(const ink_cap_t *cap, const ink_call_opts_t *o, ink_receipt_t *rc) { /* exec.bg */
    struct scratch { held_t h; ink_json_t j; char out[2048]; };
    struct scratch *s = osal_calloc(1, sizeof *s, OSAL_MEM_EXTERNAL);
    rc->status = INK_RC_FAILED;
    if (!s) return 0;
    held_t *h = &s->h; h->cap = cap; h->page_gen = o->page_gen;
    snprintf(h->member, sizeof h->member, "%s", o->member);
    snprintf(h->request_id, sizeof h->request_id, "%s", o->request_id ? o->request_id : "");
    const char *json = o->args_json && *o->args_json ? o->args_json : "{}";
    if (ink_args_from_json(&h->args, json, strlen(json), cap->input, rc->detail, sizeof rc->detail) || !bind_args(h)) { osal_free(s); return 0; }
    uint32_t token = 0; ink_rec_t rec;
    if (h->request_id[0] && read_result(&token, h->request_id, s->out, &s->j, &rec)) {
        int64_t page = 0;
        rc->status = ink_json_eq(&s->j, ink_json_find(&s->j, 0, "binding.digest"), h->digest) &&
            ink_json_eq(&s->j, ink_json_find(&s->j, 0, "binding.cap"), cap->id) &&
            ink_json_int(&s->j, ink_json_find(&s->j, 0, "binding.page_gen"), &page) && page == h->page_gen ? INK_RC_QUEUED : INK_RC_STALE;
        osal_free(s); return 0;
    }
    token = new_token();
    uint32_t ver = token ? journal(token, "running", NULL, h, NULL) : 0;
    bool saved = ver && ink_db_wait(CF.db, ver, 5000000) == 0;
    osal_free(s);
    return saved ? token : 0;
}

void ink_confirm_ack(uint32_t token, uint32_t ver) {
    char *out = osal_malloc(2048, OSAL_MEM_EXTERNAL);
    ink_rec_t rec;
    if (out && token > 1 && !ink_db_get(CF.db, token, &rec, out, 2048) && rec.ver == ver) {
        /* Low bits: received, and receipt already durable before this metadata write. */
        ink_db_write_t put = {.id = token, .sort = (int64_t)token * 4 + 1 + (rec.ver <= ink_db_durable_ver(CF.db) ? 2 : 0), .data = out, .len = rec.len};
        ink_db_put(CF.db, &put, NULL);
    }
    osal_free(out);
    ink_db_cursor_t at = {.desc = true};
    int keep = 16;
    while (ink_db_next(CF.db, &at, &rec, NULL, 0) == 1)
        if (rec.id != 1 && keep-- <= 0 && rec.sort / 4 == rec.id && (rec.sort & 1))
            ink_db_del_if_ver(CF.db, rec.id, INK_SRC_SYSTEM, rec.ver);
}

static void cap_status(ink_call_t *c) {
    struct scratch { ink_json_t j; char out[2048], rid[40]; };
    struct scratch *s = osal_calloc(1, sizeof *s, OSAL_MEM_EXTERNAL);
    if (!s) { fail(c, INK_RC_FAILED, "out of memory"); return; }
    int64_t requested = 0; ink_rec_t rec;
    ink_args_get_int(&c->args, "token", &requested);
    ink_args_get_str(&c->args, "request_id", s->rid, sizeof s->rid);
    uint32_t token = (uint32_t)requested;
    if (!read_result(&token, s->rid, s->out, &s->j, &rec)) ink_cap_result(c, "{\"state\":\"unknown\"}");
    else {
        const char *state = NULL;
        int st = ink_json_find(&s->j, 0, "state");
        if (ink_json_eq(&s->j, st, "running") || (rec.ver > ink_db_durable_ver(CF.db) && rec.sort != (int64_t)token * 4 + 3)) state = "unknown";
        if (ink_json_eq(&s->j, st, "pending")) {
            bool live = false;
            ink_svc_lock();
            for (int i = 0; i < MAX_HELD; i++) live |= CF.held[i].used && CF.held[i].token == token;
            ink_svc_unlock();
            if (!live) state = "expired";
        }
        int64_t epoch = 0, connection = 0, expires = 0;
        if (ink_json_eq(&s->j, st, "accepted") && (token <= CF.limit - 65536 || token > CF.limit ||
            !ink_json_int(&s->j, ink_json_find(&s->j, 0, "binding.expires_us"), &expires) || expires <= 0 || osal_now_us() >= (uint64_t)expires ||
            !ink_json_int(&s->j, ink_json_find(&s->j, 0, "binding.epoch"), &epoch) || !ink_json_int(&s->j, ink_json_find(&s->j, 0, "binding.connection"), &connection) ||
            !ink_hub_member_matches(ink_member_current()->id, (uint32_t)epoch, (uint32_t)connection))) state = "expired";
        if (state) { char result[48]; snprintf(result, sizeof result, "{\"state\":\"%s\"}", state); ink_cap_result(c, result); }
        else ink_cap_result(c, s->out);
    }
    osal_free(s);
}

static int request_text(const ink_json_t *j, int t, char *out, size_t cap) {
    return t < 0 || (size_t)(j->tok[t].end - j->tok[t].start) >= cap ? -1 : ink_json_str(j, t, out, cap);
}

static void on_request(void *ctx, int64_t id, const char *method, const ink_json_t *j, int p) {
    (void)ctx;
    char member[INK_MEMBER_ID_MAX], target[INK_MEMBER_ID_MAX]; int64_t epoch = 0, ms = 0;
    if (strcmp(method, "confirm/request")) { ink_hub_respond_error(id, -32601, "unknown confirm method"); return; }
    if (request_text(j, ink_json_find(j, p, "_meta.member"), member, sizeof member) <= 0 ||
        !ink_json_int(j, ink_json_find(j, p, "_meta.epoch"), &epoch) || epoch <= 0 || epoch > UINT32_MAX ||
        request_text(j, ink_json_find(j, p, "member"), target, sizeof target) <= 0 || strcmp(member, target) ||
        !ink_hub_member_matches(member, (uint32_t)epoch, ink_hub_connection())) { ink_hub_respond_error(id, -32023, "stale_member"); return; }
    ink_args_t *a = osal_calloc(1, sizeof *a, OSAL_MEM_EXTERNAL); char title[48], body[160], digest[65];
    int parent = ink_json_find(j, p, "parent");
    bool valid = a && request_text(j, ink_json_find(j, p, "title"), title, sizeof title) > 0 &&
        request_text(j, ink_json_find(j, p, "body"), body, sizeof body) > 0 &&
        request_text(j, ink_json_find(j, p, "digest"), digest, sizeof digest) == 64 && strspn(digest, "0123456789abcdef") == 64 &&
        parent >= 0 && j->tok[parent].type == 8 && (ink_json_eq(j, parent, "true") || ink_json_eq(j, parent, "false")) &&
        ink_json_int(j, ink_json_find(j, p, "expires_ms"), &ms) && ms > 0 && ms <= 60000;
    if (!valid) { osal_free(a); ink_hub_respond_error(id, -32602, "invalid confirmation"); return; }
    valid = !ink_args_str(a, "title", title) && !ink_args_str(a, "body", body) && !ink_args_str(a, "digest", digest) && !ink_args_int(a, "expires_ms", ms);
    ink_receipt_t rc = {0};
    if (!valid || ink_confirm_hold(&HUB_CARD[ink_json_eq(j, parent, "true")], a, INK_SRC_REMOTE, 0, &rc) != INK_RC_PENDING_CONFIRMATION)
        ink_hub_respond_error(id, -32603, "cannot queue confirmation");
    else { unsigned token = 0; char out[40]; sscanf(rc.object, "confirm:%u", &token); snprintf(out, sizeof out, "{\"token\":%u}", token); ink_hub_respond(id, out); }
    osal_free(a);
}

#define TOKEN_ARG "{\"type\":\"object\",\"properties\":{\"token\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"token\"]}"

static const ink_cap_t CONFIRM_CAPS[] = {
    {.id = "confirm.status", .title = "查询确认和自动动作的结果", .input = "{\"type\":\"object\",\"properties\":{\"token\":{\"type\":\"integer\",\"minimum\":2,\"maximum\":4294967295},\"request_id\":{\"type\":\"string\",\"maxLength\":39}}}", .output = "{\"type\":\"object\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_status},
    {.id = "confirm.list", .title = "有什么在等人确认", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_list},
    {.id = "confirm.withdraw", .title = "撤回自己发起、还在等确认的请求", .input = TOKEN_ARG, .effect = INK_FX_NAVIGATE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_withdraw},
    {.id = "confirm.accept", .title = "确认（只有人在设备上能按）", .input = TOKEN_ARG, .effect = INK_FX_NAVIGATE, .child_ok = true, .expose = INK_EXPOSE_OWNER, .exec = INK_EXEC_DIRECT, .run = cap_accept},
    {.id = "confirm.reject", .title = "取消（只有人在设备上能按）", .input = TOKEN_ARG, .effect = INK_FX_NAVIGATE, .child_ok = true, .expose = INK_EXPOSE_OWNER, .exec = INK_EXEC_DIRECT, .run = cap_reject},
    {.id = "confirm.result", .title = "一个待确认的请求结束了（accepted、rejected、expired、withdrawn、member_changed，确认后还有回执）", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

static const ink_state_def_t ST_PENDING = {.id = "confirm.pending", .type = INK_ST_INT, .title = "等人确认的请求数"};

int ink_confirm_init(void) {
    HUB_CARD[0] = (ink_cap_t){.id = "hub.confirm", .title = "中枢请求确认", .ask = "{body}"};
    HUB_CARD[1] = HUB_CARD[0];
    HUB_CARD[1].parent_pin = true;
    if (ink_data_init() || ink_db_register(&RESULTS) || !(CF.db = ink_db_open(&RESULTS, NULL))) return -1;
    ink_rec_t rec = {.sort = 1};
    if (ink_db_get(CF.db, 1, &rec, NULL, 0) && ink_db_count(CF.db)) return -1;
    if (rec.sort < 1 || rec.sort > UINT32_MAX - 65536) return -1;
    CF.next_token = (uint32_t)rec.sort; CF.limit = (uint32_t)rec.sort + 65536;
    uint32_t ver = 0;
    ink_db_write_t put = {.id = 1, .sort = CF.limit, .data = "{}", .len = 2, .written_ver = &ver};
    if (ink_db_put(CF.db, &put, NULL) || ink_db_wait(CF.db, ver, 5000000) || ink_hub_on_request("confirm/", on_request, NULL)) return -1;
    CF.h_pending = ink_state_define(&ST_PENDING);
    ink_state_set_int(CF.h_pending, 0);
    if (ink_cap_register(CONFIRM_CAPS, (int)(sizeof CONFIRM_CAPS / sizeof CONFIRM_CAPS[0])) != 0) return -1;
    if (ink_state_watch("member.current", on_member, NULL, INK_EXEC_DIRECT) != 0 || ink_state_watch("power.state", on_screen_changed, NULL, INK_EXEC_DIRECT) != 0 ||
        ink_state_watch("interrupt.active", on_screen_changed, NULL, INK_EXEC_DIRECT) != 0)
        return -1;
    CF.ready = true;
    return 0;
}
