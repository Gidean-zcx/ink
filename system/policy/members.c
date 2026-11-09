/*
 * Members (see ink/sys/members.h). Two stand in until something has been changed (the first boot): the owner (an adult) and
 * a child whose whitelist allows reading and the AI. From the first change on, the table in the data store is what there
 * is. Slots do not move: ink_member_current() hands out a pointer into them.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ink/hal_device.h"
#include "ink/hal_power.h"
#include "ink/hal_storage.h"
#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/crc.h"
#include "ink/sys/data.h"
#include "ink/sys/file.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/settings.h"
#include "ink/sys/sha256.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/sys/svc.h"
#include "ink/sys/time.h"
#include "ink/sys/task.h"

#define PIN_ITERATIONS 3000      /* about 100 ms on the S31 by the block cost; to be set on the device (D-69) */
#define PIN_MIN 4
#define PIN_MAX 8
#define FREE_TRIES 3
#define FIRST_WAIT_S 30
#define LONGEST_WAIT_S 900
#define UNREAD_RETRY_S 10
#define IDLE_RETURN_S 120        /* provisional lock grace (D-95); member.idle_return_s overrides it */
#define REMOVING 0x80u /* Kept in the stored role byte without changing the record format. */

typedef struct {
    bool used;
    bool removing;
    uint16_t writers;
    uint8_t identity[16];
    ink_member_t m;
    uint8_t salt[16], hash[32];
    bool has_pin;
    uint32_t fails;
    int64_t not_before_s;
} slot_t;

typedef struct __attribute__((packed)) {
    char id[16], name[32];
    uint8_t role, has_pin;
    uint8_t salt[16], hash[32];
    uint32_t fails;
    int64_t not_before_s;
    uint8_t identity[16];
} stored_t;

/* Hashes and salts: no AI, no hub, no app reads this. */
static const ink_db_def_t TABLE = {.name = "member.table", .title = "家人名单", .scope = INK_DB_DEVICE, .schema = 1, .max_records = INK_MEMBERS_MAX};
/* An id is never used twice on a device: nothing delayed can reach a new person under an old id. */
static const ink_db_def_t RETIRED = {.name = "member.retired", .title = "删掉的家人编号", .scope = INK_DB_DEVICE, .schema = 1, .max_records = 64};

static struct {
    slot_t s[INK_MEMBERS_MAX];
    uint32_t epoch;
    slot_t *cur;
    bool authed;
    bool table_is_real;
    bool table_unread; /* it is there and could not be read: nobody knows who has a PIN, so as if everyone adult had one */
    uint64_t locked_since;
    uint64_t return_after_us;
    int h_member, h_authed;
    ink_svc_deadline_t idle_at;
    ink_exec_job_t idle_job;
} M = {.epoch = 1, .return_after_us = IDLE_RETURN_S * 1000000ull,
       .s = {[0] = {.used = true, .m = {.id = "owner", .name = "我", .role = INK_ROLE_ADULT}},
             [1] = {.used = true, .m = {.id = "kid", .name = "小朋友", .role = INK_ROLE_CHILD, .allow = {"reader.", "ai.", "system.home"}}}}};

static slot_t *cur(void) { return M.cur ? M.cur : &M.s[0]; }

static slot_t *find(const char *id) {
    for (int i = 0; i < INK_MEMBERS_MAX; i++)
        if (M.s[i].used && !strcmp(M.s[i].m.id, id)) return &M.s[i];
    return NULL;
}

static const ink_state_def_t ST_MEMBER = {.id = "member.current", .type = INK_ST_STR, .title = "现在是哪位家人"};
static const ink_state_def_t ST_AUTHED = {.id = "member.authed", .type = INK_ST_BOOL, .title = "这位家人的空间开着"};
static int g_published; /* handles start at 0: this says they are there */

static void publish(void) {
    if (!g_published) { /* the first time the state table is up */
        M.h_member = ink_state_define(&ST_MEMBER);
        M.h_authed = ink_state_define(&ST_AUTHED);
        g_published = 1;
    }
    ink_member_identity_t identity;
    ink_member_identity(&identity);
    ink_state_set_str(M.h_member, identity.member);
    ink_state_set_bool(M.h_authed, identity.authed);
}

const ink_member_t *ink_member_current(void) { return &cur()->m; }

bool ink_member_removing(const char *id) { const slot_t *s = find(id); return s && s->removing; }

bool ink_member_exists(const char *id) {
    const slot_t *s = id ? find(id) : cur();
    return s && !s->removing;
}

bool ink_member_io_begin(const char *id) {
    ink_svc_lock();
    slot_t *s = id ? find(id) : NULL;
    bool ok = s && ink_member_exists(id) && s->writers < UINT16_MAX;
    if (ok) s->writers++;
    ink_svc_unlock();
    return ok;
}

void ink_member_io_end(const char *id) {
    ink_svc_lock();
    slot_t *s = find(id);
    if (s && s->writers) s->writers--;
    ink_svc_unlock();
}

