/*
 * Capability registry, policy gate and receipts (see ink/sys/cap.h), plus the
 * hub's "tools/list" / "tools/call" (the AI's way in, D-12: MCP tool and
 * result structures inside the device's own JSON-RPC envelope).
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/ui.h"

#define MAX_CAPS 96

static const ink_cap_t *REG[MAX_CAPS];
static int nreg;

static const char *RC_NAME[] = {"done",     "partial", "queued", "pending_confirmation", "rejected",
                                "stale",    "invalid", "failed", "cancelled",            "undone"};
static const char *FX_NAME[] = {"read", "navigate", "local_write", "local_destructive", "external", "system"};
static const char *CONFIRM_NAME[] = {"none", "device", "parent"};
static const char *FORM_NAME[] = {"call", "stream", "event", "session"};

const char *ink_rc_name(ink_rc_t rc) { return (unsigned)rc < sizeof RC_NAME / sizeof RC_NAME[0] ? RC_NAME[rc] : "?"; }
const char *ink_effect_name(ink_effect_t fx) { return (unsigned)fx < sizeof FX_NAME / sizeof FX_NAME[0] ? FX_NAME[fx] : "?"; }

int ink_cap_register(const ink_cap_t *table, int n) {
    for (int i = 0; i < n; i++) {
        if (nreg >= MAX_CAPS) return -1;
        REG[nreg++] = &table[i];
    }
    return 0;
}

const ink_cap_t *ink_cap_find(const char *id) {
    for (int i = 0; i < nreg; i++)
        if (!strcmp(REG[i]->id, id)) return REG[i];
    return NULL;
}

static bool on_its_page(const ink_cap_t *c) { return !c->activity || !strcmp(ink_ui_top_name(), c->activity); }

static bool in_directory(const char *id); /* the hub's directory, below */

/* Local first; otherwise what the hub offers, while it is reachable (architecture §4.6: the
 * device's capability set is its own plus the hub's, as of now). */
bool ink_cap_available(const char *id) {
    if (!strncmp(id, "hub/", 4)) return ink_hub_online() && in_directory(id + 4);
    const ink_cap_t *c = ink_cap_find(id);
    if (c) return on_its_page(c);
    return ink_hub_online() && in_directory(id);
}

/* Who may see it: the AI and the hub only what is exposed to them; touch, keys and the system
 * are the owner's own controls. */
static bool exposed(const ink_cap_t *c, ink_source_t src) {
    if (!c->expose) return true;
    switch (src) {
    case INK_SRC_AI: return c->expose & INK_EXPOSE_AI;
    case INK_SRC_REMOTE: return c->expose & INK_EXPOSE_REMOTE;
    case INK_SRC_APP: return c->expose & INK_EXPOSE_APP;
    default: return true;
    }
}

/* Default confirmation by side effect (foundation §6.2). Direct manipulation (touch, keys) is the
 * member's own confirmation except where a PIN is required. */
static ink_confirm_t confirm_for(const ink_cap_t *c, ink_source_t src) {
    ink_confirm_t k = INK_CONFIRM_NONE;
    switch (c->effect) {
    case INK_FX_LOCAL_DESTRUCTIVE:
    case INK_FX_EXTERNAL:
    case INK_FX_SYSTEM: k = INK_CONFIRM_DEVICE; break;
    default: break;
    }
    if (src == INK_SRC_TOUCH || src == INK_SRC_KEY || src == INK_SRC_SYSTEM) return INK_CONFIRM_NONE;
    return k;
}

static ink_rc_t reject(ink_receipt_t *rc, ink_rc_t st, const char *why) {
    rc->status = st;
    snprintf(rc->detail, sizeof rc->detail, "%s", why);
    return st;
}

