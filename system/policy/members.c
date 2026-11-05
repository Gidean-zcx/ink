/*
 * Members (see ink/sys/members.h). Two stand in until something has been changed (the first boot): the owner (an adult) and
 * a child whose whitelist allows reading and the AI. From the first change on, the table in the data store is what there
 * is. Slots do not move: ink_member_current() hands out a pointer into them.
 */
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

#define PIN_ITERATIONS 3000      /* about 100 ms on the S31 by the block cost; to be set on the device (D-69) */
#define PIN_MIN 4
#define PIN_MAX 8
#define FREE_TRIES 3
#define FIRST_WAIT_S 30
#define LONGEST_WAIT_S 900
#define UNREAD_RETRY_S 10
#define IDLE_RETURN_S 120

typedef struct {
    bool used;
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
} stored_t;

/* Hashes and salts: no AI, no hub, no app reads this. */
static const ink_db_def_t TABLE = {.name = "member.table", .title = "家人名单", .scope = INK_DB_DEVICE, .schema = 1, .max_records = INK_MEMBERS_MAX};

static struct {
    slot_t s[INK_MEMBERS_MAX];
    slot_t *cur;
    bool authed;
    bool table_is_real;
    bool table_unread; /* it is there and could not be read: nobody knows who has a PIN, so as if everyone adult had one */
    int h_member, h_authed;
    ink_svc_deadline_t idle_at;
    ink_exec_job_t idle_job;
} M = {.s = {[0] = {.used = true, .m = {.id = "owner", .name = "我", .role = INK_ROLE_ADULT}},
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
    ink_state_set_str(M.h_member, cur()->m.id);
    ink_state_set_bool(M.h_authed, ink_member_space_open());
}

const ink_member_t *ink_member_current(void) { return &cur()->m; }

const ink_member_t *ink_member_at(int i) {
    int seen = 0;
    for (int k = 0; k < INK_MEMBERS_MAX; k++)
        if (M.s[k].used && seen++ == i) return &M.s[k].m;
    return NULL;
}

bool ink_member_has_pin(const char *id) {
    const slot_t *s = find(id);
    return s && (s->has_pin || (M.table_unread && s->m.role == INK_ROLE_ADULT));
}

bool ink_member_any_pin(void) {
    if (M.table_unread) return true;
    for (int k = 0; k < INK_MEMBERS_MAX; k++)
        if (M.s[k].used && M.s[k].m.role == INK_ROLE_ADULT && M.s[k].has_pin) return true;
    return false;
}

bool ink_member_space_open(void) {
    const slot_t *c = cur();
    return c->m.role == INK_ROLE_CHILD || M.authed || (!c->has_pin && !M.table_unread);
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
static void save_slot(ink_db_t *db, const slot_t *s) {
    stored_t r = {.role = (uint8_t)s->m.role, .has_pin = s->has_pin, .fails = s->fails, .not_before_s = s->not_before_s};
    snprintf(r.id, sizeof r.id, "%s", s->m.id);
    snprintf(r.name, sizeof r.name, "%s", s->m.name);
    memcpy(r.salt, s->salt, sizeof r.salt);
    memcpy(r.hash, s->hash, sizeof r.hash);
    ink_db_write_t w = {.id = record_id(s->m.id), .data = &r, .len = sizeof r, .src = INK_SRC_SYSTEM};
    ink_db_put(db, &w, NULL);
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

/* What is on the flash, into fresh: how many, 0 when there is no family yet, -1 when it cannot be told. */
static int read_table(slot_t *fresh) {
    ink_db_t *db = ink_db_open(&TABLE, NULL);
    if (!db) return -1;
    ink_db_cursor_t c = {0};
    ink_rec_t r;
    stored_t st;
    int n = 0;
    while (n < INK_MEMBERS_MAX && ink_db_next(db, &c, &r, &st, sizeof st) == 1) {
        if (r.len != sizeof st) continue;
        slot_t *s = &fresh[n++];
        s->used = true;
        snprintf(s->m.id, sizeof s->m.id, "%.15s", st.id);
        snprintf(s->m.name, sizeof s->m.name, "%.31s", st.name);
        s->m.role = (ink_role_t)st.role;
        s->has_pin = st.has_pin;
        memcpy(s->salt, st.salt, sizeof s->salt);
        memcpy(s->hash, st.hash, sizeof s->hash);
        s->fails = st.fails;
        s->not_before_s = st.not_before_s;
        set_allow(s);
    }
    memset(&st, 0, sizeof st);
    if (!n && ink_db_count(db) > 0) n = -1; /* records, and none that this can use: not an empty family */
    ink_db_close(db);
    return n;
}

static void load_table(void) {
    if (ink_db_register(&TABLE) != 0) return; /* some test programs run without the data store */
    slot_t *fresh = osal_calloc(INK_MEMBERS_MAX, sizeof *fresh, OSAL_MEM_EXTERNAL);
    int n = fresh ? read_table(fresh) : -1;
    if (n < 0) {
        M.table_unread = true;
        ink_probe_count("members.table_unread");
    } else if (n > 0) {
        memcpy(M.s, fresh, sizeof M.s);
        M.table_is_real = true;
        /* an adult first, as on the first boot */
        slot_t *first = &M.s[0];
        for (int i = INK_MEMBERS_MAX - 1; i >= 0; i--)
            if (M.s[i].used && M.s[i].m.role == INK_ROLE_ADULT) first = &M.s[i];
        M.cur = first;
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
    load_table();
    publish();
}

int ink_member_switch(const char *id) {
    slot_t *t = find(id);
    if (!t) return -1;
    if (t == cur()) return ink_member_space_open() ? 0 : -2;
    if (t->has_pin || (M.table_unread && t->m.role == INK_ROLE_ADULT)) return -2; /* needs the PIN */
    M.cur = t;
    M.authed = false;
    ink_probe_count("members.switches");
    publish();
    return 0;
}

void ink_member_test_reset(void) {
    for (int i = 0; i < INK_MEMBERS_MAX; i++)
        if (M.s[i].used && M.s[i].m.role == INK_ROLE_ADULT) {
            M.cur = &M.s[i];
            break;
        }
    M.authed = false;
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
    if (!M.authed) return;
    M.authed = false;
    ink_probe_count("members.space_closed");
    ink_state_set_bool(M.h_authed, ink_member_space_open());
}

static void idle_check(void *arg);

static void arm_idle(void) {
    int64_t s = ink_setting_get("member.idle_return_s");
    ink_svc_at(&M.idle_at, ink_power_last_activity() + (uint64_t)(s > 0 ? s : IDLE_RETURN_S) * 1000000ull, idle_check, NULL);
}

static void idle_job(void *arg) { /* exec.bg */
    (void)arg;
    if (!M.authed) return;
    int64_t s = ink_setting_get("member.idle_return_s");
    if (osal_now_us() >= ink_power_last_activity() + (uint64_t)(s > 0 ? s : IDLE_RETURN_S) * 1000000ull) close_space();
    else arm_idle();
}

static void idle_check(void *arg) { /* the timer task */
    (void)arg;
    ink_exec_post(INK_EXEC_BG, &M.idle_job, idle_job, NULL);
}

static void on_power(void *ctx, ink_power_state_t from, ink_power_state_t to, uint64_t cause_us) { /* power task */
    (void)ctx;
    (void)from;
    (void)cause_us;
    if (to == INK_PWR_LOCKED) close_space();
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
            if (M.s[i].used && M.s[i].m.role == INK_ROLE_ADULT && M.s[i].has_pin) s = &M.s[i];
    } else s = find(j->id);
    if (s) switch (j->kind) {
        case JOB_UNLOCK:
            if (!s->has_pin) r = INK_PIN_OK;
            else r = try_pin(s, j->pin, &wait_s);
            if (r == INK_PIN_OK) {
                M.cur = s;
                M.authed = s->has_pin;
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
                        s->has_pin = true;
                        if (s == cur()) M.authed = true; /* the one who just set it is here */
                    } else {
                        s->has_pin = false;
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
    char out[768];
    ink_jw_t w;
    ink_jw_init(&w, out, sizeof out);
    ink_jw_raw(&w, "[");
    const ink_member_t *m;
    for (int i = 0; (m = ink_member_at(i)) != NULL; i++) {
        ink_jw_raw(&w, i ? ",{\"id\":" : "{\"id\":");
        ink_jw_str(&w, m->id);
        ink_jw_raw(&w, ",\"name\":");
        ink_jw_str(&w, m->name);
        ink_jw_raw(&w, m->role == INK_ROLE_CHILD ? ",\"role\":\"child\"" : ",\"role\":\"adult\"");
        ink_jw_raw(&w, ink_member_has_pin(m->id) ? ",\"has_pin\":true}" : ",\"has_pin\":false}");
    }
    ink_jw_raw(&w, "]");
    ink_cap_result(c, out);
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
    slot_t *s = NULL;
    for (int i = 0; i < INK_MEMBERS_MAX && !s; i++)
        if (!M.s[i].used) s = &M.s[i];
    if (!s) {
        fail(c, INK_RC_FAILED, "the family is full");
        return;
    }
    memset(s, 0, sizeof *s);
    snprintf(s->m.id, sizeof s->m.id, "%.15s", id);
    snprintf(s->m.name, sizeof s->m.name, "%.31s", name[0] ? name : id);
    s->m.role = !strcmp(role, "adult") ? INK_ROLE_ADULT : INK_ROLE_CHILD;
    set_allow(s);
    s->used = true;
    save_all();
    ink_cap_emit("member.changed", NULL);
}

static void wipe_member_files(void *arg) { /* the storage task: their data area */
    const char *id = arg;
    char dir[INK_LAYOUT_PATH_MAX];
    if (ink_layout_member(dir, sizeof dir, id, "x") < 0) return;
    dir[strlen(dir) - 2] = 0;
    hal_dir_t *d = hal_fs_opendir(dir);
    if (!d) return;
    hal_dirent_t e;
    char path[INK_LAYOUT_PATH_MAX + 64];
    while (hal_fs_readdir(d, &e) == 1) {
        if (e.is_dir) continue;
        if (snprintf(path, sizeof path, "%s/%s", dir, e.name) < (int)sizeof path) hal_fs_remove(path);
    }
    hal_fs_closedir(d);
    hal_fs_remove(dir);
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
        if (M.s[i].used && M.s[i].m.role == INK_ROLE_ADULT) adults++;
    if (s->m.role == INK_ROLE_ADULT && adults == 1) {
        fail(c, INK_RC_FAILED, "the last adult stays");
        return;
    }
    ink_db_t *db = ink_db_open(&TABLE, NULL);
    if (db) {
        for (int i = 0; i < INK_MEMBERS_MAX; i++)
            if (M.s[i].used) save_slot(db, &M.s[i]); /* the table is real from now on */
        M.table_is_real = true;
        ink_db_del(db, record_id(id), (uint8_t)c->src);
        ink_db_close(db);
    }
    s->used = false;
    /* what was theirs, on the flash and on the card */
    ink_db_forget_member(id);
    ink_storage_run(wipe_member_files, id, true);
    int card = ink_fs_forget_member(id);
    ink_cap_emit("member.changed", NULL);
    snprintf(c->rc.say, sizeof c->rc.say, "%s", card == INK_FS_ERR_NO_CARD ? "删掉了。存储卡不在，卡上的文件还留着" : "删掉了");
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

static const ink_cap_t MEMBER_CAPS[] = {
    {.id = "member.list", .title = "家里有哪些人", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .while_locked = true, .exec = INK_EXEC_DIRECT, .run = cap_list},
    {.id = "member.add", .title = "加一位家人", .ask = "加一位家人：{name}（{role}）",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":15},\"name\":{\"type\":\"string\",\"maxLength\":31},\"role\":{\"type\":\"string\",\"enum\":[\"adult\",\"child\"]}},\"required\":[\"id\"]}",
     .effect = INK_FX_SYSTEM, .exec = INK_EXEC_BG, .run = cap_add},
    {.id = "member.remove", .title = "删除一位家人和他们的全部数据（不能撤销）", .ask = "删除家人 {id} 和他们的全部数据，不能撤销",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":15}},\"required\":[\"id\"]}",
     .effect = INK_FX_SYSTEM, .parent_pin = true, .exec = INK_EXEC_BG, .run = cap_remove},
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
    {.id = "member.idle_return_s", .title = "大人的空间闲置多久后要重新输密码（秒）", .type = INK_SET_INT, .min = 30, .max = 3600, .def = IDLE_RETURN_S},
};

int ink_members_start(void) {
    if (ink_settings_define(MEMBER_SETTINGS, 1) != 0 || ink_power_subscribe(on_power, NULL) != 0) return -1;
    return ink_cap_register(MEMBER_CAPS, (int)(sizeof MEMBER_CAPS / sizeof MEMBER_CAPS[0]));
}
