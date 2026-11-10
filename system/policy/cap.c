/*
 * Capability registry, policy gate and receipts (see ink/sys/cap.h), plus the
 * hub's "tools/list" / "tools/call" (the AI's way in, D-12: MCP tool and
 * result structures inside the device's own JSON-RPC envelope).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/log.h"
#include "ink/sys/camera.h"
#include "ink/sys/power.h"
#include "ink/sys/confirm.h"
#include "ink/sys/data.h"
#include "ink/sys/svc.h"
#include "ink/sys/hub.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/i18n.h"
#include "ink/sys/ui.h"
#include "ink/ui_shell.h"
#include "ink/sys/sha256.h"
#include "ink/sys/storage.h"

#define MAX_CAPS 512 /* pointers in PSRAM; 256 filled up on 2026-11-09 as the system apps arrived */

static const ink_cap_t *REG[MAX_CAPS] INK_PSRAM_BSS;
static int nreg;

static const char *RC_NAME[] = {"done",     "partial", "queued", "pending_confirmation", "rejected",
                                "stale",    "invalid", "failed", "cancelled",            "undone"};
static const char *FX_NAME[] = {"read", "navigate", "local_write", "local_destructive", "external", "system"};
static const char *CONFIRM_NAME[] = {"none", "device", "parent"};
static const char *FORM_NAME[] = {"call", "stream", "event", "session"};

const char *ink_rc_name(ink_rc_t rc) { return (unsigned)rc < sizeof RC_NAME / sizeof RC_NAME[0] ? RC_NAME[rc] : "?"; }
const char *ink_effect_name(ink_effect_t fx) { return (unsigned)fx < sizeof FX_NAME / sizeof FX_NAME[0] ? FX_NAME[fx] : "?"; }

int ink_cap_register(const ink_cap_t *table, int n) { /* all of the table or none of it */
    if (n < 0 || nreg + n > MAX_CAPS) return -1;
    for (int i = 0; i < n; i++) REG[nreg++] = &table[i];
    return 0;
}

const ink_cap_t *ink_cap_at(int i) { return i >= 0 && i < nreg ? REG[i] : NULL; }

const ink_cap_t *ink_cap_find(const char *id) {
    for (int i = 0; i < nreg; i++)
        if (!strcmp(REG[i]->id, id)) return REG[i];
    return NULL;
}

static bool on_its_page(const ink_cap_t *c) { return !c->activity || ((ink_ui_page_current() || ink_ui_locked_allows(c->id)) && !strcmp(ink_ui_action_page(), c->activity)); }
bool ink_cap_locked_page(void) { return ink_ui_locked_activity() != NULL; }

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

/* D-105: the gateway's intent marker applies only to ordinary external actions. */
static ink_confirm_t confirm_for(const ink_cap_t *c, ink_source_t src, bool asked) {
    if (src == INK_SRC_CONFIRMED) return INK_CONFIRM_NONE;
    if (c->parent_pin && ink_member_any_pin()) return INK_CONFIRM_PARENT;
    if (c->ask_by_hand) return INK_CONFIRM_DEVICE;
    if (src == INK_SRC_TOUCH || src == INK_SRC_KEY || src == INK_SRC_SYSTEM) return INK_CONFIRM_NONE;
    if (c->effect == INK_FX_LOCAL_DESTRUCTIVE) return INK_CONFIRM_DEVICE;
    if (c->effect == INK_FX_EXTERNAL && !asked) return INK_CONFIRM_DEVICE;
    return INK_CONFIRM_NONE;
}

static ink_rc_t reject(ink_receipt_t *rc, ink_rc_t st, const char *why) {
    rc->status = st;
    snprintf(rc->detail, sizeof rc->detail, "%s", why);
    return st;
}
static bool opens_emergency(const ink_cap_t *c) { return c && c->locked_entry; }
/* The policy gate (detailed design §2.4), before anything runs. INK_RC_DONE = may run. */
static ink_rc_t gate(const ink_cap_t *c, ink_source_t src, uint32_t page_gen, bool asked, ink_receipt_t *rc) {
    if (!c) return reject(rc, INK_RC_INVALID, "unknown action");
    const ink_member_t *m = ink_member_current();
    if (m->role == INK_ROLE_CHILD && src != INK_SRC_SYSTEM && !ink_member_allows(c->id, true)) return reject(rc, INK_RC_REJECTED, TR("家长没有打开这个应用"));
    /* the card the member just answered is still over the page it was asked about */
    if (src == INK_SRC_CONFIRMED ? c->activity && (!ink_ui_page_current() || strcmp(ink_ui_page_name(), c->activity)) : !on_its_page(c)) return reject(rc, INK_RC_INVALID, "not available on this page");
    if (c->form == INK_FORM_EVENT || !c->run) return reject(rc, INK_RC_INVALID, "an event: subscribe to it");
    if (!exposed(c, src)) return reject(rc, INK_RC_INVALID, "not offered to this caller");
    if (m->role == INK_ROLE_CHILD) {
        if (!ink_member_time_allows(c->id)) return reject(rc, INK_RC_REJECTED, "today\'s time is up");
        if (c->effect == INK_FX_SYSTEM && !(c->child_ok && (src == INK_SRC_TOUCH || src == INK_SRC_KEY))) { /* turning it off is anyone's who holds it */
            ink_probe_count("policy.rejected");
            return reject(rc, INK_RC_REJECTED, "children cannot run system actions");
        }
        if (!c->child_ok && !ink_member_allows(c->id, false)) {
            ink_probe_count("policy.rejected");
            return reject(rc, INK_RC_REJECTED, "not on this child's whitelist");
        }
    }
    bool restricted = ink_ui_locked_activity() != NULL;
    bool locked_caller = ink_power_state() == INK_PWR_LOCKED && c->locked_owner_only &&
        (src == INK_SRC_AI || src == INK_SRC_REMOTE || src == INK_SRC_APP);
    if ((!ink_member_space_open() || locked_caller || restricted) && src != INK_SRC_SYSTEM) {
        bool allowed = restricted ? ink_ui_locked_allows(c->id) : c->while_locked;
        if (!opens_emergency(c) && (src == INK_SRC_AI || src == INK_SRC_REMOTE || src == INK_SRC_APP || !allowed)) {
            ink_probe_count("policy.rejected");
            return reject(rc, INK_RC_REJECTED, "this member's space is locked");
        }
    }
    if (src == INK_SRC_APP && c->permission && !ink_app_foreground_may(c->permission)) {
        ink_probe_count("policy.rejected");
        return reject(rc, INK_RC_REJECTED, "the app did not declare this permission");
    }
    bool remote = src == INK_SRC_AI || src == INK_SRC_REMOTE;
    if (remote && strstr(c->id, "upgrade")) { /* the AI never starts an upgrade */
        ink_probe_count("policy.rejected");
        return reject(rc, INK_RC_REJECTED, "upgrades are started by people only");
    }
    /* a tool call prepared for an older page must not act on the new one */
    if ((remote || src == INK_SRC_CONFIRMED) && page_gen && c->activity && page_gen != ink_ui_content_version())
        return reject(rc, INK_RC_STALE, "the page changed since the request was made");
    if (confirm_for(c, src, asked) != INK_CONFIRM_NONE) {
        ink_probe_count("policy.pending_confirmation");
        return reject(rc, INK_RC_PENDING_CONFIRMATION, "needs confirmation on the device");
    }
    return INK_RC_DONE;
}

static bool time_data_allows(const ink_call_t *call) {
    if (!ink_member_time_up() || (strcmp(call->cap->id, "data.list") && strcmp(call->cap->id, "data.get"))) return true;
    char collection[48] = ""; ink_args_get_str(&call->args, "collection", collection, sizeof collection);
    return !strcmp(collection, "phone.contacts") || !strcmp(collection, "phone.calls") || !strcmp(collection, "sms.messages");
}

/* ------------------------------------------------------------ calls (the soft bus, detailed design §3) */

#define POOL_N 16 /* calls in flight */
#define SEEN_N 32 /* request ids remembered for retries */
#define RID_CAP 40

#define ACTIVITY_PENDING 16
#define ACTIVITY_SLOTS (ACTIVITY_PENDING * INK_MEMBERS_MAX)

typedef struct {
    ink_exec_job_t job;
    ink_svc_deadline_t retry;
    bool used;
    char member[INK_MEMBER_ID_MAX], cap[48], object[48];
    ink_rc_t status;
    uint32_t revision, undo_ms;
    bool asked;
    int64_t when_s;
} activity_note_t;

static struct {
    bool ready;
    activity_note_t pending[ACTIVITY_SLOTS];
} AL INK_PSRAM_BSS;

static const ink_db_def_t ACTIVITY = {.name = "activity.log", .title = "AI 的活动记录", .scope = INK_DB_MEMBER,
    .schema = 1, .json = true, .ai_read = true, .max_bytes = 128 * 1024, .drop_oldest = true};

/*
 * One call, from acceptance to the slot's return. Its deadline counts from acceptance, whatever it
 * waits for (detailed design §3.3). Three things must have happened before the slot can be used
 * again, in any order: the provider is through with it (finished: the handler returned without
 * deferring, ink_cap_done came, or it was never run), the caller has been told (answered), and
 * no task still looks at it (refs: the one running its handler, a waiting ink_cap_invoke). The last
 * of them frees it, in unlock_retire().
 */