/* The policy gate (detailed design §2.4), before anything runs. INK_RC_DONE = may run. */
static ink_rc_t gate(const ink_cap_t *c, ink_source_t src, uint32_t page_gen, ink_receipt_t *rc) {
    if (!c) return reject(rc, INK_RC_INVALID, "unknown action");
    if (!on_its_page(c)) return reject(rc, INK_RC_INVALID, "not available on this page");
    if (c->form == INK_FORM_EVENT || !c->run) return reject(rc, INK_RC_INVALID, "an event: subscribe to it");
    if (!exposed(c, src)) return reject(rc, INK_RC_INVALID, "not offered to this caller");
    const ink_member_t *m = ink_member_current();
    if (m->role == INK_ROLE_CHILD) {
        if (c->effect == INK_FX_SYSTEM) {
            ink_probe_count("policy.rejected");
            return reject(rc, INK_RC_REJECTED, "children cannot run system actions");
        }
        if (!ink_member_allows(c->id, c->child_ok)) {
            ink_probe_count("policy.rejected");
            return reject(rc, INK_RC_REJECTED, "not on this child's whitelist");
        }
    }
    bool remote = src == INK_SRC_AI || src == INK_SRC_REMOTE;
    if (remote && strstr(c->id, "upgrade")) { /* the AI never starts an upgrade */
        ink_probe_count("policy.rejected");
        return reject(rc, INK_RC_REJECTED, "upgrades are started by people only");
    }
    /* a tool call prepared for an older page must not act on the new one */
    if (remote && page_gen && c->activity && page_gen != ink_ui_content_version())
        return reject(rc, INK_RC_STALE, "the page changed since the request was made");
    if (confirm_for(c, src) != INK_CONFIRM_NONE) {
        /* the system confirmation card is not built yet: report what would happen */
        ink_probe_count("policy.pending_confirmation");
        return reject(rc, INK_RC_PENDING_CONFIRMATION, "needs confirmation on the device");
    }
    return INK_RC_DONE;
}

/* ------------------------------------------------------------ calls (the soft bus, detailed design §3) */

#define POOL_N 16 /* calls in flight */
#define SEEN_N 32 /* request ids remembered for retries */
#define RID_CAP 40

typedef struct {
    ink_call_t call; /* what the handler sees: first, so an ink_call_t * is its pending_t * */
    ink_call_opts_t o;
    ink_receipt_t out; /* the answer the caller gets (a copy: a late handler may still write call.rc) */
    char id[48];
    char rid[RID_CAP];
    char *json; /* JSON arguments until the gate passed */
    uint64_t deadline_us;
    bool used, deferred, finished, replied, answered, remote;
    bool pinned; /* a waiting ink_cap_invoke still looks at it: not released yet */
} pending_t;

typedef struct {
    char rid[RID_CAP];
    ink_receipt_t rc;
} seen_t;

#define DIR_MAX 32 /* server capabilities the hub offers (architecture §5.3) */

typedef struct {
    char id[40];
    uint8_t form;
} remote_cap_t;

static struct {
    osal_mutex_t *lock;
    pending_t *pool;
    seen_t *seen;
    remote_cap_t *dir; /* DIR_MAX, PSRAM: the hub's directory as last fetched */
    int ndir;
    int seen_next;
    osal_timer_t *timer;
    uint64_t timer_due;
    uint32_t boot_tag, next_rid;
} CB;

static void timeout_cb(void *arg);

static bool in_directory(const char *id) {
    if (!CB.lock) return false;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    bool hit = false;
    for (int i = 0; i < CB.ndir && !hit; i++) hit = !strcmp(CB.dir[i].id, id);
    osal_mutex_unlock(CB.lock);
    return hit;
}

int ink_cap_init(void) {
    if (CB.pool) return 0;
    const char *prev = osal_mem_tag("cap");
    CB.lock = osal_mutex_create();
    CB.pool = osal_calloc(POOL_N, sizeof(pending_t), OSAL_MEM_EXTERNAL);
    CB.seen = osal_calloc(SEEN_N, sizeof(seen_t), OSAL_MEM_EXTERNAL);
    CB.dir = osal_calloc(DIR_MAX, sizeof(remote_cap_t), OSAL_MEM_EXTERNAL);
    CB.timer = osal_timer_create("cap", timeout_cb, NULL);
    osal_mem_tag(prev);
    CB.boot_tag = (uint32_t)(osal_wall_us() / 1000000);
    return CB.lock && CB.pool && CB.seen && CB.dir && CB.timer ? 0 : -1;
}

static pending_t *alloc(void) {
    pending_t *p = NULL;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < POOL_N && !p; i++)
        if (!CB.pool[i].used) p = &CB.pool[i];
    if (p) {
        memset(p, 0, sizeof *p);
        p->used = true;
    }
    osal_mutex_unlock(CB.lock);
    if (!p) ink_probe_count("cap.busy");
    return p;
}

static void release(pending_t *p) {
    osal_free(p->json);
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    p->json = NULL;
    p->used = false;
    osal_mutex_unlock(CB.lock);
}