const ink_member_t *ink_member_at(int i) {
    int seen = 0;
    for (int k = 0; k < INK_MEMBERS_MAX; k++)
        if (M.s[k].used && seen++ == i) return &M.s[k].m;
    return NULL;
}
bool ink_members_configured(void) {
    ink_svc_lock();
    bool configured = M.table_is_real || M.table_unread;
    ink_svc_unlock();
    return configured;
}

bool ink_member_has_pin(const char *id) {
    const slot_t *s = find(id);
    return s && (s->has_pin || (M.table_unread && s->m.role == INK_ROLE_ADULT));
}

bool ink_member_any_pin(void) {
    if (M.table_unread) return true;
    for (int k = 0; k < INK_MEMBERS_MAX; k++)
        if (M.s[k].used && !M.s[k].removing && M.s[k].m.role == INK_ROLE_ADULT && M.s[k].has_pin) return true;
    return false;
}

static bool return_expired(uint64_t now) {
    uint64_t since = __atomic_load_n(&M.locked_since, __ATOMIC_ACQUIRE);
    uint64_t after = __atomic_load_n(&M.return_after_us, __ATOMIC_RELAXED);
    return since && now >= since && now - since >= after;
}

bool ink_member_space_open(void) {
    const slot_t *c = cur();
    return c->m.role == INK_ROLE_CHILD || (M.authed && !return_expired(osal_now_us())) || (!c->has_pin && !M.table_unread);
}

uint32_t ink_member_epoch(void) { return __atomic_load_n(&M.epoch, __ATOMIC_ACQUIRE); }

void ink_member_identity(ink_member_identity_t *out) {
    ink_svc_lock();
    snprintf(out->member, sizeof out->member, "%s", cur()->m.id);
    
    out->epoch = ink_member_epoch();
    out->authed = ink_member_space_open();
    ink_svc_unlock();
}

static void invalidate_identity(void) { /* under the service lock, before changing identity */
    __atomic_add_fetch(&M.epoch, 1, __ATOMIC_ACQ_REL);
}

/* ---- the table on the flash ---- */

static void set_allow(slot_t *s) {
    static const char *const CHILD[] = {"reader.", "ai.", "system.home"};
    memset(s->m.allow, 0, sizeof s->m.allow);
    if (s->m.role == INK_ROLE_CHILD)
        for (size_t i = 0; i < sizeof CHILD / sizeof CHILD[0]; i++) s->m.allow[i] = CHILD[i];
}

static uint32_t record_id(const char *id) {
    uint32_t r = ink_crc32(0, id, strlen(id)) & 0x7fffffff;
    return r ? r : 1;
}

/* What is in memory, written: from the first change on the table is what there is. */
static int save_slot(ink_db_t *db, const slot_t *s) {
    stored_t r = {.role = (uint8_t)s->m.role | (s->removing ? REMOVING : 0), .has_pin = s->has_pin, .fails = s->fails, .not_before_s = s->not_before_s};
    snprintf(r.id, sizeof r.id, "%s", s->m.id);
    snprintf(r.name, sizeof r.name, "%s", s->m.name);
    memcpy(r.salt, s->salt, sizeof r.salt);
    memcpy(r.hash, s->hash, sizeof r.hash);
    memcpy(r.identity, s->identity, sizeof r.identity);
    ink_db_write_t w = {.id = record_id(s->m.id), .data = &r, .len = sizeof r, .src = INK_SRC_SYSTEM};
    return ink_db_put(db, &w, NULL);
}

static void save_all(void) {
    if (M.table_unread) return; /* what is here are the defaults: they must not bury what is there */
    ink_db_t *db = ink_db_open(&TABLE, NULL);
    if (!db) return;
    for (int i = 0; i < INK_MEMBERS_MAX; i++)
        if (M.s[i].used) save_slot(db, &M.s[i]);
    M.table_is_real = true;
    ink_db_close(db);
}

int ink_member_durable_id(const char *id, uint8_t out[16]) {
    slot_t *s = find(id);
    if (!out || !s || !ink_member_exists(id) || M.table_unread ||
        !ink_exec_is_current(INK_EXEC_BG)) return -1;
    ink_db_t *db = ink_db_open(&TABLE, NULL);
    if (!db) return -1;
    uint8_t used = 0;
    for (unsigned i = 0; i < sizeof s->identity; i++) used |= s->identity[i];
    int rc = 0;
    if (!used) {
        /* The persisted table version also prevents deterministic simulator RNG reuse after reboot. */
        uint32_t serial = ink_db_last_ver(db);
        if (serial == UINT32_MAX) rc = -1;
        else {
            serial++;
            hal_random_fill(s->identity, sizeof s->identity);
            for (unsigned i = 0; i < 4; i++) s->identity[i] = (uint8_t)(serial >> (i * 8));
            rc = save_slot(db, s);
            for (int i = 0; !rc && i < INK_MEMBERS_MAX; i++)
                if (M.s[i].used && &M.s[i] != s) rc = save_slot(db, &M.s[i]);
        }
    }
    /* Flash writes wait for the panel, and right after a member switch it is drawing a whole new screen. */
    if (!rc && !used) rc = ink_db_wait(db, ink_db_last_ver(db), 5000000);
    if (!rc) { memcpy(out, s->identity, 16); M.table_is_real = true; }
    else if (!used) memset(s->identity, 0, sizeof s->identity);
    ink_db_close(db);
    return rc;
}