typedef struct {
    ink_call_t call; /* what the handler sees: first, so an ink_call_t * is its pending_t * */
    ink_call_opts_t o;
    ink_receipt_t out; /* the first final receipt, what the caller gets (a late handler may still write call.rc) */
    char id[48];
    char rid[RID_CAP];
    char *json; /* JSON arguments until the gate passed */
    char *result; /* what the handler found (rc.result points here until the slot goes) */
    uint64_t deadline_us;
    uint64_t log_started;
    ink_exec_job_t start_job, answer_job;
    uint8_t refs;
    bool used;
    bool accepted; /* fully prepared: the deadline applies */
    bool remote;   /* the hub link's own timeout is its deadline */
    bool cache_hit;
    bool repeat_safe; /* rid is the caller's: the receipt is kept for retries */
    bool started, deferred, replied, finished, answered;
    bool asked; /* Only the hub tools envelope can set this; never arguments. */
    bool hand_locked; /* the page allowed this call before its handler could close it */
    activity_note_t *activity;
    char member[INK_MEMBER_ID_MAX]; /* ink_call_opts_t.member */
    uint32_t origin_epoch;
    uint8_t identity[32];
} pending_t;

typedef struct {
    char rid[RID_CAP], member[INK_MEMBER_ID_MAX];
    uint8_t identity[32];
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
    uint32_t dir_epoch;
    int seen_next;
    osal_timer_t *timer;
    uint64_t timer_due;
    uint32_t boot_tag, next_rid;
} CB;

static void activity_flush(void *arg);

static void activity_retry(void *arg) { /* timer task: storage work belongs to the executor */
    activity_note_t *n = arg;
    if (ink_exec_post(INK_EXEC_BG, &n->job, activity_flush, n) != 0)
        ink_svc_at(&n->retry, osal_now_us() + 1000000, activity_retry, n);
}

static void activity_flush(void *arg) { /* exec.bg */
    activity_note_t *n = arg;
    if (!ink_member_exists(n->member)) {
        osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER); n->used = false; osal_mutex_unlock(CB.lock);
        return;
    }
    ink_db_t *db = ink_db_open_member(&ACTIVITY, n->member);
    char json[512];
    ink_jw_t w;
    ink_jw_init(&w, json, sizeof json);
    ink_jw_raw(&w, "{\"cap\":"); ink_jw_str(&w, n->cap);
    ink_jw_raw(&w, ",\"status\":"); ink_jw_str(&w, ink_rc_name(n->status));
    ink_jw_raw(&w, ",\"object\":"); ink_jw_str(&w, n->object);
    ink_jw_raw(&w, ",\"revision\":"); ink_jw_int(&w, n->revision);
    ink_jw_raw(&w, ",\"undo_ms\":"); ink_jw_int(&w, n->undo_ms);
    ink_jw_raw(&w, ",\"when\":"); ink_jw_int(&w, n->when_s);
    ink_jw_raw(&w, n->asked ? ",\"asked\":true}" : ",\"asked\":false}");
    ink_db_write_t rec = {.data = json, .len = w.len, .sort = n->when_s, .src = INK_SRC_AI};
    int result = db && !w.overflow ? ink_db_put(db, &rec, NULL) : INK_DB_ERR_NOMEM;
    if (db) ink_db_close(db);
    if (result != INK_DB_OK) {
        if (result == INK_DB_ERR_FULL) ink_probe_count("activity.backlog_full");
        ink_probe_count("activity.write_retry");
        ink_svc_at(&n->retry, osal_now_us() + 1000000, activity_retry, n);
        return;
    }
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    n->used = false;
    osal_mutex_unlock(CB.lock);
}

static bool needs_activity(const ink_cap_t *c, ink_source_t src) {
    return AL.ready && c && strcmp(c->id, "activity.clear") && c->effect >= INK_FX_LOCAL_WRITE &&
           (src == INK_SRC_AI || src == INK_SRC_REMOTE || src == INK_SRC_CONFIRMED);
}

static activity_note_t *activity_reserve(const ink_cap_t *c, const char *member) {
    activity_note_t *n = NULL;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    int held = 0;
    for (int i = 0; i < ACTIVITY_SLOTS; i++)
        if (AL.pending[i].used && !strcmp(AL.pending[i].member, member)) held++;
    for (int i = 0; held < ACTIVITY_PENDING && i < ACTIVITY_SLOTS && !n; i++)
        if (!AL.pending[i].used) {
            n = &AL.pending[i];
            n->used = true;
            
            snprintf(n->member, sizeof n->member, "%s", member);
            snprintf(n->cap, sizeof n->cap, "%s", c->id);
        }
    osal_mutex_unlock(CB.lock);
    return n;
}

/* Under the call lock: no arguments, passwords or free-form receipt text enter the log. */
static void activity_settle(pending_t *p, const ink_receipt_t *rc) {
    activity_note_t *n = p->activity;
    if (!n) return;
    p->activity = NULL;
    snprintf(n->object, sizeof n->object, "%s", rc->object);
    n->status = rc->status;
    n->revision = rc->revision;
    n->undo_ms = rc->undo_ms;
    n->asked = p->asked;
    n->when_s = osal_wall_us() / 1000000;
    activity_retry(n);
}

static void timeout_cb(void *arg);

static bool in_directory(const char *id) {
    if (!CB.lock) return false;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    bool hit = false;
    for (int i = 0; CB.dir_epoch == ink_hub_member_epoch() && i < CB.ndir && !hit; i++) hit = !strcmp(CB.dir[i].id, id);
    osal_mutex_unlock(CB.lock);
    return hit;
}

static int register_bus_caps(void);

typedef struct { uint32_t value[2]; osal_sem_t *done; int result; } boot_write_t;
static void boot_written(void *ctx, int result) {
    boot_write_t *b = ctx;
    b->result = result;
    osal_sem_give(b->done);
}

static bool next_boot(void) {
    boot_write_t b = {0};
    void *saved = NULL;
    size_t len = 0;
    int r = ink_storage_load("/user/hub-event-boot", &saved, &len);
    if (r == 0) {
        if (len == sizeof b.value) memcpy(b.value, saved, len);
        ink_storage_free(saved);
        if (len != sizeof b.value || b.value[1] != (b.value[0] ^ 0x494e4b42u) || b.value[0] == UINT32_MAX) return false;
    } else if (r != INK_STORAGE_NO_FILE) return false;
    b.value[0]++;
    b.value[1] = b.value[0] ^ 0x494e4b42u;
    b.done = osal_sem_create(0, 1);
    if (!b.done) return false;
    r = ink_storage_write_borrow("/user/hub-event-boot", b.value, sizeof b.value, boot_written, &b, NULL);
    if (!r) osal_sem_take(b.done, OSAL_WAIT_FOREVER);
    osal_sem_delete(b.done);
    if (r || b.result) return false;
    CB.boot_tag = b.value[0];
    return true;
}

int ink_cap_init(void) {
    if (CB.pool) return 0;
    if (register_bus_caps() != 0) return -1;
    const char *prev = osal_mem_tag("cap");
    CB.lock = osal_mutex_create();
    CB.pool = osal_calloc(POOL_N, sizeof(pending_t), OSAL_MEM_EXTERNAL);
    CB.seen = osal_calloc(SEEN_N, sizeof(seen_t), OSAL_MEM_EXTERNAL);
    CB.dir = osal_calloc(DIR_MAX, sizeof(remote_cap_t), OSAL_MEM_EXTERNAL);
    CB.timer = osal_timer_create("cap", timeout_cb, NULL);
    osal_mem_tag(prev);
    return CB.lock && CB.pool && CB.seen && CB.dir && CB.timer && next_boot() ? 0 : -1;
}

static pending_t *claim(const char *rid, const char *member, const uint8_t identity[32],
                        ink_rc_t verdict, bool *repeat) {
    pending_t *p = NULL;
    *repeat = false;
    if (rid && !*rid) rid = NULL;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < POOL_N && !p; i++) if (!CB.pool[i].used) p = &CB.pool[i];
    if (p) {
        memset(p, 0, sizeof *p);
        p->used = true;
        snprintf(p->member, sizeof p->member, "%s", member);
        
        memcpy(p->identity, identity, 32);
        if (verdict != INK_RC_DONE && verdict != INK_RC_PENDING_CONFIRMATION) {
            p->out.status = verdict;
            *repeat = true;
        }
        for (int i = 0; rid && !*repeat && i < SEEN_N; i++) {
            const seen_t *s = &CB.seen[i];
            if (!strcmp(s->rid, rid) && !strcmp(s->member, member)) {
                if (!memcmp(s->identity, identity, 32)) p->out = s->rc;
                else { p->out.status = INK_RC_INVALID; snprintf(p->out.detail, sizeof p->out.detail, "request id reused with different action or arguments"); }
                p->cache_hit = true;
                *repeat = true;
            }
        }
        for (int i = 0; rid && !*repeat && i < POOL_N; i++) {
            pending_t *q = &CB.pool[i];
            if (q != p && q->used && !q->replied && !strcmp(q->member, member) && !strcmp(q->rid, rid)) {
                p->cache_hit = true;
                p->out.status = memcmp(q->identity, identity, 32) ? INK_RC_INVALID : INK_RC_QUEUED;
                *repeat = true;
            }
        }
        if (*repeat) p->started = p->replied = p->finished = true;
        else if (rid) { snprintf(p->rid, sizeof p->rid, "%s", rid); p->repeat_safe = true; }
    }
    osal_mutex_unlock(CB.lock);
    if (!p) ink_probe_count("cap.busy");
    return p;
}