static void remember(const char *rid, const ink_receipt_t *rc) {
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    seen_t *s = &CB.seen[CB.seen_next];
    CB.seen_next = (CB.seen_next + 1) % SEEN_N;
    snprintf(s->rid, sizeof s->rid, "%s", rid);
    s->rc = *rc;
    osal_mutex_unlock(CB.lock);
}

static void answered(pending_t *p) {
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    p->answered = true;
    bool done = p->finished && !p->pinned;
    osal_mutex_unlock(CB.lock);
    if (done) release(p);
}

static void answer_job(void *arg) {
    pending_t *p = arg;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    ink_cap_done_fn done = p->o.done; /* NULL once a waiting ink_cap_invoke gave up on it */
    osal_mutex_unlock(CB.lock);
    if (done) done(p->o.ctx, &p->out);
    answered(p);
}

/* p->out is final: tell the caller, on its executor. */
static void answer(pending_t *p) {
    if (p->rid[0]) remember(p->rid, &p->out);
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    bool nobody = !p->o.done;
    osal_mutex_unlock(CB.lock);
    if (nobody) answered(p);
    else if (p->o.reply_exec == INK_EXEC_DIRECT || ink_exec_is_current(p->o.reply_exec)) answer_job(p);
    else if (ink_exec_post(p->o.reply_exec, answer_job, p) != 0) {
        ink_probe_count("cap.reply_dropped");
        answered(p);
    }
}

/* The provider is done with the call (any task). */
static void finish(pending_t *p) {
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    p->finished = true;
    bool reply = !p->replied, late = p->replied && p->answered && !p->pinned;
    if (reply) {
        p->replied = true;
        p->call.rc.page_gen = ink_ui_content_version();
        p->out = p->call.rc;
    }
    osal_mutex_unlock(CB.lock);
    if (reply) answer(p);
    else if (late) { /* answered "timed out" long ago */
        ink_probe_count("cap.late");
        release(p);
    }
}

void ink_cap_defer(ink_call_t *call) {
    pending_t *p = (pending_t *)call;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    p->deferred = true;
    if (!CB.timer_due || p->deadline_us < CB.timer_due) { /* only ever brought forward */
        uint64_t now = osal_now_us();
        CB.timer_due = p->deadline_us;
        osal_timer_start(CB.timer, p->deadline_us > now ? p->deadline_us - now : 1, 0);
    }
    osal_mutex_unlock(CB.lock);
}

void ink_cap_done(ink_call_t *call) { finish((pending_t *)call); }

static void timeout_cb(void *arg) { /* timer task: answer the overdue, re-arm for the rest */
    (void)arg;
    pending_t *due[POOL_N];
    int n = 0;
    uint64_t now = osal_now_us(), next = 0;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < POOL_N; i++) {
        pending_t *p = &CB.pool[i];
        if (!p->used || !p->deferred || p->replied) continue;
        if (p->deadline_us <= now) {
            p->replied = true;
            memset(&p->out, 0, sizeof p->out);
            p->out.status = INK_RC_FAILED;
            snprintf(p->out.detail, sizeof p->out.detail, "timed out");
            due[n++] = p;
        } else if (!next || p->deadline_us < next) next = p->deadline_us;
    }
    CB.timer_due = next;
    if (next) osal_timer_start(CB.timer, next - now, 0);
    osal_mutex_unlock(CB.lock);
    for (int i = 0; i < n; i++) {
        ink_probe_count("cap.timeouts");
        answer(due[i]);
    }
}

static uint32_t timeout_ms(const ink_cap_t *c) {
    static const uint32_t BY_QOS[] = {3000, 1000, 10000, 30000, 120000}; /* interactive, realtime, media, network, background */
    if (c && c->timeout_ms) return c->timeout_ms;
    return c && (unsigned)c->qos < sizeof BY_QOS / sizeof BY_QOS[0] ? BY_QOS[c->qos] : 3000;
}

/* On the provider's executor: gate, arguments, run. */
static void start(void *arg) {
    pending_t *p = arg;
    const ink_cap_t *c = p->call.cap;
    ink_receipt_t *rc = &p->call.rc;
    memset(rc, 0, sizeof *rc);
    char key[64];
    snprintf(key, sizeof key, "action.%s.%s", p->call.src == INK_SRC_AI ? "ai" : "user", p->id);
    ink_probe_count(key);
    if (gate(c, p->call.src, p->call.page_gen, rc) == INK_RC_DONE) {
        char why[96];
        int bad = p->json ? ink_args_from_json(&p->call.args, p->json, strlen(p->json), c->input, why, sizeof why)
                          : ink_args_check(&p->call.args, c->input, why, sizeof why);
        if (bad) reject(rc, INK_RC_INVALID, why);
        else {
            rc->status = INK_RC_DONE;
            c->run(&p->call);
            osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
            bool deferred = p->deferred;
            osal_mutex_unlock(CB.lock);
            if (deferred) return; /* ink_cap_done (or the deadline) answers */
        }
    }
    finish(p);
}