/* What is on the flash, into fresh: how many, 0 when there is no family yet, -1 when it cannot be told. */
static int read_table(slot_t *fresh) {
    ink_db_t *db = ink_db_open(&TABLE, NULL);
    if (!db) return -1;
    ink_db_cursor_t c = {0};
    ink_rec_t r;
    stored_t st = {0};
    int n = 0;
    while (n < INK_MEMBERS_MAX && ink_db_next(db, &c, &r, &st, sizeof st) == 1) {
        if (r.len != sizeof st && r.len != offsetof(stored_t, identity)) continue;
        slot_t *s = &fresh[n++];
        s->used = true;
        snprintf(s->m.id, sizeof s->m.id, "%.15s", st.id);
        snprintf(s->m.name, sizeof s->m.name, "%.31s", st.name);
        s->m.role = (ink_role_t)(st.role & ~REMOVING);
        s->removing = (st.role & REMOVING) != 0;
        s->has_pin = st.has_pin;
        memcpy(s->salt, st.salt, sizeof s->salt);
        memcpy(s->hash, st.hash, sizeof s->hash);
        s->fails = st.fails;
        s->not_before_s = st.not_before_s;
        if (r.len == sizeof st) memcpy(s->identity, st.identity, sizeof s->identity);
        set_allow(s);
    }
    memset(&st, 0, sizeof st);
    if (!n && ink_db_count(db) > 0) n = -1; /* records, and none that this can use: not an empty family */
    ink_db_close(db);
    return n;
}

static void load_table(void) {
    if (ink_db_register(&TABLE) != 0) return; /* some test programs run without the data store */
    ink_db_register(&RETIRED);
    slot_t *fresh = osal_calloc(INK_MEMBERS_MAX, sizeof *fresh, OSAL_MEM_EXTERNAL);
    int n = fresh ? read_table(fresh) : -1;
    if (n < 0) {
        ink_svc_lock();
        invalidate_identity();
        M.table_unread = true;
        ink_svc_unlock();
        ink_probe_count("members.table_unread");
    } else if (n > 0) {
        ink_svc_lock();
        invalidate_identity();
        memcpy(M.s, fresh, sizeof M.s);
        M.table_is_real = true;
        /* an adult first, as on the first boot */
        slot_t *first = &M.s[0];
        for (int i = INK_MEMBERS_MAX - 1; i >= 0; i--)
            if (M.s[i].used && !M.s[i].removing && M.s[i].m.role == INK_ROLE_ADULT) first = &M.s[i];
        M.cur = first;
        ink_svc_unlock();
    }
    if (fresh) memset(fresh, 0, INK_MEMBERS_MAX * sizeof *fresh);
    osal_free(fresh);
}

/* It could not be read at the boot; can it now? Nothing here changes while it looks. */
static bool table_back(void) {
    slot_t *fresh = osal_calloc(INK_MEMBERS_MAX, sizeof *fresh, OSAL_MEM_EXTERNAL);
    bool back = fresh && read_table(fresh) >= 0;
    if (fresh) memset(fresh, 0, INK_MEMBERS_MAX * sizeof *fresh);
    osal_free(fresh);
    return back;
}

void ink_members_init(void) {
    if (ink_svc_init() != 0) osal_panic("members: service lock unavailable");
    load_table();
    publish();
}

int ink_member_switch(const char *id) {
    ink_svc_lock();
    slot_t *t = find(id);
    int r = !t || t->removing ? -1 : t == cur() ? (ink_member_space_open() ? 0 : -2) :
            t->has_pin || (M.table_unread && t->m.role == INK_ROLE_ADULT) ? -2 : 1;
    if (r != 1) { ink_svc_unlock(); return r; }
    invalidate_identity();
    M.cur = t;
    M.authed = false;
    ink_svc_unlock();
    ink_probe_count("members.switches");
    publish();
    return 0;
}

void ink_member_test_reset(void) {
    ink_svc_lock();
    invalidate_identity();
    for (int i = 0; i < INK_MEMBERS_MAX; i++)
        if (M.s[i].used && !M.s[i].removing && M.s[i].m.role == INK_ROLE_ADULT) {
            M.cur = &M.s[i];
            break;
        }
    M.authed = false;
    ink_svc_unlock();
    publish();
}

bool ink_member_allows(const char *action_id, bool child_ok) {
    const ink_member_t *m = ink_member_current();
    if (m->role == INK_ROLE_ADULT) return true;
    if (child_ok) return true;
    for (int i = 0; i < 8 && m->allow[i]; i++)
        if (!strncmp(action_id, m->allow[i], strlen(m->allow[i]))) return true;
    return false;
}