/* Releases the lock. Whoever finds all three conditions met frees the slot: only one can, each sets
 * its own condition under the lock before it comes here. */
static void unlock_retire(pending_t *p) {
    char *json = NULL, *result = NULL;
    if (p->finished && p->answered && !p->refs) {
        json = p->json;
        p->json = NULL;
        result = p->result;
        p->result = NULL;
        p->used = false;
    }
    osal_mutex_unlock(CB.lock);
    osal_free(json);
    osal_free(result);
}

/* Under the lock: `rc` is the call's final receipt. Becoming final and being found by a retry are one
 * step: there is no moment in which a retry would run it again. */
static void settle(pending_t *p, const ink_receipt_t *rc) {
    /* A capability that answers "failed" is an outcome, not a fault in the system: WARN keeps it in the log without
     * forcing a flash write right away (ERROR does; integration-races in CI met such a flush at its worst moment). */
    ink_log(rc->status == INK_RC_FAILED ? INK_LOG_WARN : INK_LOG_INFO, "cap", "%.47s src=%d result=%d us=%llu", p->call.cap ? p->call.cap->id : "remote-or-unknown", p->o.src, rc->status, (unsigned long long)(osal_now_us() - p->log_started));
    p->replied = true;
    p->out = *rc;
    p->out.page_gen = ink_ui_content_version();
    activity_settle(p, rc);
    if (!p->repeat_safe) return;
    if (p->result && p->call.cap && p->call.cap->effect == INK_FX_READ) return; /* a repeat reads again: the result is not kept */
    seen_t *s = &CB.seen[CB.seen_next];
    CB.seen_next = (CB.seen_next + 1) % SEEN_N;
    snprintf(s->rid, sizeof s->rid, "%s", p->rid);
    snprintf(s->member, sizeof s->member, "%s", p->member);
    
    memcpy(s->identity, p->identity, 32);
    s->rc = p->out;
    s->rc.result = NULL; /* it goes with the slot */
}

static void answered(pending_t *p) {
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    p->answered = true;
    unlock_retire(p);
}

static void answer_job(void *arg) {
    pending_t *p = arg;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    ink_cap_done_fn done = p->o.done; /* NULL once a waiting ink_cap_invoke gave up on it */
    osal_mutex_unlock(CB.lock);
    bool handoff = p->call.cap && !strcmp(p->call.cap->id, "system.switch_member") && p->out.status == INK_RC_DONE && !p->out.result;
    bool hand_locked = opens_emergency(p->call.cap) || (p->hand_locked && (p->o.src == INK_SRC_TOUCH || p->o.src == INK_SRC_KEY));
    if (!handoff && p->member[0] && (!ink_member_exists(p->member) ||
        strcmp(p->member, ink_member_current()->id) || (!ink_member_space_open() && !hand_locked && p->o.src != INK_SRC_SYSTEM && p->out.status == INK_RC_DONE))) {
        memset(&p->out, 0, sizeof p->out); p->out.status = INK_RC_STALE;
    }
    if (done) done(p->o.ctx, &p->out);
    answered(p);
}

/* p->out is final: tell the caller, on its executor (ready: checked when the call was accepted). */
static void answer(pending_t *p) {
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    bool nobody = !p->o.done;
    osal_mutex_unlock(CB.lock);
    if (nobody) answered(p);
    else if (p->o.reply_exec == INK_EXEC_DIRECT || ink_exec_is_current(p->o.reply_exec)) answer_job(p);
    else if (ink_exec_post(p->o.reply_exec, &p->answer_job, answer_job, p) != 0) answered(p);
}

/* Under the lock: the provider is through with the call. True: the caller must be told now. */
static bool provider_done(pending_t *p) {
    if (p->finished) return false; /* a handler that called ink_cap_done without deferring */
    p->finished = true;
    if (p->replied) return false;  /* answered "timed out" before: this result comes too late */
    settle(p, &p->call.rc);
    return true;
}

/* The provider is done with the call (any task). */
static void finish(pending_t *p) {
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    bool late = !p->finished && p->replied, reply = provider_done(p);
    unlock_retire(p);
    if (reply) answer(p);
    else if (late) ink_probe_count("cap.late");
}

void ink_cap_result(ink_call_t *call, const char *json) {
    pending_t *p = (pending_t *)call;
    size_t n = json ? strlen(json) : 0;
    const char *prev = osal_mem_tag("cap");
    char *copy = n ? osal_malloc(n + 1, OSAL_MEM_EXTERNAL) : NULL;
    osal_mem_tag(prev);
    if (copy) memcpy(copy, json, n + 1);
    else if (n) ink_probe_count("cap.result_dropped");
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    char *old = p->result;
    p->result = copy;
    call->rc.result = copy;
    osal_mutex_unlock(CB.lock);
    osal_free(old);
}

void ink_cap_defer(ink_call_t *call) {
    pending_t *p = (pending_t *)call;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    p->deferred = true;
    osal_mutex_unlock(CB.lock);
}

void ink_cap_done(ink_call_t *call) { finish((pending_t *)call); }

uint32_t ink_cap_time_left_ms(const ink_call_t *call) {
    const pending_t *p = (const pending_t *)call;
    uint64_t now = osal_now_us();
    return p->deadline_us > now ? (uint32_t)((p->deadline_us - now) / 1000u) : 0;
}

const char *ink_cap_call_member(const ink_call_t *call) {
    const pending_t *p = (const pending_t *)call;
    return p->member;
}