static int remote_call(const char *name, const ink_args_t *args, const ink_call_opts_t *o);

/* A retry of a request already answered: the same receipt again. One still running: "queued",
 * it must not run a second time (a text message would go out twice). */
static bool replay(const char *rid, const ink_call_opts_t *o) {
    ink_receipt_t rc;
    bool hit = false;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < SEEN_N && !hit; i++)
        if (CB.seen[i].rid[0] && !strcmp(CB.seen[i].rid, rid)) {
            rc = CB.seen[i].rc;
            hit = true;
        }
    for (int i = 0; i < POOL_N && !hit; i++)
        if (CB.pool[i].used && !CB.pool[i].replied && !strcmp(CB.pool[i].rid, rid)) {
            memset(&rc, 0, sizeof rc);
            rc.status = INK_RC_QUEUED;
            snprintf(rc.detail, sizeof rc.detail, "this request is still running");
            hit = true;
        }
    osal_mutex_unlock(CB.lock);
    if (!hit) return false;
    ink_probe_count("cap.replayed");
    pending_t *p = alloc();
    if (!p) return true; /* answered once already; this copy is lost */
    p->o = *o;
    p->out = rc;
    p->finished = p->replied = true;
    answer(p);
    return true;
}

static pending_t *prepare(const char *id, const ink_cap_t *c, const ink_args_t *args, const ink_call_opts_t *o) {
    pending_t *p = alloc();
    if (!p) return NULL;
    p->call.cap = c;
    snprintf(p->id, sizeof p->id, "%s", id);
    if (args) p->call.args = *args;
    else ink_args_init(&p->call.args);
    if (!args && o->args_json && *o->args_json) {
        size_t n = strlen(o->args_json);
        p->json = osal_malloc(n + 1, OSAL_MEM_EXTERNAL);
        if (!p->json) {
            release(p);
            return NULL;
        }
        memcpy(p->json, o->args_json, n + 1);
    }
    p->call.src = o->src;
    p->call.page_gen = o->page_gen;
    p->o = *o;
    p->o.request_id = p->o.args_json = NULL; /* the caller's strings need not outlive the call */
    if (o->request_id) snprintf(p->rid, sizeof p->rid, "%s", o->request_id);
    p->deadline_us = osal_now_us() + (uint64_t)timeout_ms(c) * 1000u;
    return p;
}

int ink_cap_call(const char *addr, const ink_args_t *args, const ink_call_opts_t *o) {
    if (!CB.pool || !addr || !o) return -1;
    if (!strncmp(addr, "hub/", 4)) return remote_call(addr + 4, args, o);
    if (o->request_id && replay(o->request_id, o)) return 0;
    const ink_cap_t *c = ink_cap_find(addr);
    if (!c && in_directory(addr)) return remote_call(addr, args, o); /* not here, but the hub has it */
    pending_t *p = prepare(addr, c, args, o);
    if (!p) return -1;
    ink_exec_t e = c ? c->exec : INK_EXEC_DIRECT;
    if (e == INK_EXEC_DIRECT || ink_exec_is_current(e)) start(p);
    else if (ink_exec_post(e, start, p) != 0) {
        release(p);
        ink_probe_count("cap.post_failed");
        return -1;
    }
    return 0;
}

typedef struct {
    ink_receipt_t *rc;
    int copied;
} sync_reply_t;

static void copy_receipt(void *ctx, const ink_receipt_t *rc) {
    sync_reply_t *s = ctx;
    *s->rc = *rc;
    __atomic_store_n(&s->copied, 1, __ATOMIC_RELEASE);
}

/* Run here and wait (the UI task, or a test task): the handler runs inline whatever its executor,
 * except media and background work, which is queued. */