/* ---- going idle, locking ---- */

static void close_space(void) {
    ink_svc_lock();
    if (!M.authed) { ink_svc_unlock(); return; }
    invalidate_identity();
    M.authed = false;
    ink_svc_unlock();
    ink_probe_count("members.space_closed");
    ink_state_set_bool(M.h_authed, ink_member_space_open());
}

static void idle_check(void *arg);

static void arm_idle(void) {
    uint64_t since = __atomic_load_n(&M.locked_since, __ATOMIC_ACQUIRE);
    uint64_t after = __atomic_load_n(&M.return_after_us, __ATOMIC_RELAXED);
    if (M.authed && since) ink_svc_at(&M.idle_at, since + after, idle_check, NULL);
    else ink_svc_cancel(&M.idle_at);
}

static void idle_job(void *arg) { /* exec.bg */
    (void)arg;
    if (!M.authed || !__atomic_load_n(&M.locked_since, __ATOMIC_ACQUIRE)) return;
    if (return_expired(osal_now_us())) close_space();
    else arm_idle();
}

static void idle_check(void *arg) { /* the timer task */
    (void)arg;
    ink_exec_post(INK_EXEC_BG, &M.idle_job, idle_job, NULL);
}

static void on_power(void *ctx, ink_power_state_t from, ink_power_state_t to, uint64_t cause_us) { /* power task */
    (void)ctx;
    if (to == INK_PWR_LOCKED) {
        __atomic_store_n(&M.locked_since, osal_now_us(), __ATOMIC_RELEASE);
        arm_idle();
    } else if (from == INK_PWR_LOCKED) {
        /* A busy background executor must not let a wake key bypass the deadline. */
        if (return_expired(cause_us)) close_space();
        __atomic_store_n(&M.locked_since, 0, __ATOMIC_RELEASE);
        ink_svc_cancel(&M.idle_at);
    }
}

static void apply_return_after(void *ctx, int64_t seconds) {
    (void)ctx;
    ink_svc_lock();
    bool was_open = ink_member_space_open();
    __atomic_store_n(&M.return_after_us, (uint64_t)seconds * 1000000ull, __ATOMIC_RELAXED);
    if (was_open != ink_member_space_open()) invalidate_identity();
    ink_svc_unlock();
    arm_idle();
}

/* ---- the PIN ---- */

static bool pin_form_ok(const char *pin) {
    size_t n = strlen(pin);
    if (n < PIN_MIN || n > PIN_MAX) return false;
    for (size_t i = 0; i < n; i++)
        if (pin[i] < '0' || pin[i] > '9') return false;
    return true;
}

/* How long until the next try counts; 0: now. Also when the clock has gone back since it was set. */
static uint32_t wait_left(slot_t *s) {
    int64_t now = ink_time_now_s();
    if (s->not_before_s - now > LONGEST_WAIT_S) s->not_before_s = now + LONGEST_WAIT_S;
    return s->not_before_s > now ? (uint32_t)(s->not_before_s - now) : 0;
}

static bool pin_matches(const slot_t *s, const char *pin) {
    uint8_t got[32];
    ink_pbkdf2_sha256(pin, strlen(pin), s->salt, sizeof s->salt, PIN_ITERATIONS, got);
    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= got[i] ^ s->hash[i];
    return diff == 0;
}

/* One try: it counts whatever happens to the device next. */
static int try_pin(slot_t *s, const char *pin, uint32_t *wait_s) {
    *wait_s = wait_left(s);
    if (*wait_s) {
        ink_probe_count("members.pin_too_soon");
        return INK_PIN_WAIT;
    }
    if (pin_matches(s, pin)) {
        if (s->fails || s->not_before_s) {
            s->fails = 0;
            s->not_before_s = 0;
            save_all();
        }
        return INK_PIN_OK;
    }
    s->fails++;
    ink_probe_count("members.pin_wrong");
    if (s->fails >= FREE_TRIES) {
        uint32_t shift = s->fails - FREE_TRIES;
        uint32_t w = shift >= 5 ? LONGEST_WAIT_S : (uint32_t)FIRST_WAIT_S << shift;
        *wait_s = w > LONGEST_WAIT_S ? LONGEST_WAIT_S : w;
        s->not_before_s = ink_time_now_s() + *wait_s;
    }
    save_all();
    return INK_PIN_WRONG;
}

typedef enum { JOB_UNLOCK, JOB_SET, JOB_CHECK_PARENT } job_kind_t;
typedef struct {
    ink_exec_job_t job;
    job_kind_t kind;
    char id[16];
    char pin[PIN_MAX + 1], new_pin[PIN_MAX + 1];
    ink_pin_done_fn done;
    void *ctx;
} pin_job_t;

static void wipe(pin_job_t *j) {
    volatile char *p = (volatile char *)j;
    for (size_t i = 0; i < sizeof *j; i++) p[i] = 0;
}