static void timeout_cb(void *arg) { /* timer task: answer the overdue, re-arm for the rest */
    (void)arg;
    pending_t *due[POOL_N];
    int n = 0;
    uint64_t now = osal_now_us(), next = 0;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < POOL_N; i++) {
        pending_t *p = &CB.pool[i];
        if (!p->used || !p->accepted || p->remote || p->replied) continue;
        if (p->deadline_us <= now) {
            ink_receipt_t rc = {.status = INK_RC_FAILED};
            snprintf(rc.detail, sizeof rc.detail, "%s", p->started ? "timed out, it may still finish" : "timed out before it started");
            settle(p, &rc);
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
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    bool expired = p->replied; /* its deadline passed while it waited: it must not act any more */
    if (expired) p->finished = true;
    else {
        p->started = true;
        p->refs++; /* the provider may complete it from anywhere while run() is still on this stack */
    }
    if (expired) {
        unlock_retire(p);
        ink_probe_count("cap.expired");
        return;
    }
    osal_mutex_unlock(CB.lock);
    const ink_cap_t *c = p->call.cap;
    ink_receipt_t *rc = &p->call.rc;
    memset(rc, 0, sizeof *rc);
    char key[64];
    snprintf(key, sizeof key, "action.%s.%s", p->call.src == INK_SRC_AI ? "ai" : "user", p->id);
    ink_probe_count(key);
    bool ran = false;
    bool delivery = !p->o.hub_epoch || ink_hub_member_matches(p->member, p->o.hub_epoch, p->o.hub_connection);
    ink_rc_t verdict = !delivery || !ink_member_exists(p->member) || strcmp(p->member, ink_member_current()->id) ? reject(rc, INK_RC_STALE, "someone else is using the device now") : gate(c, p->call.src, p->call.page_gen, p->asked, rc);
    if (verdict == INK_RC_DONE || verdict == INK_RC_PENDING_CONFIRMATION) {
        p->hand_locked = c->while_locked || ink_ui_locked_allows(c->id);
        char why[96];
        int bad = p->json ? ink_args_from_json(&p->call.args, p->json, strlen(p->json), c->input, why, sizeof why)
                          : ink_args_check(&p->call.args, c->input, why, sizeof why);
        if (bad) reject(rc, INK_RC_INVALID, why); /* no card for what could not run anyway */
        else if (!time_data_allows(&p->call)) reject(rc, INK_RC_REJECTED, "only phone and SMS data after time runs out");
        else if (verdict == INK_RC_PENDING_CONFIRMATION) ink_confirm_hold(c, &p->call.args, p->call.src, p->call.page_gen, rc);
        else {
            rc->status = INK_RC_DONE;
            c->run(&p->call);
            ran = true;
        }
    }
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    p->refs--;
    bool later = ran && p->deferred; /* ink_cap_done or the deadline answers (ink_cap_done may have come already) */
    bool too_late = !later && !p->finished && p->replied, reply = !later && provider_done(p);
    unlock_retire(p);
    if (reply) answer(p);
    else if (too_late) ink_probe_count("cap.late");
}

static int remote_call(const char *name, const ink_args_t *args, const ink_call_opts_t *o);

/* Accept a call: a slot holding everything the call needs, its deadline running. NULL: not accepted.
 * A repeat of a request has been answered when this returns NULL with *repeat set. */
static pending_t *prepare(const char *id, const ink_cap_t *c, const ink_args_t *args, const ink_call_opts_t *o, bool remote, bool asked, bool *repeat) {
    uint32_t origin_epoch = o->hub_epoch ? o->hub_epoch : ink_hub_member_epoch();
    char *json = NULL;
    *repeat = false;
    if (!args && o->args_json && *o->args_json) {
        size_t n = strlen(o->args_json);
        json = osal_malloc(n + 1, OSAL_MEM_EXTERNAL);
        if (!json) return NULL;
        memcpy(json, o->args_json, n + 1);
    }
    const char *member = o->member ? o->member : ink_member_current()->id;
    ink_receipt_t checked = {0};
    ink_rc_t verdict = strcmp(member, ink_member_current()->id) ? INK_RC_STALE :
        remote ? (ink_member_space_open() ? INK_RC_DONE : INK_RC_REJECTED) :
        o->request_id ? gate(c, o->src, 0, asked, &checked) : INK_RC_DONE;
    uint8_t identity[32];
    ink_sha256_t sum;
    ink_sha256_init(&sum);
    ink_sha256_update(&sum, id, strlen(id) + 1);
    ink_sha256_update(&sum, &remote, sizeof remote);
    ink_sha256_update(&sum, &o->src, sizeof o->src);
    ink_sha256_update(&sum, &asked, sizeof asked);
    if (args) {
        char *encoded = osal_malloc(INK_ARGS_TEXT * 2, OSAL_MEM_EXTERNAL);
        int n = encoded ? ink_args_to_json(args, encoded, INK_ARGS_TEXT * 2) : -1;
        if (n < 0) { osal_free(encoded); osal_free(json); return NULL; }
        ink_sha256_update(&sum, encoded, (size_t)n);
        osal_free(encoded);
    } else ink_sha256_update(&sum, json ? json : "{}", json ? strlen(json) : 2);
    ink_sha256_final(&sum, identity);
    pending_t *p = claim(o->request_id, member, identity, verdict, repeat);
    if (!p || *repeat) {
        osal_free(json);
        if (p) { /* the first receipt again, or "queued": told like any other */
            if (p->cache_hit) ink_probe_count("cap.replayed");
            p->call.cap = c;
            p->o = *o;
            p->o.request_id = p->o.args_json = NULL;
            answer(p);
        }
        return NULL;
    }
    p->call.cap = c;
    p->origin_epoch = origin_epoch;
    p->log_started = osal_now_us();
    ink_log(INK_LOG_INFO, "cap", "call %.47s src=%d", c ? c->id : "remote-or-unknown", o->src);
    snprintf(p->id, sizeof p->id, "%s", id);
    if (args) p->call.args = *args;
    else ink_args_init(&p->call.args);
    p->json = json;
    p->call.src = o->src;
    p->call.page_gen = o->page_gen;
    /* whoever asks, asks for who is here now: if someone else has come by the time it runs, it does not run for them */
    snprintf(p->member, sizeof p->member, "%s", o->member ? o->member : ink_member_current()->id);
    if (needs_activity(c, o->src) && !strcmp(p->member, ink_member_current()->id)) {
        p->activity = activity_reserve(c, p->member);
        if (!p->activity) {
            osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
            p->finished = p->answered = true;
            unlock_retire(p);
            return NULL;
        }
    }
    p->o = *o;
    p->o.request_id = p->o.args_json = NULL; /* the caller's strings need not outlive the call */
    p->o.member = NULL;
    p->remote = remote;
    p->deadline_us = osal_now_us() + (uint64_t)timeout_ms(c) * 1000u;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    p->accepted = true;
    if (!remote && (!CB.timer_due || p->deadline_us < CB.timer_due)) { /* only ever brought forward */
        uint64_t now = osal_now_us();
        CB.timer_due = p->deadline_us;
        osal_timer_start(CB.timer, p->deadline_us > now ? p->deadline_us - now : 1, 0);
    }
    osal_mutex_unlock(CB.lock);
    return p;
}

static int call_with_intent(const char *addr, const ink_args_t *args, const ink_call_opts_t *o, bool asked) {
    if (!CB.pool || !addr || !o) return -1;
    if (o->request_id && strlen(o->request_id) >= RID_CAP) return -1; /* cut, it would not be recognised again */
    if (o->done && ink_exec_ready(o->reply_exec) != 0) return -1;     /* the receipt could not be delivered */
    if (!strncmp(addr, "hub/", 4)) return remote_call(addr + 4, args, o);
    const ink_cap_t *c = ink_cap_find(addr);
    if (!c && in_directory(addr)) return remote_call(addr, args, o); /* not here, but the hub has it */
    ink_exec_t e = c ? c->exec : INK_EXEC_DIRECT;
    if (ink_exec_ready(e) != 0) return -1;
    bool repeat;
    pending_t *p = prepare(addr, c, args, o, false, asked, &repeat);
    if (!p) return repeat ? 0 : -1;
    p->asked = asked;
    if (e == INK_EXEC_DIRECT || ink_exec_is_current(e)) start(p);
    else ink_exec_post(e, &p->start_job, start, p);
    return 0;
}

int ink_cap_call(const char *addr, const ink_args_t *args, const ink_call_opts_t *o) {
    return call_with_intent(addr, args, o, false);
}

typedef struct {
    ink_receipt_t *rc;
    int copied;
} sync_reply_t;

static void copy_receipt(void *ctx, const ink_receipt_t *rc) {
    sync_reply_t *s = ctx;
    *s->rc = *rc;
    s->rc->result = NULL; /* it goes with the slot: read results need ink_cap_call */
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
    bool repeat;
    pending_t *p = CB.pool ? prepare(id, c, args, &o, false, false, &repeat) : NULL;
    if (!p) return reject(rc, INK_RC_FAILED, "busy");
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    p->refs++; /* looked at below, however soon it completes */
    osal_mutex_unlock(CB.lock);
    start(p);
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    bool later = !p->replied;
    if (later) p->o.done = NULL; /* it completes later: nobody here to tell, rc is about to go out of scope */
    osal_mutex_unlock(CB.lock);
    /* answered in another task at this very moment: the copy into rc is on its way */
    while (!later && !__atomic_load_n(&sync.copied, __ATOMIC_ACQUIRE)) osal_task_yield();
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    p->refs--;
    unlock_retire(p);
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
        int found = ink_json_find(j, sc, "result");
        if (found >= 0) { /* a hub capability's result is as good as a local one's */
            size_t n = (size_t)(j->tok[found].end - j->tok[found].start);
            char *copy = osal_malloc(n + 1, OSAL_MEM_EXTERNAL);
            if (copy) {
                memcpy(copy, j->text + j->tok[found].start, n);
                copy[n] = 0;
                ink_cap_result(&p->call, copy);
                osal_free(copy);
            }
        }
    }
    if (!ink_member_exists(p->member) || strcmp(p->member, ink_member_current()->id) || !ink_member_space_open()) {
        memset(rc, 0, sizeof *rc); rc->status = INK_RC_STALE;
    }
    char key[48];
    snprintf(key, sizeof key, "cap.remote.%s", ink_rc_name(rc->status));
    ink_probe_count(key);
    finish(p);
}