static ink_rc_t invoke(const char *id, const ink_args_t *args, const char *json, ink_source_t src, uint32_t page_gen,
                       ink_receipt_t *rc) {
    memset(rc, 0, sizeof *rc);
    const ink_cap_t *c = ink_cap_find(id);
    ink_call_opts_t o = {.src = src, .page_gen = page_gen, .args_json = json, .reply_exec = INK_EXEC_DIRECT};
    if (c && (c->exec == INK_EXEC_MEDIA || c->exec == INK_EXEC_BG)) {
        if (ink_cap_call(id, args, &o) != 0) return reject(rc, INK_RC_FAILED, "busy");
        rc->status = INK_RC_QUEUED;
        return rc->status;
    }
    sync_reply_t sync = {.rc = rc};
    o.done = copy_receipt;
    o.ctx = &sync;
    pending_t *p = CB.pool ? prepare(id, c, args, &o) : NULL;
    if (!p) return reject(rc, INK_RC_FAILED, "busy");
    p->pinned = true; /* looked at below, however soon it completes */
    start(p);
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    bool later = !p->replied;
    if (later) p->o.done = NULL; /* it completes later: nobody here to tell, rc is about to go out of scope */
    osal_mutex_unlock(CB.lock);
    /* answered in another task at this very moment: the copy into rc is on its way */
    while (!later && !__atomic_load_n(&sync.copied, __ATOMIC_ACQUIRE)) osal_task_yield();
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    p->pinned = false;
    bool gone = p->finished && p->answered;
    osal_mutex_unlock(CB.lock);
    if (gone) release(p);
    if (later) {
        rc->status = INK_RC_QUEUED;
        snprintf(rc->detail, sizeof rc->detail, "the result comes later");
    }
    return rc->status;
}

ink_rc_t ink_cap_invoke(const char *id, const ink_args_t *args, ink_source_t src, uint32_t page_gen, ink_receipt_t *rc) {
    return invoke(id, args, NULL, src, page_gen, rc);
}

ink_rc_t ink_cap_invoke_json(const char *id, const char *args_json, ink_source_t src, uint32_t page_gen, ink_receipt_t *rc) {
    return invoke(id, NULL, args_json && *args_json ? args_json : NULL, src, page_gen, rc);
}

/* ------------------------------------------------------------ remote proxy (device -> hub) */

static ink_rc_t rc_by_name(const char *s) {
    for (unsigned i = 0; i < sizeof RC_NAME / sizeof RC_NAME[0]; i++)
        if (!strcmp(RC_NAME[i], s)) return (ink_rc_t)i;
    return INK_RC_FAILED;
}

/* The hub's answer: an MCP CallToolResult whose structuredContent is a receipt. Hub task. */
static void remote_reply(void *ctx, int status, const ink_json_t *j, int result) {
    pending_t *p = ctx;
    ink_receipt_t *rc = &p->call.rc;
    memset(rc, 0, sizeof *rc);
    if (status != INK_HUB_OK) {
        rc->status = INK_RC_FAILED;
        const char *why = status == INK_HUB_ERR_OFFLINE ? "offline" : status == INK_HUB_ERR_TIMEOUT ? "timed out" : status == INK_HUB_ERR_LOST ? "connection lost" : "hub error";
        if (status == INK_HUB_ERR_REMOTE && ink_json_str(j, ink_json_find(j, result, "message"), rc->detail, sizeof rc->detail) > 0) why = NULL;
        if (why) snprintf(rc->detail, sizeof rc->detail, "%s", why);
    } else {
        char st[32];
        int sc = ink_json_find(j, result, "structuredContent");
        rc->status = ink_json_str(j, ink_json_find(j, sc, "status"), st, sizeof st) > 0 ? rc_by_name(st) : INK_RC_DONE;
        ink_json_str(j, ink_json_find(j, sc, "object"), rc->object, sizeof rc->object);
        ink_json_str(j, ink_json_find(j, sc, "say"), rc->say, sizeof rc->say);
        ink_json_str(j, ink_json_find(j, sc, "detail"), rc->detail, sizeof rc->detail);
        int64_t v;
        if (ink_json_int(j, ink_json_find(j, sc, "revision"), &v)) rc->revision = (uint32_t)v;
        if (ink_json_int(j, ink_json_find(j, sc, "undo_ms"), &v)) rc->undo_ms = (uint32_t)v;
        if (ink_json_eq(j, ink_json_find(j, result, "isError"), "true")) rc->status = INK_RC_FAILED;
    }
    char key[48];
    snprintf(key, sizeof key, "cap.remote.%s", ink_rc_name(rc->status));
    ink_probe_count(key);
    finish(p);
}