static void pin_job(void *arg) { /* exec.bg: slow by design */
    pin_job_t *j = arg;
    int r = INK_PIN_NO_MEMBER;
    uint32_t wait_s = 0;
    slot_t *s = NULL;
    if (M.table_unread) { /* maybe it was a moment's trouble. Then start over: everything up to now started from the defaults */
        if (table_back()) {
            ink_probe_count("members.table_back");
            ink_db_flush_all();
            for (int i = 0; i < 100 && (!ink_db_idle() || ink_storage_pending()); i++) osal_task_delay_us(50000);
            hal_power_reboot();
        }
        if (j->done) j->done(j->ctx, INK_PIN_WAIT, UNREAD_RETRY_S);
        wipe(j);
        osal_free(j);
        return;
    }
    if (j->kind == JOB_CHECK_PARENT) { /* the one using the device if an adult with a PIN, else the first who has one */
        if (cur()->m.role == INK_ROLE_ADULT && cur()->has_pin) s = cur();
        for (int i = 0; i < INK_MEMBERS_MAX && !s; i++)
            if (M.s[i].used && !M.s[i].removing && M.s[i].m.role == INK_ROLE_ADULT && M.s[i].has_pin) s = &M.s[i];
    } else s = find(j->id);
    if (s && !s->removing && (j->kind == JOB_CHECK_PARENT || ink_member_exists(j->id))) switch (j->kind) {
        case JOB_UNLOCK:
            if (!s->has_pin) r = INK_PIN_OK;
            else r = try_pin(s, j->pin, &wait_s);
            if (r == INK_PIN_OK) {
                uint64_t locked_since = ink_power_state() == INK_PWR_LOCKED ? osal_now_us() : 0;
                ink_svc_lock();
                /* the same person unlocking an open space changes nothing */
                if (M.cur != s || M.authed != s->has_pin || !ink_member_space_open()) invalidate_identity();
                M.cur = s;
                M.authed = s->has_pin;
                __atomic_store_n(&M.locked_since, locked_since, __ATOMIC_RELEASE);
                ink_svc_unlock();
                ink_power_activity(osal_now_us()); /* the person is here: however long the slow part took */
                publish();
                if (M.authed) arm_idle();
                ink_probe_count("members.unlocked");
            }
            break;
        case JOB_SET:
            if (j->new_pin[0] && !pin_form_ok(j->new_pin)) r = INK_PIN_BAD_FORM;
            else {
                r = s->has_pin ? try_pin(s, j->pin, &wait_s) : INK_PIN_OK;
                if (r == INK_PIN_OK) {
                    if (j->new_pin[0]) {
                        hal_random_fill(s->salt, sizeof s->salt);
                        ink_pbkdf2_sha256(j->new_pin, strlen(j->new_pin), s->salt, sizeof s->salt, PIN_ITERATIONS, s->hash);
                        ink_svc_lock();
                        if (s == cur()) invalidate_identity();
                        s->has_pin = true;
                        if (s == cur()) M.authed = true; /* the one who just set it is here */
                        ink_svc_unlock();
                    } else {
                        ink_svc_lock();
                        if (s == cur()) invalidate_identity();
                        s->has_pin = false;
                        ink_svc_unlock();
                        memset(s->hash, 0, sizeof s->hash);
                    }
                    s->fails = 0;
                    s->not_before_s = 0;
                    save_all();
                    publish();
                    if (M.authed) arm_idle();
                    ink_probe_count("members.pin_set");
                }
            }
            break;
        case JOB_CHECK_PARENT: r = try_pin(s, j->pin, &wait_s); break;
        }
    ink_pin_done_fn done = j->done;
    void *ctx = j->ctx;
    wipe(j);
    osal_free(j);
    if (done) done(ctx, r, wait_s);
}

static int start_job(job_kind_t kind, const char *id, const char *pin, const char *new_pin, ink_pin_done_fn done, void *ctx) {
    if ((pin && strlen(pin) > PIN_MAX) || (new_pin && strlen(new_pin) > PIN_MAX)) return -1;
    pin_job_t *j = osal_calloc(1, sizeof *j, OSAL_MEM_EXTERNAL);
    if (!j) return -1;
    j->kind = kind;
    
    snprintf(j->id, sizeof j->id, "%s", id ? id : "");
    snprintf(j->pin, sizeof j->pin, "%s", pin ? pin : "");
    snprintf(j->new_pin, sizeof j->new_pin, "%s", new_pin ? new_pin : "");
    j->done = done;
    j->ctx = ctx;
    if (ink_exec_post(INK_EXEC_BG, &j->job, pin_job, j) != 0) {
        wipe(j);
        osal_free(j);
        return -1;
    }
    return 0;
}