static int remote_call(const char *name, const ink_args_t *args, const ink_call_opts_t *o) {
    bool repeat;
    pending_t *p = prepare(name, NULL, args, o, true, false, &repeat);
    if (!p) return repeat ? 0 : -1;
    if (!p->rid[0]) snprintf(p->rid, sizeof p->rid, "d%08x-%u", (unsigned)CB.boot_tag, (unsigned)__atomic_add_fetch(&CB.next_rid, 1, __ATOMIC_RELAXED));
    const char *prev = osal_mem_tag("cap");
    char *params = osal_malloc(INK_ARGS_TEXT * 2 + 256, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!params) {
        osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
        p->finished = p->answered = true; /* not accepted after all: nobody will be told */
        unlock_retire(p);
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
    ink_jw_raw(&w, ",\"member\":");
    ink_jw_str(&w, p->member);
    ink_jw_raw(&w, ",\"page_gen\":");
    ink_jw_int(&w, p->call.page_gen);
    ink_jw_raw(&w, "}}");
    int r = w.overflow ? -1 : ink_hub_call_for("tools/call", params, 30000000, remote_reply, p, p->member, p->origin_epoch);
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

#define SUBS_MAX 64 /* 27 system subscriptions by 2026-11-09, tests add theirs */

static struct {
    const char *prefix;
    size_t plen;
    ink_event_fn fn;
    void *ctx;
    ink_exec_t exec;
    int handle;
} SUBS[SUBS_MAX] INK_PSRAM_BSS;
static int nsubs, next_sub_handle;

typedef struct {
    ink_exec_job_t job;
    ink_event_fn fn;
    void *ctx;
    int sub, handle;
    char id[48], member[INK_MEMBER_ID_MAX];
    ink_args_t payload;
} event_job_t;

static int subscribe(const char *prefix, ink_event_fn fn, void *ctx, ink_exec_t exec, bool removable) {
    if (!CB.lock || !fn) return -1;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    int r = -1;
    for (int i = 0; i < SUBS_MAX; i++) if (!SUBS[i].fn) {
        r = ++next_sub_handle;
        SUBS[i] = (typeof(SUBS[0])){prefix ? prefix : "", prefix ? strlen(prefix) : 0, fn, ctx, exec, removable ? r : 0};
        if (i == nsubs) nsubs++;
        break;
    }
    osal_mutex_unlock(CB.lock);
    return r;
}
int ink_cap_subscribe(const char *prefix, ink_event_fn fn, void *ctx, ink_exec_t exec) {
    return subscribe(prefix, fn, ctx, exec, false) < 0 ? -1 : 0;
}
int ink_cap_subscribe_handle(const char *prefix, ink_event_fn fn, void *ctx, ink_exec_t exec) {
    return exec == INK_EXEC_DIRECT ? -1 : subscribe(prefix, fn, ctx, exec, true);
}
int ink_cap_unsubscribe(int handle) {
    if (!CB.lock || handle <= 0) return -1;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    int r = -1;
    for (int i = 0; i < SUBS_MAX; i++) if (SUBS[i].handle == handle && SUBS[i].fn &&
        SUBS[i].exec != INK_EXEC_DIRECT && ink_exec_is_current(SUBS[i].exec)) { SUBS[i].fn = NULL; r = 0; }
    osal_mutex_unlock(CB.lock);
    return r;
}

static void event_job(void *arg) {
    event_job_t *e = arg;
    bool live = true;
    if (e->handle) {
        osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
        live = SUBS[e->sub].handle == e->handle && SUBS[e->sub].fn;
        osal_mutex_unlock(CB.lock);
    }
    if (live && ink_member_exists(e->member)) e->fn(e->ctx, e->id, &e->payload);
    osal_free(e);
}

/* Events to the AI (system services design §1.2 rule 3): what is registered and offered goes to the hub through the
 * hub task, the latest 8 wait for it to be online. */
#define EVENTS_KEPT 8
static struct {
    uint32_t seq;
    bool delivered;
    int64_t at_s;
    char id[48];
    char member[INK_MEMBER_ID_MAX]; /* whose it is: who it says, or who was there when it happened */
    ink_args_t args;
} EV[EVENTS_KEPT] INK_PSRAM_BSS;
static uint32_t ev_next, ev_sent, ev_acked; /* sequence numbers, under CB.lock */

static void keep_event(const char *id, const ink_args_t *payload, const char *whose) {
    const ink_cap_t *c = ink_cap_find(id);
    if (!CB.lock || !c || c->form != INK_FORM_EVENT || !exposed(c, INK_SRC_AI)) return;
    int64_t wall = osal_wall_us();
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    if (ev_next == UINT32_MAX) { osal_mutex_unlock(CB.lock); ink_probe_count("cap.events_exhausted"); return; }
    uint32_t seq = ++ev_next;
    typeof(EV[0]) *e = &EV[seq % EVENTS_KEPT];
    e->seq = seq;
    e->delivered = false;
    
    e->at_s = wall > 0 ? wall / 1000000 : 0;
    snprintf(e->id, sizeof e->id, "%s", id);
    snprintf(e->member, sizeof e->member, "%s", whose);
    if (payload) e->args = *payload;
    else ink_args_init(&e->args);
    if (ev_next - ev_acked > EVENTS_KEPT) {
        ev_acked = ev_next - EVENTS_KEPT; /* the oldest was just written over */
        if (ev_sent < ev_acked) ev_sent = ev_acked;
        ink_probe_count("cap.events_lost_offline");
    }
    osal_mutex_unlock(CB.lock);
    if (ink_hub_online()) ink_hub_wake();
}

#define EVENT_JSON_CAP 1024

static void send_events(void) { /* hub task */
    for (;;) {
        osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
        bool any = ev_sent != ev_next;
        osal_mutex_unlock(CB.lock);
        if (!any) return;
        char *out = osal_malloc(EVENT_JSON_CAP + sizeof(ink_args_t), OSAL_MEM_EXTERNAL);
        if (!out) return;
        ink_args_t *args = (ink_args_t *)(out + EVENT_JSON_CAP);
        char id[48] = "", whose[INK_MEMBER_ID_MAX] = "";
        int64_t at = 0;
        osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
        uint32_t seq = ev_sent + 1;
        const typeof(EV[0]) *e = &EV[seq % EVENTS_KEPT];
        snprintf(id, sizeof id, "%s", e->id);
        snprintf(whose, sizeof whose, "%s", e->member);
        at = e->at_s;
        *args = e->args;
        osal_mutex_unlock(CB.lock);
        /* it goes only while it is theirs who is here: the next one's AI does not hear about it */
        bool theirs = ink_member_exists(whose) && ink_member_space_open() && !strcmp(whose, ink_member_current()->id);
        int head = snprintf(out, EVENT_JSON_CAP, "{\"event\":\"%s\",\"at_s\":%lld,\"seq\":%lu,\"boot\":%lu,\"member\":\"%s\",\"args\":",
                            id, (long long)at, (unsigned long)seq, (unsigned long)CB.boot_tag, whose);
        int body = ink_args_to_json(args, out + head, EVENT_JSON_CAP - (size_t)head - 2);
        int r = 0;
        if (!theirs) ink_probe_count("cap.event_for_another_member");
        else if (body >= 0) {
            out[head + body] = '}';
            out[head + body + 1] = 0;
            r = ink_hub_notify("device/event", out);
        } else ink_probe_count("cap.event_too_long");
        osal_free(out);
        if (r != 0) return; /* the queue is full: the next time round */
        osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
        if (theirs && body >= 0 && EV[seq % EVENTS_KEPT].seq == seq) EV[seq % EVENTS_KEPT].delivered = true;
        if (ev_sent + 1 == seq) ev_sent = seq; /* still the oldest: nothing wrote over it */
        osal_mutex_unlock(CB.lock);
    }
}

void ink_cap_emit_for(const char *id, const ink_args_t *payload, const char *whose) {
    if (!whose || !ink_member_exists(whose)) return;
    char key[64];
    snprintf(key, sizeof key, "event.%s", id);
    ink_probe_count(key);
    keep_event(id, payload, whose);
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < nsubs; i++) {
        typeof(SUBS[0]) sub = SUBS[i];
        if (!sub.fn || strncmp(id, sub.prefix, sub.plen)) continue;
        osal_mutex_unlock(CB.lock);
        const char *prev = osal_mem_tag("cap");
        event_job_t *e = osal_calloc(1, sizeof *e, OSAL_MEM_EXTERNAL);
        osal_mem_tag(prev);
        if (!e) {
            ink_probe_count("event.dropped");
            osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
            continue;
        }
        snprintf(e->member, sizeof e->member, "%s", whose);
        
        e->fn = sub.fn; e->ctx = sub.ctx; e->sub = i; e->handle = sub.handle;
        snprintf(e->id, sizeof e->id, "%s", id);
        if (payload) e->payload = *payload;
        else ink_args_init(&e->payload);
        ink_args_str(&e->payload, "member", whose);
        if (ink_exec_post(sub.exec, &e->job, event_job, e) != 0) {
            ink_probe_count("event.dropped");
            osal_free(e);
        }
        osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    }
    osal_mutex_unlock(CB.lock);
}

void ink_cap_emit(const char *id, const ink_args_t *payload) {
    char whose[INK_MEMBER_ID_MAX] = "";
    if (payload) ink_args_get_str(payload, "member", whose, sizeof whose);
    if (!whose[0]) snprintf(whose, sizeof whose, "%s", ink_member_current()->id);
    ink_cap_emit_for(id, payload, whose);
}

/* What the current member may use now, as the AI sees it. */
static bool listed(const ink_cap_t *c) {
    if ((!ink_member_space_open() && !opens_emergency(c)) || !on_its_page(c) || !exposed(c, INK_SRC_AI)) return false;
    const ink_member_t *m = ink_member_current();
    return m->role != INK_ROLE_CHILD || (c->effect != INK_FX_SYSTEM && ink_member_allows(c->id, c->child_ok) && ink_member_time_allows(c->id));
}

bool ink_cap_ai_callable(const ink_cap_t *c) { return c && listed(c) && c->form != INK_FORM_EVENT; }

static void manifest_entry(ink_jw_t *w, const ink_cap_t *c, bool first);

int ink_cap_manifest_json(char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "[");
    bool first = true;
    for (int i = 0; i < nreg; i++) {
        if (!ink_cap_ai_callable(REG[i])) continue;
        manifest_entry(&w, REG[i], first);
        first = false;
    }
    ink_jw_raw(&w, "]");
    return w.overflow ? -1 : (int)w.len;
}

int ink_cap_manifest_page(int start, const char *prefix, char *out, size_t cap, int *next) {
    ink_jw_t w;
    if (cap < 2) return -1;
    ink_jw_init(&w, out, cap - 1); /* room for the closing bracket */
    ink_jw_raw(&w, "[");
    bool first = true;
    *next = -1;
    size_t plen = prefix ? strlen(prefix) : 0;
    for (int i = start < 0 ? 0 : start; i < nreg; i++) {
        if (!ink_cap_ai_callable(REG[i]) || (plen && strncmp(REG[i]->id, prefix, plen))) continue;
        size_t before = w.len;
        manifest_entry(&w, REG[i], first);
        if (w.overflow) { /* the next page starts here; one entry alone must fit */
            w.overflow = false;
            w.len = before;
            if (first) {
                ink_probe_count("cap.manifest_entry_too_long");
                continue;
            }
            *next = i;
            break;
        }
        first = false;
    }
    out[w.len++] = ']';
    out[w.len] = 0;
    return (int)w.len;
}