static int remote_call(const char *name, const ink_args_t *args, const ink_call_opts_t *o) {
    if (o->request_id && replay(o->request_id, o)) return 0;
    pending_t *p = prepare(name, NULL, args, o);
    if (!p) return -1;
    p->remote = true;
    if (!p->rid[0]) snprintf(p->rid, sizeof p->rid, "d%08x-%u", (unsigned)CB.boot_tag, (unsigned)__atomic_add_fetch(&CB.next_rid, 1, __ATOMIC_RELAXED));
    const char *prev = osal_mem_tag("cap");
    char *params = osal_malloc(INK_ARGS_TEXT * 2 + 256, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!params) {
        release(p);
        return -1;
    }
    ink_jw_t w;
    ink_jw_init(&w, params, INK_ARGS_TEXT * 2 + 256);
    ink_jw_raw(&w, "{\"name\":");
    ink_jw_str(&w, name);
    ink_jw_raw(&w, ",\"arguments\":");
    if (p->json) ink_jw_raw(&w, p->json);
    else {
        int n = ink_args_to_json(&p->call.args, w.buf + w.len, w.cap - w.len);
        if (n < 0) w.overflow = true;
        else w.len += (size_t)n;
    }
    ink_jw_raw(&w, ",\"_meta\":{\"request_id\":");
    ink_jw_str(&w, p->rid);
    ink_jw_raw(&w, ",\"page_gen\":");
    ink_jw_int(&w, p->call.page_gen);
    ink_jw_raw(&w, "}}");
    int r = w.overflow ? -1 : ink_hub_call("tools/call", params, 30000000, remote_reply, p);
    osal_free(params);
    if (r < 0) { /* not even queued: answer now */
        memset(&p->call.rc, 0, sizeof p->call.rc);
        p->call.rc.status = INK_RC_FAILED;
        snprintf(p->call.rc.detail, sizeof p->call.rc.detail, "%s", w.overflow ? "arguments too long" : "offline");
        finish(p);
    }
    return 0;
}

/* ------------------------------------------------------------ events */

#define SUBS_MAX 16

static struct {
    const char *prefix;
    size_t plen;
    ink_event_fn fn;
    void *ctx;
    ink_exec_t exec;
} SUBS[SUBS_MAX];
static int nsubs;

typedef struct {
    ink_event_fn fn;
    void *ctx;
    char id[48];
    ink_args_t payload;
} event_job_t;

int ink_cap_subscribe(const char *prefix, ink_event_fn fn, void *ctx, ink_exec_t exec) {
    if (!CB.lock || !fn) return -1;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    int r = -1;
    if (nsubs < SUBS_MAX) {
        SUBS[nsubs] = (typeof(SUBS[0])){prefix ? prefix : "", prefix ? strlen(prefix) : 0, fn, ctx, exec};
        r = nsubs++;
    }
    osal_mutex_unlock(CB.lock);
    return r < 0 ? -1 : 0;
}

static void event_job(void *arg) {
    event_job_t *e = arg;
    e->fn(e->ctx, e->id, &e->payload);
    osal_free(e);
}

void ink_cap_emit(const char *id, const ink_args_t *payload) {
    char key[64];
    snprintf(key, sizeof key, "event.%s", id);
    ink_probe_count(key);
    for (int i = 0; i < nsubs; i++) {
        if (strncmp(id, SUBS[i].prefix, SUBS[i].plen)) continue;
        const char *prev = osal_mem_tag("cap");
        event_job_t *e = osal_malloc(sizeof *e, OSAL_MEM_EXTERNAL);
        osal_mem_tag(prev);
        if (!e) {
            ink_probe_count("event.dropped");
            continue;
        }
        e->fn = SUBS[i].fn;
        e->ctx = SUBS[i].ctx;
        snprintf(e->id, sizeof e->id, "%s", id);
        if (payload) e->payload = *payload;
        else ink_args_init(&e->payload);
        if (ink_exec_post(SUBS[i].exec, event_job, e) != 0) {
            ink_probe_count("event.dropped");
            osal_free(e);
        }
    }
}

/* What the current member may use now, as the AI sees it. */
static bool listed(const ink_cap_t *c) {
    if (!on_its_page(c) || !exposed(c, INK_SRC_AI)) return false;
    const ink_member_t *m = ink_member_current();
    return m->role != INK_ROLE_CHILD || (c->effect != INK_FX_SYSTEM && ink_member_allows(c->id, c->child_ok));
}