int ink_member_unlock(const char *id, const char *pin, ink_pin_done_fn done, void *ctx) { return start_job(JOB_UNLOCK, id, pin, NULL, done, ctx); }
int ink_member_set_pin(const char *id, const char *old_pin, const char *new_pin, ink_pin_done_fn done, void *ctx) { return start_job(JOB_SET, id, old_pin, new_pin, done, ctx); }
int ink_member_check_parent(const char *pin, ink_pin_done_fn done, void *ctx) { return start_job(JOB_CHECK_PARENT, NULL, pin, NULL, done, ctx); }

/* ---- on the bus ---- */

static void fail(ink_call_t *c, ink_rc_t st, const char *why) {
    c->rc.status = st;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
}

static void cap_list(ink_call_t *c) {
    char *out = osal_malloc(1536, OSAL_MEM_EXTERNAL);
    if (!out) { fail(c, INK_RC_FAILED, "out of memory"); return; }
    ink_jw_t w;
    ink_jw_init(&w, out, 1536);
    ink_jw_raw(&w, "[");
    const ink_member_t *m;
    for (int i = 0; (m = ink_member_at(i)) != NULL; i++) {
        ink_jw_raw(&w, i ? ",{\"id\":" : "{\"id\":");
        ink_jw_str(&w, m->id);
        ink_jw_raw(&w, ",\"name\":");
        ink_jw_str(&w, m->name);
        ink_jw_raw(&w, m->role == INK_ROLE_CHILD ? ",\"role\":\"child\"" : ",\"role\":\"adult\"");
        ink_jw_raw(&w, ink_member_has_pin(m->id) ? ",\"has_pin\":true" : ",\"has_pin\":false");
        ink_jw_raw(&w, find(m->id)->removing ? ",\"removing\":true}" : "}");
    }
    ink_jw_raw(&w, "]");
    if (w.overflow) fail(c, INK_RC_FAILED, "member list too long");
    else ink_cap_result(c, out);
    osal_free(out);
}

static bool id_retired(const char *id) {
    ink_db_t *db = ink_db_open(&RETIRED, NULL);
    if (!db) return false;
    ink_rec_t r;
    bool used = ink_db_get(db, record_id(id), &r, NULL, 0) == INK_DB_OK;
    ink_db_close(db);
    return used;
}

static void retire_id(const char *id) {
    ink_db_t *db = ink_db_open(&RETIRED, NULL);
    if (!db) return;
    ink_db_write_t w = {.id = record_id(id), .data = id, .len = strlen(id), .src = INK_SRC_SYSTEM};
    ink_db_put(db, &w, NULL);
    ink_db_flush(db);
    ink_db_close(db);
}

static void cap_add(ink_call_t *c) { /* exec.bg */
    char id[32], name[32] = "", role[8] = "child";
    ink_args_get_str(&c->args, "id", id, sizeof id);
    ink_args_get_str(&c->args, "name", name, sizeof name);
    ink_args_get_str(&c->args, "role", role, sizeof role);
    if (!ink_layout_member_ok(id)) {
        fail(c, INK_RC_INVALID, "not a usable id");
        return;
    }
    if (find(id)) {
        fail(c, INK_RC_FAILED, "already there");
        return;
    }
    if (id_retired(id)) {
        fail(c, INK_RC_FAILED, "that id belonged to someone who was removed");
        return;
    }
    slot_t *s = NULL;
    for (int i = 0; i < INK_MEMBERS_MAX && !s; i++)
        if (!M.s[i].used) s = &M.s[i];
    if (!s) {
        fail(c, INK_RC_FAILED, "the family is full");
        return;
    }
    ink_svc_lock();
    memset(s, 0, sizeof *s);
    snprintf(s->m.id, sizeof s->m.id, "%.15s", id);
    snprintf(s->m.name, sizeof s->m.name, "%.31s", name[0] ? name : id);
    s->m.role = !strcmp(role, "adult") ? INK_ROLE_ADULT : INK_ROLE_CHILD;
    set_allow(s);
    s->used = true;
    
    ink_svc_unlock();
    save_all();
    ink_cap_emit("member.changed", NULL);
}

typedef struct { const char *id; int result; } member_wipe_t;

static void wipe_member_files(void *arg) { /* the storage task: their flat data area */
    member_wipe_t *w = arg;
    char dir[INK_LAYOUT_PATH_MAX];
    if (ink_layout_member(dir, sizeof dir, w->id, "x") < 0) return;
    dir[strlen(dir) - 2] = 0;
    if (ink_storage_forget_member(w->id) != 0) return;
    hal_dir_t *d = hal_fs_opendir(dir);
    if (!d && hal_fs_size(dir) < 0 && hal_fs_mkdir(dir) == 0) d = hal_fs_opendir(dir);
    if (!d) return;
    hal_dirent_t e;
    char path[INK_LAYOUT_PATH_MAX + 64];
    int more = 0;
    bool ok = true;
    while (ok && (more = hal_fs_readdir(d, &e)) == 1) {
        if (!strcmp(e.name, ".") || !strcmp(e.name, "..")) continue;
        ok = !e.is_dir && snprintf(path, sizeof path, "%s/%s", dir, e.name) < (int)sizeof path && hal_fs_remove(path) == 0;
    }
    hal_fs_closedir(d);
    if (ok && more == 0 && hal_fs_remove(dir) == 0) w->result = 0;
}