static void manifest_entry(ink_jw_t *wp, const ink_cap_t *c, bool first) {
#define w (*wp)
    {
        ink_jw_raw(&w, first ? "{\"name\":" : ",{\"name\":");
        ink_jw_str(&w, c->id);
        ink_jw_raw(&w, ",\"title\":");
        ink_jw_str(&w, c->title);
        ink_jw_raw(&w, ",\"effect\":");
        ink_jw_str(&w, ink_effect_name(c->effect));
        ink_jw_raw(&w, ",\"confirm\":");
        ink_jw_str(&w, CONFIRM_NAME[confirm_for(c, INK_SRC_AI, false)]);
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
#undef w
}

int ink_cap_ids_json(char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "[");
    bool first = true;
    for (int i = 0; i < nreg; i++) {
        if (!ink_cap_ai_callable(REG[i])) continue; /* what can be called */
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
    if (rc->result) {
        ink_jw_raw(&w, ",\"result\":");
        ink_jw_raw(&w, rc->result);
    }
    ink_jw_raw(&w, "}");
    return w.overflow ? -1 : (int)w.len;
}

/* ------------------------------------------------------------ undo (foundation §6.2: reversible writes say how long) */

#define UNDO_MAX 4

typedef struct {
    char object[48], cap[48], member[16];
    ink_args_t args;
    uint64_t until_us;
    uint32_t revision;
} undo_t;

static undo_t UNDO[UNDO_MAX] INK_PSRAM_BSS;
static int undo_next;

void ink_cap_undoable(ink_call_t *call, const char *cap, const ink_args_t *args, uint32_t ms) {
    pending_t *p = (pending_t *)call;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    if (!ink_member_exists(p->member) ||
        strcmp(p->member, ink_member_current()->id) || !ink_member_space_open()) {
        osal_mutex_unlock(CB.lock);
        return;
    }
    undo_t *u = &UNDO[undo_next];
    undo_next = (undo_next + 1) % UNDO_MAX;
    snprintf(u->object, sizeof u->object, "%s", call->rc.object);
    snprintf(u->cap, sizeof u->cap, "%s", cap);
    snprintf(u->member, sizeof u->member, "%s", p->member);
    if (args) u->args = *args;
    else ink_args_init(&u->args);
    u->until_us = osal_now_us() + (uint64_t)ms * 1000u;
    u->revision = call->rc.revision;
    
    osal_mutex_unlock(CB.lock);
    call->rc.undo_ms = ms;
}

static void undo_done(void *ctx, const ink_receipt_t *rc) {
    ink_call_t *c = ctx;
    if (rc->status == INK_RC_DONE) {
        c->rc.status = INK_RC_UNDONE;
        snprintf(c->rc.say, sizeof c->rc.say, TR("撤销了"));
    } else {
        c->rc.status = rc->status;
        snprintf(c->rc.object, sizeof c->rc.object, "%s", rc->object);
        snprintf(c->rc.say, sizeof c->rc.say, "%s", rc->say);
        snprintf(c->rc.detail, sizeof c->rc.detail, "%s", rc->detail);
    }
    ink_cap_done(c);
}

static void cap_undo(ink_call_t *c) {
    char object[48];
    ink_args_get_str(&c->args, "object", object, sizeof object);
    int64_t revision = 0;
    bool bound = ink_args_get_int(&c->args, "revision", &revision);
    undo_t u = {0};
    bool found = false;
    uint64_t now = osal_now_us();
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    for (int n = 0; n < UNDO_MAX && !found; n++) {
        int i = (undo_next + UNDO_MAX - 1 - n) % UNDO_MAX;
        undo_t *e = &UNDO[i];
        if (ink_member_exists(NULL) && e->until_us > now && (!bound || e->revision == (uint32_t)revision) && !strcmp(e->object, object) && !strcmp(e->member, ink_member_current()->id)) {
            u = *e;
            e->until_us = 0; /* once */
            found = true;
        }
    }
    osal_mutex_unlock(CB.lock);
    if (!found) {
        reject(&c->rc, INK_RC_FAILED, "nothing to undo, or too late");
        return;
    }
    snprintf(c->rc.object, sizeof c->rc.object, "%s", object);
    ink_cap_defer(c); /* Settings recheck their own risk; a permission inverse is still a permission change. */
    ink_source_t src = !strcmp(u.cap, "settings.set") ? c->src : INK_SRC_CONFIRMED;
    ink_call_opts_t o = {.src = src, .member = u.member, .reply_exec = INK_EXEC_DIRECT, .done = undo_done, .ctx = c};
    if (ink_cap_call(u.cap, &u.args, &o) != 0) {
        reject(&c->rc, INK_RC_FAILED, "busy");
        ink_cap_done(c);
    }
}

/* ------------------------------------------------------------ finding capabilities (foundation §6.5: the whole list is too much to keep in every turn) */

static void cap_domains(ink_call_t *c) {
    char *out = osal_malloc(2048, OSAL_MEM_EXTERNAL);
    if (!out) {
        reject(&c->rc, INK_RC_FAILED, "out of memory");
        return;
    }
    ink_jw_t w;
    ink_jw_init(&w, out, 2048);
    ink_jw_raw(&w, "{");
    bool first = true;
    for (int i = 0; i < nreg; i++) {
        const char *dot = strchr(REG[i]->id, '.');
        size_t n = dot ? (size_t)(dot - REG[i]->id) : strlen(REG[i]->id);
        bool seen = false;
        int count = 0;
        for (int k = 0; k < nreg; k++) {
            if (!listed(REG[k]) || strncmp(REG[k]->id, REG[i]->id, n) || (REG[k]->id[n] != '.' && REG[k]->id[n])) continue;
            if (k < i) seen = true;
            count++;
        }
        if (seen || !count) continue;
        if (!first) ink_jw_raw(&w, ",");
        first = false;
        ink_jw_strn(&w, REG[i]->id, n);
        ink_jw_raw(&w, ":");
        ink_jw_int(&w, count);
    }
    ink_jw_raw(&w, "}");
    if (w.overflow) reject(&c->rc, INK_RC_FAILED, "too long");
    else ink_cap_result(c, out);
    osal_free(out);
}

static void cap_find(ink_call_t *c) {
    char q[48] = "";
    ink_args_get_str(&c->args, "query", q, sizeof q);
    char *out = osal_malloc(4096, OSAL_MEM_EXTERNAL);
    if (!out) {
        reject(&c->rc, INK_RC_FAILED, "out of memory");
        return;
    }
    ink_jw_t w;
    ink_jw_init(&w, out, 4095);
    ink_jw_raw(&w, "[");
    bool first = true;
    for (int i = 0; i < nreg; i++) {
        const ink_cap_t *k = REG[i];
        if (!listed(k) || (!strstr(k->id, q) && !strstr(k->title, q))) continue;
        size_t before = w.len;
        manifest_entry(&w, k, first);
        if (w.overflow) { /* as many as fit: the query can be narrowed */
            w.overflow = false;
            w.len = before;
            break;
        }
        first = false;
    }
    out[w.len++] = ']';
    out[w.len] = 0;
    ink_cap_result(c, out);
    osal_free(out);
}

static const ink_member_t *activity_member(ink_call_t *c, char *member) {
    snprintf(member, INK_MEMBER_ID_MAX, "%s", ink_cap_call_member(c));
    ink_args_get_str(&c->args, "member", member, INK_MEMBER_ID_MAX);
    for (int i = 0; ink_member_at(i); i++) {
        const ink_member_t *m = ink_member_at(i);
        if (strcmp(m->id, member)) continue;
        if (!strcmp(member, ink_cap_call_member(c)) ||
            (ink_member_current()->role == INK_ROLE_ADULT && m->role == INK_ROLE_CHILD)) return m;
        break;
    }
    reject(&c->rc, INK_RC_REJECTED, "not your or your child's records");
    return NULL;
}

static void cap_activity_clear(ink_call_t *c) {
    char member[INK_MEMBER_ID_MAX];
    if (!activity_member(c, member)) return;
    ink_db_t *db = ink_db_open_member(&ACTIVITY, member);
    if (!db) { reject(&c->rc, INK_RC_FAILED, "activity log unavailable"); return; }
    ink_db_cursor_t cursor = {0}; ink_rec_t rec;
    int got;
    while ((got = ink_db_next(db, &cursor, &rec, NULL, 0)) == 1) {
        if (ink_db_del(db, rec.id, c->src) != INK_DB_OK) { got = -1; break; }
    }
    if (got < 0 || ink_db_wait(db, ink_db_last_ver(db), 5000000) != INK_DB_OK)
        reject(&c->rc, INK_RC_FAILED, "some activity records could not be deleted");
    ink_db_close(db);
}

static void cap_activity_list(ink_call_t *c) { /* exec.bg */
    char member[INK_MEMBER_ID_MAX], cursor[48] = "";
    const ink_member_t *target = activity_member(c, member);
    if (!target) return;
    ink_db_t *db = ink_db_open_member(&ACTIVITY, member);
    char *out = osal_malloc(16384, OSAL_MEM_EXTERNAL);
    if (!db || !out) {
        if (db) ink_db_close(db);
        osal_free(out);
        reject(&c->rc, INK_RC_FAILED, "activity log unavailable");
        return;
    }
    ink_db_cursor_t at = {.desc = true};
    if (ink_args_get_str(&c->args, "cursor", cursor, sizeof cursor) > 0) {
        long long sort = 0; unsigned id = 0;
        if (sscanf(cursor, "%lld:%u", &sort, &id) != 2) {
            ink_db_close(db); osal_free(out);
            reject(&c->rc, INK_RC_INVALID, "invalid cursor");
            return;
        }
        at.sort = sort; at.id = id; at.started = true;
    }
    int64_t limit = 20;
    ink_args_get_int(&c->args, "limit", &limit);
    int64_t now = osal_wall_us() / 1000000;
    int64_t keep = (target->role == INK_ROLE_CHILD ? 180ll : 365ll) * 86400;
    ink_jw_t w;
    ink_jw_init(&w, out, 16384 - 80);
    ink_jw_raw(&w, "{\"items\":[");
    ink_rec_t rec;
    char row[512];
    int shown = 0, total = 0;
    bool more = false;
    ink_db_cursor_t last = at;
    ink_db_cursor_t count_at = {0};
    while (ink_db_next(db, &count_at, &rec, NULL, 0) == 1)
        if (!(rec.sort > 0 && now > 0 && rec.sort < now - keep)) total++;
    while (ink_db_next(db, &at, &rec, row, sizeof row - 1) == 1) {
        if (rec.sort > 0 && now > 0 && rec.sort < now - keep) continue;
        if (shown >= limit) { more = true; break; }
        row[rec.len < sizeof row ? rec.len : sizeof row - 1] = 0;
        size_t before = w.len;
        if (shown) ink_jw_raw(&w, ",");
        ink_jw_raw(&w, row);
        if (w.overflow) { w.overflow = false; w.len = before; out[w.len] = 0; more = true; break; }
        shown++;
        last = at;
    }
    ink_jw_raw(&w, "],\"total\":"); ink_jw_int(&w, total);
    ink_jw_raw(&w, ",\"keep_days\":"); ink_jw_int(&w, keep / 86400);
    if (more) {
        char next[80];
        snprintf(next, sizeof next, ",\"next\":\"%lld:%u\"", (long long)last.sort, (unsigned)last.id);
        ink_jw_raw(&w, next);
    }
    ink_jw_raw(&w, "}");
    ink_db_close(db);
    if (strcmp(ink_cap_call_member(c), ink_member_current()->id) || !ink_member_space_open()) reject(&c->rc, INK_RC_STALE, "member changed");
    else ink_cap_result(c, out);
    osal_free(out);
}

static const ink_cap_t BUS_CAPS[] = {
    {.id = "activity.clear", .title = "删除全部活动记录", .ask = TR_NOOP("删除 {member} 的全部活动记录，找不回来。对话、记忆和运行记录暂不能删除。"),
     .effect = INK_FX_LOCAL_DESTRUCTIVE, .ask_by_hand = true, .exec = INK_EXEC_BG,
     .input = "{\"type\":\"object\",\"properties\":{\"member\":{\"type\":\"string\",\"maxLength\":15}},\"required\":[\"member\"]}", .run = cap_activity_clear},
    {.id = "activity.list", .title = "查看 AI 做过的事", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG,
     .input = "{\"type\":\"object\",\"properties\":{\"member\":{\"type\":\"string\",\"maxLength\":15},\"cursor\":{\"type\":\"string\",\"maxLength\":47},\"limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":100}}}",
     .run = cap_activity_list},
    {.id = "system.undo",
     .title = "撤销刚才的操作",
     .input = "{\"type\":\"object\",\"properties\":{\"object\":{\"type\":\"string\",\"maxLength\":47},\"revision\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":4294967295}},\"required\":[\"object\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_undo},
    {.id = "caps.domains", .title = "有哪些类能力（每类多少个）", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_domains},
    {.id = "caps.find",
     .title = "按关键词查能力（带完整描述）",
     .input = "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\",\"maxLength\":47}},\"required\":[\"query\"]}",
     .effect = INK_FX_READ,
     .child_ok = true,
     .exec = INK_EXEC_DIRECT,
     .run = cap_find},
};