int ink_cap_manifest_json(char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "[");
    bool first = true;
    for (int i = 0; i < nreg; i++) {
        const ink_cap_t *c = REG[i];
        if (!listed(c)) continue;
        ink_jw_raw(&w, first ? "{\"name\":" : ",{\"name\":");
        first = false;
        ink_jw_str(&w, c->id);
        ink_jw_raw(&w, ",\"title\":");
        ink_jw_str(&w, c->title);
        ink_jw_raw(&w, ",\"effect\":");
        ink_jw_str(&w, ink_effect_name(c->effect));
        ink_jw_raw(&w, ",\"confirm\":");
        ink_jw_str(&w, CONFIRM_NAME[confirm_for(c, INK_SRC_AI)]);
        if (c->version > 1) {
            ink_jw_raw(&w, ",\"version\":");
            ink_jw_int(&w, c->version);
        }
        if (c->form != INK_FORM_CALL) {
            ink_jw_raw(&w, ",\"form\":");
            ink_jw_str(&w, FORM_NAME[c->form]);
        }
        ink_jw_raw(&w, ",\"inputSchema\":");
        ink_jw_raw(&w, c->input ? c->input : "{\"type\":\"object\",\"properties\":{}}");
        if (c->output) {
            ink_jw_raw(&w, ",\"outputSchema\":");
            ink_jw_raw(&w, c->output);
        }
        ink_jw_raw(&w, "}");
    }
    ink_jw_raw(&w, "]");
    return w.overflow ? -1 : (int)w.len;
}

int ink_cap_ids_json(char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "[");
    bool first = true;
    for (int i = 0; i < nreg; i++) {
        if (!on_its_page(REG[i]) || !exposed(REG[i], INK_SRC_AI)) continue;
        if (!first) ink_jw_raw(&w, ",");
        first = false;
        ink_jw_str(&w, REG[i]->id);
    }
    ink_jw_raw(&w, "]");
    return w.overflow ? -1 : (int)w.len;
}

int ink_receipt_json(const ink_receipt_t *rc, char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "{\"status\":");
    ink_jw_str(&w, ink_rc_name(rc->status));
    if (rc->object[0]) {
        ink_jw_raw(&w, ",\"object\":");
        ink_jw_str(&w, rc->object);
        ink_jw_raw(&w, ",\"revision\":");
        ink_jw_int(&w, rc->revision);
    }
    if (rc->undo_ms) {
        ink_jw_raw(&w, ",\"undo_ms\":");
        ink_jw_int(&w, rc->undo_ms);
    }
    if (rc->say[0]) {
        ink_jw_raw(&w, ",\"say\":");
        ink_jw_str(&w, rc->say);
    }
    if (rc->detail[0]) {
        ink_jw_raw(&w, ",\"detail\":");
        ink_jw_str(&w, rc->detail);
    }
    if (rc->page_gen) {
        ink_jw_raw(&w, ",\"page_gen\":");
        ink_jw_int(&w, rc->page_gen);
    }
    ink_jw_raw(&w, "}");
    return w.overflow ? -1 : (int)w.len;
}

/* ------------------------------------------------------------ hub tools */

typedef struct {
    int64_t id;
    bool list;
    uint32_t page_gen;
    uint64_t received_us;
    char name[48];
    char rid[RID_CAP]; /* _meta.request_id: a retry gets the first receipt again */
    char args[];       /* JSON object text */
} tool_job_t;

static void tool_done(void *ctx, const ink_receipt_t *rc) { /* where the capability finished */
    tool_job_t *t = ctx;
    char key[48];
    snprintf(key, sizeof key, "tools.calls.%s", ink_rc_name(rc->status));
    ink_probe_count(key);
    const char *prev = osal_mem_tag("tools");
    char *out = osal_malloc(768, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!out || ink_receipt_json(rc, out, 768) < 0) ink_hub_respond_error(t->id, -32603, out ? "receipt too long" : "out of memory");
    else ink_hub_respond(t->id, out);
    osal_free(out);
    osal_free(t);
}