static void cap_remove(ink_call_t *c) { /* exec.bg */
    char id[16];
    ink_args_get_str(&c->args, "id", id, sizeof id);
    slot_t *s = find(id);
    if (!s) {
        fail(c, INK_RC_INVALID, "no such member");
        return;
    }
    if (s == cur()) {
        fail(c, INK_RC_FAILED, "they are using the device");
        return;
    }
    if (ink_usb_disk_active()) { /* their files cannot be reached: whoever gets the name next would find them */
        fail(c, INK_RC_FAILED, "the card is lent to a computer");
        return;
    }
    int adults = 0;
    for (int i = 0; i < INK_MEMBERS_MAX; i++)
        if (M.s[i].used && !M.s[i].removing && M.s[i].m.role == INK_ROLE_ADULT) adults++;
    if (!s->removing && s->m.role == INK_ROLE_ADULT && adults == 1) {
        fail(c, INK_RC_FAILED, "the last adult stays");
        return;
    }
    ink_db_t *db = ink_db_open(&TABLE, NULL);
    bool saved = db != NULL;
    if (!db) ink_probe_count("members.cleanup_table_unavailable");
    ink_svc_lock();
    s->removing = true; /* The id stays reserved through failures and restarts. */
    ink_svc_unlock();
    if (saved) {
        for (int i = 0; i < INK_MEMBERS_MAX; i++)
            if (M.s[i].used) {
                int r = save_slot(db, &M.s[i]);
                if (r != INK_DB_OK) { ink_probe_value("members.cleanup_put_error", r); saved = false; }
            }
        if (saved) {
            saved = ink_db_wait(db, ink_db_last_ver(db), (int64_t)ink_cap_time_left_ms(c) * 1000) == 0;
            if (!saved) ink_probe_count("members.cleanup_state_timeout");
        }
        M.table_is_real = true;
    }
    if (!saved) ink_probe_count("members.cleanup_state_failed");
    ink_svc_lock();
    bool writers_idle = s->writers == 0;
    ink_svc_unlock();
    if (!writers_idle) ink_probe_count("members.cleanup_writer_busy");
    bool cleared = saved && writers_idle && ink_time_forget_member(id, (int64_t)ink_cap_time_left_ms(c) * 1000) == 0;
    if (cleared) ink_task_forget_member(id);
    cleared = cleared && ink_db_try_forget_member(id) == 0;
    if (saved && !cleared) ink_probe_count("members.cleanup_cache_busy");
    member_wipe_t wipe = {.id = id, .result = -1};
    if (cleared) ink_storage_run(wipe_member_files, &wipe, true);
    if (cleared && wipe.result != 0) ink_probe_count("members.cleanup_flash_failed");
    cleared = cleared && wipe.result == 0;
    if (cleared && ink_fs_forget_member(id) != INK_FS_OK) {
        ink_probe_count("members.cleanup_card_failed");
        cleared = false;
    }
    if (cleared) cleared = ink_db_del(db, record_id(id), (uint8_t)c->src) == INK_DB_OK &&
                           ink_db_wait(db, ink_db_last_ver(db), (int64_t)ink_cap_time_left_ms(c) * 1000) == 0;
    if (db) ink_db_close(db);
    if (!cleared) {
        fail(c, INK_RC_FAILED, "member cleanup incomplete; retry removal with the card inserted");
        snprintf(c->rc.say, sizeof c->rc.say, "还没删完，请插好存储卡后重试；这位家人的编号暂时不能使用");
        ink_cap_emit("member.changed", NULL);
        return;
    }
    retire_id(id);
    ink_svc_lock();
    s->used = false;
    ink_svc_unlock();
    ink_cap_emit("member.changed", NULL);
    snprintf(c->rc.say, sizeof c->rc.say, "删掉了");
}

/* What only happens on the device: the answer is told when the slow part is over. */
static void pin_call_done(void *ctx, int result, uint32_t wait_s) {
    ink_call_t *c = ctx;
    if (result == INK_PIN_WAIT) snprintf(c->rc.detail, sizeof c->rc.detail, "wait %u s", (unsigned)wait_s);
    else if (result == INK_PIN_WRONG) snprintf(c->rc.detail, sizeof c->rc.detail, "wrong PIN");
    else if (result == INK_PIN_BAD_FORM) snprintf(c->rc.detail, sizeof c->rc.detail, "4 to 8 digits");
    else if (result == INK_PIN_NO_MEMBER) snprintf(c->rc.detail, sizeof c->rc.detail, "no such member");
    if (result != INK_PIN_OK) c->rc.status = result == INK_PIN_BAD_FORM || result == INK_PIN_NO_MEMBER ? INK_RC_INVALID : INK_RC_FAILED;
    ink_cap_done(c);
}