static int register_bus_caps(void) { return ink_cap_register(BUS_CAPS, (int)(sizeof BUS_CAPS / sizeof BUS_CAPS[0])); }

/* ------------------------------------------------------------ hub tools */

typedef struct {
    ink_exec_job_t job;
    int64_t id;
    bool list, asked;
    char member[INK_MEMBER_ID_MAX];
    int cursor;        /* tools/list: the page */
    char domain[16];   /* tools/list: only this domain */
    uint32_t page_gen;
    uint32_t epoch, connection;
    uint32_t confirm_token;
    uint64_t received_us;
    char name[48];
    char rid[RID_CAP]; /* _meta.request_id: a retry gets the first receipt again */
    char args[];       /* JSON object text */
} tool_job_t;

static void tool_done(void *ctx, const ink_receipt_t *rc) { /* where the capability finished */
    tool_job_t *t = ctx;
    if (!ink_hub_member_matches(t->member, t->epoch, t->connection)) {
        if (t->connection == ink_hub_connection()) ink_hub_respond_error(t->id, -32023, "stale_member");
        goto finished;
    }
    char key[48];
    snprintf(key, sizeof key, "tools.calls.%s", ink_rc_name(rc->status));
    ink_probe_count(key);
    const char *prev = osal_mem_tag("tools");
    size_t cap = 768 + (rc->result ? strlen(rc->result) : 0);
    char *out = osal_malloc(cap, OSAL_MEM_EXTERNAL);
    osal_mem_tag(prev);
    if (!out || ink_receipt_json(rc, out, cap) < 0) ink_hub_respond_error(t->id, -32603, out ? "receipt too long" : "out of memory");
    else {
        if (t->confirm_token) { size_t n = strlen(out); snprintf(out + n - 1, cap - n + 1, ",\"confirm_token\":%lu}", (unsigned long)t->confirm_token); }
        ink_hub_respond_for(t->id, out, t->member, t->epoch, t->connection);
    }
    osal_free(out);
finished:
    if (t->confirm_token) ink_confirm_result(t->confirm_token, t->name, "accepted", rc, t->member);
    osal_free(t);
}

#define TOOLS_PAGE 6144