static void tool_run(void *arg) { /* UI task */
    tool_job_t *t = arg;
    if (t->list) {
        const char *prev = osal_mem_tag("tools");
        char *out = osal_malloc(6144, OSAL_MEM_EXTERNAL);
        osal_mem_tag(prev);
        int n = out ? ink_cap_manifest_json(out + 10, 6144 - 12) : -1;
        if (n < 0) ink_hub_respond_error(t->id, -32603, out ? "tool list too long" : "out of memory");
        else {
            memcpy(out, "{\"tools\":", 9);
            out[9] = ' ';
            out[10 + n] = '}';
            out[11 + n] = 0;
            ink_hub_respond(t->id, out);
        }
        osal_free(out);
        osal_free(t);
        return;
    }
    ink_ui_cause(t->received_us, "ai.tool_to_visible_ms"); /* what the capability changes on screen */
    ink_call_opts_t o = {.src = INK_SRC_AI, .page_gen = t->page_gen, .request_id = t->rid[0] ? t->rid : NULL, .args_json = t->args,
                         .reply_exec = INK_EXEC_DIRECT, .done = tool_done, .ctx = t};
    if (ink_cap_call(t->name, NULL, &o) != 0) {
        ink_hub_respond_error(t->id, -32603, "busy");
        osal_free(t);
    }
}

static void on_tools(void *ctx, int64_t id, const char *method, const ink_json_t *j, int params) { /* hub task */
    (void)ctx;
    bool list = !strcmp(method, "tools/list");
    if (!list && strcmp(method, "tools/call")) {
        ink_hub_respond_error(id, -32601, "unknown tools method");
        return;
    }
    int args_t = list ? -1 : ink_json_find(j, params, "arguments");
    size_t alen = args_t >= 0 ? (size_t)(j->tok[args_t].end - j->tok[args_t].start) : 0;
    const char *prev = osal_mem_tag("tools");
    tool_job_t *t = osal_calloc(1, sizeof *t + alen + 1, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!t) {
        ink_hub_respond_error(id, -32603, "out of memory");
        return;
    }
    t->id = id;
    t->list = list;
    t->received_us = osal_now_us();
    if (!list) {
        if (ink_json_str(j, ink_json_find(j, params, "name"), t->name, sizeof t->name) < 0) {
            osal_free(t);
            ink_hub_respond_error(id, -32602, "tools/call needs a name");
            return;
        }
        int64_t pg = 0;
        if (ink_json_int(j, ink_json_find(j, params, "page_gen"), &pg) || ink_json_int(j, ink_json_find(j, params, "_meta.page_gen"), &pg))
            t->page_gen = (uint32_t)pg;
        ink_json_str(j, ink_json_find(j, params, "_meta.request_id"), t->rid, sizeof t->rid);
        if (alen) memcpy(t->args, j->text + j->tok[args_t].start, alen);
        t->args[alen] = 0;
    }
    ink_ui_post(tool_run, t);
}

/* ------------------------------------------------------------ the hub's directory (step 8) */

static void dir_reply(void *ctx, int status, const ink_json_t *j, int result) { /* hub task */
    (void)ctx;
    if (status != INK_HUB_OK) {
        ink_probe_count("cap.directory_failed");
        return;
    }
    int list = ink_json_find(j, result, "tools");
    if (list < 0 || j->tok[list].type != 2) return;
    remote_cap_t got[DIR_MAX];
    int n = 0, t = list + 1;
    for (int k = 0; k < j->tok[list].size && t < j->ntok && n < DIR_MAX; k++) {
        int end = j->tok[t].end, next = t + 1;
        while (next < j->ntok && j->tok[next].start < end) next++;
        char form[12] = "";
        if (ink_json_str(j, ink_json_find(j, t, "name"), got[n].id, sizeof got[n].id) > 0) {
            ink_json_str(j, ink_json_find(j, t, "form"), form, sizeof form);
            got[n].form = !strcmp(form, "session") ? INK_FORM_SESSION : !strcmp(form, "stream") ? INK_FORM_STREAM : INK_FORM_CALL;
            n++;
        }
        t = next;
    }
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    memcpy(CB.dir, got, sizeof got[0] * (size_t)n);
    CB.ndir = n;
    osal_mutex_unlock(CB.lock);
    ink_probe_value("cap.directory_size", n);
}

static void dir_fetch(void *ctx) { /* hub task: on every (re)connection */
    (void)ctx;
    ink_hub_call("caps/list", "{}", 10000000, dir_reply, NULL);
}

static void on_caps_changed(void *ctx, const char *method, const ink_json_t *j, int params) { /* hub task */
    (void)ctx;
    (void)j;
    (void)params;
    if (!strcmp(method, "caps/changed")) dir_fetch(NULL);
}

int ink_tools_init(void) {
    if (ink_hub_on_online(dir_fetch, NULL) || ink_hub_subscribe("caps/", on_caps_changed, NULL)) return -1;
    return ink_hub_on_request("tools/", on_tools, NULL);
}