static void cap_unlock(ink_call_t *c) {
    char id[16], pin[PIN_MAX + 2];
    ink_args_get_str(&c->args, "id", id, sizeof id);
    ink_args_get_str(&c->args, "pin", pin, sizeof pin);
    ink_cap_defer(c);
    if (ink_member_unlock(id, pin, pin_call_done, c) != 0) {
        fail(c, INK_RC_FAILED, "could not start");
        ink_cap_done(c);
    }
    memset(pin, 0, sizeof pin);
}

static void cap_set_pin(ink_call_t *c) {
    char id[16], old_pin[PIN_MAX + 2] = "", new_pin[PIN_MAX + 2] = "";
    ink_args_get_str(&c->args, "id", id, sizeof id);
    ink_args_get_str(&c->args, "old_pin", old_pin, sizeof old_pin);
    ink_args_get_str(&c->args, "new_pin", new_pin, sizeof new_pin);
    ink_cap_defer(c);
    if (ink_member_set_pin(id, old_pin, new_pin, pin_call_done, c) != 0) {
        fail(c, INK_RC_FAILED, "could not start");
        ink_cap_done(c);
    }
    memset(old_pin, 0, sizeof old_pin);
    memset(new_pin, 0, sizeof new_pin);
}

static void cap_rename(ink_call_t *c) {
    char id[16], name[32];
    ink_args_get_str(&c->args, "id", id, sizeof id); ink_args_get_str(&c->args, "name", name, sizeof name);
    slot_t *s = find(id);
    if (!s || s->removing || !name[0]) { fail(c, INK_RC_INVALID, "unknown member or empty name"); return; }
    ink_svc_lock(); snprintf(s->m.name, sizeof s->m.name, "%s", name); ink_svc_unlock();
    save_all(); ink_cap_emit("member.changed", NULL);
}
static const ink_cap_t MEMBER_CAPS[] = {
    {.id = "member.rename", .title = "修改家人的称呼", .effect = INK_FX_LOCAL_WRITE, .exec = INK_EXEC_BG,
        .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":15},\"name\":{\"type\":\"string\",\"minLength\":1,\"maxLength\":31}},\"required\":[\"id\",\"name\"]}", .run = cap_rename},

    {.id = "member.list", .title = "家里有哪些人", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .while_locked = true, .exec = INK_EXEC_DIRECT, .run = cap_list},
    {.id = "member.add", .title = "加一位家人", .ask = "加一位家人：{name}（{role}）",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":15},\"name\":{\"type\":\"string\",\"maxLength\":31},\"role\":{\"type\":\"string\",\"enum\":[\"adult\",\"child\"]}},\"required\":[\"id\"]}",
     .effect = INK_FX_SYSTEM, .ask_by_hand = true, .parent_pin = true, .exec = INK_EXEC_BG, .run = cap_add},
    {.id = "member.remove", .title = "删除一位家人和他们的全部数据（不能撤销）", .ask = "删除家人 {id} 和他们的全部数据，不能撤销",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":15}},\"required\":[\"id\"]}",
     .effect = INK_FX_SYSTEM, .ask_by_hand = true, .parent_pin = true, .exec = INK_EXEC_BG, .run = cap_remove},
    /* a PIN goes through no one's hands but the person's */
    {.id = "member.unlock", .title = "输密码进入家人的空间（只能在设备上）",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":15},\"pin\":{\"type\":\"string\",\"maxLength\":8}},\"required\":[\"id\",\"pin\"]}",
     .effect = INK_FX_NAVIGATE, .child_ok = true, .while_locked = true, .expose = INK_EXPOSE_OWNER, .exec = INK_EXEC_DIRECT, .run = cap_unlock},
    {.id = "member.set_pin", .title = "设置或修改密码（只能在设备上）",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":15},\"old_pin\":{\"type\":\"string\",\"maxLength\":8},\"new_pin\":{\"type\":\"string\",\"maxLength\":8}},\"required\":[\"id\",\"new_pin\"]}",
     .effect = INK_FX_LOCAL_WRITE, .expose = INK_EXPOSE_OWNER, .exec = INK_EXEC_DIRECT, .run = cap_set_pin},
    {.id = "member.changed", .title = "家人有变化（加了、删了）", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

static const ink_setting_def_t MEMBER_SETTINGS[] = {
    {.id = "member.idle_return_s", .title = "大人锁屏多久后要重新输密码（秒）", .type = INK_SET_INT, .min = 30, .max = 3600, .def = IDLE_RETURN_S, .ask_by_hand = true,
     .apply = apply_return_after, .apply_exec = INK_EXEC_DIRECT},
};

int ink_members_start(void) {
    if (ink_settings_define(MEMBER_SETTINGS, 1) != 0 || ink_power_subscribe(on_power, NULL) != 0) return -1;
    apply_return_after(NULL, ink_setting_get("member.idle_return_s"));
    return ink_cap_register(MEMBER_CAPS, (int)(sizeof MEMBER_CAPS / sizeof MEMBER_CAPS[0]));
}