static void tool_run(void *arg) { /* UI task */
    tool_job_t *t = arg;
    if (!ink_hub_member_matches(t->member, t->epoch, t->connection)) {
        if (t->connection == ink_hub_connection()) ink_hub_respond_error(t->id, -32023, "stale_member");
        osal_free(t);
        return;
    }
    if (ink_exec_is_current(INK_EXEC_BG)) {
        ink_receipt_t rc = {0};
        const ink_cap_t *c = ink_cap_find(t->name);
        ink_call_opts_t o = {.member = t->member, .page_gen = t->page_gen, .request_id = t->rid[0] ? t->rid : NULL, .args_json = t->args};
        if (gate(c, INK_SRC_AI, t->page_gen, t->asked, &rc) == INK_RC_DONE) {
            t->confirm_token = ink_confirm_track(c, &o, &rc);
            if (!t->confirm_token) { tool_done(t, &rc); return; }
        }
        ink_exec_post(INK_EXEC_UI, &t->job, tool_run, t); return;
    }
    if (t->list) { /* paged (architecture: progressive disclosure): caps.domains and caps.find say what there is */
        const char *prev = osal_mem_tag("tools");
        char *out = osal_malloc(TOOLS_PAGE, OSAL_MEM_EXTERNAL);
        osal_mem_tag(prev);
        int next = -1, n = out ? ink_cap_manifest_page(t->cursor, t->domain[0] ? t->domain : NULL, out + 9, TOOLS_PAGE - 48, &next) : -1;
        if (n < 0) ink_hub_respond_error(t->id, -32603, "out of memory");
        else {
            memcpy(out, "{\"tools\":", 9);
            if (next >= 0) snprintf(out + 9 + n, 48, ",\"nextCursor\":\"%d\"}", next);
            else snprintf(out + 9 + n, 48, "}");
            ink_hub_respond_for(t->id, out, t->member, t->epoch, t->connection);
        }
        osal_free(out);
        osal_free(t);
        return;
    }
    ink_ui_cause(t->received_us, "ai.tool_to_visible_ms"); /* what the capability changes on screen */
    ink_call_opts_t o = {.src = INK_SRC_AI, .member = t->member, .hub_epoch = t->epoch, .hub_connection = t->connection, .page_gen = t->page_gen, .request_id = t->rid[0] ? t->rid : NULL, .args_json = t->args,
                         .reply_exec = INK_EXEC_DIRECT, .done = tool_done, .ctx = t};
    if (call_with_intent(t->name, NULL, &o, t->asked) != 0) {
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
    char target[INK_MEMBER_ID_MAX];
    snprintf(target, sizeof target, "%s", ink_member_current()->id);
    uint32_t connection = ink_hub_connection();
    int64_t epoch = ink_hub_member_epoch();
    if (!list && (ink_json_str(j, ink_json_find(j, params, "_meta.member"), target, sizeof target) <= 0 ||
        !ink_json_int(j, ink_json_find(j, params, "_meta.epoch"), &epoch) || epoch <= 0 || epoch > UINT32_MAX ||
        !ink_hub_member_matches(target, (uint32_t)epoch, connection))) {
        ink_hub_respond_error(id, -32023, "stale_member");
        return;
    }
    /* a request id is compared whole: one that does not fit, or is not a string, could run a retry again */
    int rid_t = list ? -1 : ink_json_find(j, params, "_meta.request_id");
    if (rid_t >= 0 && (j->tok[rid_t].type != 4 || j->tok[rid_t].end - j->tok[rid_t].start >= RID_CAP)) {
        ink_hub_respond_error(id, -32602, "request_id must be a short string");
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
    t->epoch = (uint32_t)epoch;
    t->connection = connection;
    snprintf(t->member, sizeof t->member, "%s", target);
    if (list) {
        char cursor[16];
        if (ink_json_str(j, ink_json_find(j, params, "cursor"), cursor, sizeof cursor) > 0) t->cursor = atoi(cursor);
        ink_json_str(j, ink_json_find(j, params, "domain"), t->domain, sizeof t->domain);
    }
    if (!list) {
        int intent = ink_json_find(j, params, "_meta.asked");
        t->asked = intent >= 0 && j->tok[intent].type == 8 && ink_json_eq(j, intent, "true");
        int member = ink_json_find(j, params, "_meta.member");
        if (member >= 0 && (ink_json_str(j, member, t->member, sizeof t->member) < 0 || !ink_layout_member_ok(t->member))) {
            osal_free(t);
            ink_hub_respond_error(id, -32602, "invalid member");
            return;
        }
        if (ink_json_str(j, ink_json_find(j, params, "name"), t->name, sizeof t->name) < 0) {
            osal_free(t);
            ink_hub_respond_error(id, -32602, "tools/call needs a name");
            return;
        }
        int undo = ink_json_find(j, params, "_meta.undo_ms");
        int64_t undo_ms = 0;
        if (undo >= 0 && (!ink_json_int(j, undo, &undo_ms) || undo_ms != 3000)) {
            osal_free(t);
            ink_hub_respond_error(id, -32602, "undo_ms must be 3000");
            return;
        }
        /* Never bypass a requested display-and-cancel window before its presenter exists. */
        if (undo >= 0 || (t->asked && (!strcmp(t->name, "message.send") || !strcmp(t->name, "call.dial")))) {
            osal_free(t);
            ink_hub_respond_error(id, -32021, "automatic undo window unavailable");
            return;
        }
        
        int64_t pg = 0;
        if (ink_json_int(j, ink_json_find(j, params, "page_gen"), &pg) || ink_json_int(j, ink_json_find(j, params, "_meta.page_gen"), &pg))
            t->page_gen = (uint32_t)pg;
        ink_json_str(j, ink_json_find(j, params, "_meta.request_id"), t->rid, sizeof t->rid);
        if (alen) memcpy(t->args, j->text + j->tok[args_t].start, alen);
        t->args[alen] = 0;
    }
    const ink_cap_t *c = list ? NULL : ink_cap_find(t->name);
    bool commit = c && ink_hub_member_matches(t->member, t->epoch, t->connection) && needs_activity(c, INK_SRC_AI) && confirm_for(c, INK_SRC_AI, t->asked) == INK_CONFIRM_NONE;
    if (ink_exec_post(commit ? INK_EXEC_BG : INK_EXEC_UI, &t->job, tool_run, t) != 0) {
        ink_hub_respond_error(id, -32603, "busy");
        osal_free(t);
    } else ink_probe_count("tools.accepted");
}

/* ------------------------------------------------------------ the hub's directory (step 8) */

static struct {
    remote_cap_t got[DIR_MAX];
    char cursors[DIR_MAX][64];
    int n, pages;
    uint32_t generation, epoch, connection;
    int64_t revision;
    bool has_revision;
} DF INK_PSRAM_BSS;

static void dir_reply(void *ctx, int status, const ink_json_t *j, int result) { /* hub task */
    if ((uint32_t)(uintptr_t)ctx != DF.generation || DF.epoch != ink_hub_member_epoch() ||
        DF.connection != ink_hub_connection()) return;
    if (status != INK_HUB_OK) goto failed;
    int list = ink_json_find(j, result, "tools");
    if (list < 0 || j->tok[list].type != 2) goto failed;
    int64_t revision = 0;
    bool has_revision = ink_json_int(j, ink_json_find(j, result, "rev"), &revision);
    if (DF.pages && (has_revision != DF.has_revision || (has_revision && revision != DF.revision))) goto failed;
    DF.revision = revision;
    DF.has_revision = has_revision;
    int t = list + 1;
    for (int k = 0; k < j->tok[list].size && t < j->ntok; k++) {
        if (DF.n == DIR_MAX || j->tok[t].type != 1) goto failed;
        int end = j->tok[t].end, next = t + 1;
        while (next < j->ntok && j->tok[next].start < end) next++;
        remote_cap_t *c = &DF.got[DF.n];
        char form[12] = "";
        if (ink_json_str(j, ink_json_find(j, t, "name"), c->id, sizeof c->id) <= 0) goto failed;
        for (int i = 0; i < DF.n; i++) if (!strcmp(DF.got[i].id, c->id)) goto failed;
        ink_json_str(j, ink_json_find(j, t, "form"), form, sizeof form);
        c->form = !strcmp(form, "session") ? INK_FORM_SESSION : !strcmp(form, "stream") ? INK_FORM_STREAM : INK_FORM_CALL;
        DF.n++;
        t = next;
    }
    int next = ink_json_find(j, result, "nextCursor");
    if (next >= 0) {
        char cursor[64];
        if (DF.pages >= DIR_MAX || ink_json_str(j, next, cursor, sizeof cursor) <= 0 || !j->tok[list].size) goto failed;
        for (int i = 0; i < DF.pages; i++) if (!strcmp(cursor, DF.cursors[i])) goto failed;
        snprintf(DF.cursors[DF.pages++], sizeof DF.cursors[0], "%s", cursor);
        char params[160];
        ink_jw_t w;
        ink_jw_init(&w, params, sizeof params);
        ink_jw_raw(&w, "{\"cursor\":"); ink_jw_str(&w, cursor); ink_jw_raw(&w, "}");
        if (w.overflow || ink_hub_call("caps/list", params, 10000000, dir_reply, ctx) < 0) goto failed;
        return;
    }
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    memcpy(CB.dir, DF.got, sizeof DF.got[0] * (size_t)DF.n);
    CB.ndir = DF.n;
    CB.dir_epoch = DF.epoch;
    osal_mutex_unlock(CB.lock);
    ink_probe_value("cap.directory_size", DF.n);
    return;
failed:
    ink_probe_count("cap.directory_failed");
}

static void dir_fetch(void *ctx) { /* hub task: on every (re)connection */
    (void)ctx;
    if (DF.connection != ink_hub_connection()) {
        osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
        ev_sent = ev_acked;
        osal_mutex_unlock(CB.lock);
    }
    DF.connection = ink_hub_connection();
    DF.epoch = ink_hub_member_epoch();
    DF.n = DF.pages = 0;
    DF.generation++;
    if (ink_hub_call("caps/list", "{}", 10000000, dir_reply, (void *)(uintptr_t)DF.generation) < 0)
        ink_probe_count("cap.directory_failed");
}

static void event_ack(void *ctx, const char *method, const ink_json_t *j, int params) {
    (void)ctx;
    int64_t boot, upto;
    if (strcmp(method, "device/event_ack") || !ink_json_int(j, ink_json_find(j, params, "boot"), &boot) ||
        !ink_json_int(j, ink_json_find(j, params, "upto"), &upto) || boot != CB.boot_tag || upto <= 0 || upto > UINT32_MAX) return;
    uint32_t token[EVENTS_KEPT], ver[EVENTS_KEPT];
    int n = 0;
    osal_mutex_lock(CB.lock, OSAL_WAIT_FOREVER);
    if ((uint32_t)upto > ev_acked && (uint32_t)upto <= ev_sent) {
        for (int i = 0; i < EVENTS_KEPT; i++) {
            const typeof(EV[0]) *e = &EV[i];
            int64_t t = 0, v = 0;
            if (e->delivered && e->seq > ev_acked && e->seq <= (uint32_t)upto && !strcmp(e->id, "confirm.result") &&
                !strcmp(e->member, ink_member_current()->id) && ink_args_get_int(&e->args, "token", &t) &&
                ink_args_get_int(&e->args, "result_ver", &v) && t > 1 && v > 0) { token[n] = (uint32_t)t; ver[n++] = (uint32_t)v; }
        }
        ev_acked = (uint32_t)upto;
    }
    bool all = ev_acked == ev_next;
    osal_mutex_unlock(CB.lock);
    for (int i = 0; i < n; i++) ink_confirm_ack(token[i], ver[i]);
    if (all) ink_hub_events_acked();
}

static void on_caps_changed(void *ctx, const char *method, const ink_json_t *j, int params) { /* hub task */
    (void)ctx;
    (void)j;
    (void)params;
    if (!strcmp(method, "caps/changed")) dir_fetch(NULL);
}

int ink_tools_init(void) {
    if (ink_db_register(&ACTIVITY) != 0) return -1;
    /* Boot reads the small logs before interactive work can queue activity writes. */
    for (int i = 0; ink_member_at(i); i++) {
        ink_db_t *db = ink_db_open(&ACTIVITY, ink_member_at(i)->id);
        if (db) ink_db_close(db);
    }
    AL.ready = true;
    if (ink_hub_on_online(dir_fetch, NULL) || ink_hub_subscribe("caps/", on_caps_changed, NULL) || ink_hub_subscribe("device/event_ack", event_ack, NULL)) return -1;
    ink_hub_set_tick(send_events);
    return ink_hub_on_request("tools/", on_tools, NULL);
}
