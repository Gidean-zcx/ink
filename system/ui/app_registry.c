/*
 * The app directory and lifecycle (see ink/sys/appreg.h). The foreground follows the UI's activity stack and the lock:
 * whatever puts a page on top, a button, the AI or a key, the same transitions happen, before the page is built. The
 * table changes in the UI task, under the shared service lock so that other tasks can read it; what apps are told is
 * told outside the lock.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/appreg.h"
#include "ink/sys/cap.h"
#include "ink/sys/exec.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/power.h"
#include "ink/sys/res.h"
#include "ink/sys/state.h"
#include "ink/sys/storage.h"
#include "ink/sys/svc.h"
#include "ink/sys/ui.h"

#define MAX_APPS 16
#define FREEZE_AFTER_US 10000000ull
#define LIST_CAP 2048

typedef struct {
    const ink_app_def_t *def;
    ink_app_state_t state;
    uint32_t bg;       /* background work going on */
    uint64_t left_at;  /* when it left the foreground */
    uint64_t last_use;
} app_t;

static struct {
    bool ready;
    app_t a[MAX_APPS];
    int n;
    int fg;            /* in the foreground: index, or -1 */
    char top[32];      /* the app of the page on top: "" for a page of nobody's */
    bool locked;
    uint32_t generation;
    char member[INK_MEMBER_ID_MAX]; /* whose data the running apps are using */
    int h_foreground, h_cards_rev;
    int cards_rev;
    ink_svc_deadline_t freeze_at;
    ink_exec_job_t freeze_job, power_job, pressure_job, member_job;
} AP INK_PSRAM_BSS;

const char *ink_app_state_name(ink_app_state_t s) {
    static const char *const N[] = {"stopped", "foreground", "background", "frozen"};
    return (unsigned)s < 4 ? N[s] : "?";
}

static app_t *find(const char *id) {
    for (int i = 0; i < AP.n; i++)
        if (!strcmp(AP.a[i].def->id, id)) return &AP.a[i];
    return NULL;
}

/* The state changes under the lock, and it is told. UI task. */
static void set_state(app_t *a, ink_app_state_t s) {
    ink_svc_lock();
    a->state = s;
    ink_svc_unlock();
    ink_args_t *args = osal_malloc(sizeof *args, OSAL_MEM_EXTERNAL);
    if (args) {
        ink_args_init(args);
        ink_args_str(args, "id", a->def->id);
        ink_args_str(args, "state", ink_app_state_name(s));
        ink_cap_emit("app.state", args);
        osal_free(args);
    }
}

static int state_path(const app_t *a, char *out, size_t cap) {
    char name[48];
    snprintf(name, sizeof name, "%s.state", a->def->id);
    return ink_layout_member(out, cap, AP.member, name);
}

/* What it wants kept goes to the flash in the background; nothing kept, nothing left. A full queue drains in moments: it
 * waits that long. False: it could not be handed over, and what is in the app is the only copy. */
#define SAVE_TRIES 40
#define SAVE_WAIT_US 10000
static bool save(app_t *a) {
    if (!AP.generation || AP.generation != ink_member_generation(AP.member)) return true;
    if (!a->def->ops || !a->def->ops->save_state) return true;
    char path[INK_LAYOUT_PATH_MAX];
    void *buf = osal_malloc(INK_APP_STATE_MAX, OSAL_MEM_EXTERNAL);
    if (!buf || state_path(a, path, sizeof path) < 0) {
        osal_free(buf);
        ink_probe_count("app.state_not_saved");
        return false;
    }
    int n = a->def->ops->save_state(buf, INK_APP_STATE_MAX);
    int r = -1;
    for (int i = 0; i < SAVE_TRIES && r != 0; i++) {
        r = n > 0 && n <= INK_APP_STATE_MAX ? ink_storage_write_member_atomic(path, buf, (size_t)n, AP.generation) :
                                            ink_storage_remove_member(path, AP.generation, NULL, NULL);
        if (AP.generation != ink_member_generation(AP.member)) { r = 0; break; }
        if (r != 0) osal_task_delay_us(SAVE_WAIT_US);
    }
    osal_free(buf);
    if (r != 0) ink_probe_count("app.state_not_saved");
    return r == 0;
}

static void restore(app_t *a) {
    if (!a->def->ops || !a->def->ops->restore_state) return;
    char path[INK_LAYOUT_PATH_MAX];
    void *buf = osal_malloc(INK_APP_STATE_MAX, OSAL_MEM_EXTERNAL);
    if (!buf || state_path(a, path, sizeof path) < 0) {
        osal_free(buf);
        return;
    }
    int n = ink_storage_read(path, buf, INK_APP_STATE_MAX); /* sees what is still queued for it */
    if (n > 0) a->def->ops->restore_state(buf, (size_t)n);
    osal_free(buf);
}

static void freeze(app_t *a) {
    if (!save(a)) { /* the only copy of what it keeps is in it: it stays as it is, and is tried again after the same while */
        a->left_at = osal_now_us();
        return;
    }
    if (a->def->ops && a->def->ops->on_freeze) a->def->ops->on_freeze();
    set_state(a, INK_APP_FROZEN);
    ink_probe_count("app.frozen");
}

/* It has saved. */
static void stop_saved(app_t *a) {
    if (a->state == INK_APP_STOPPED) return;
    if (a->def->ops && a->def->ops->on_stop) a->def->ops->on_stop();
    ink_svc_lock();
    a->bg = 0;
    ink_svc_unlock();
    set_state(a, INK_APP_STOPPED);
    ink_probe_count("app.stopped");
}

/* From the background or frozen (what was frozen has saved already). When memory says so it goes, saved or not. */
static void stop(app_t *a) {
    if (a->state == INK_APP_STOPPED) return;
    if (a->state != INK_APP_FROZEN) save(a);
    stop_saved(a);
}

static void freeze_due(void *arg);

static void plan_freeze(void) { /* UI task */
    uint64_t next = 0;
    for (int i = 0; i < AP.n; i++) {
        const app_t *a = &AP.a[i];
        if (a->state == INK_APP_BACKGROUND && !a->bg && (!next || a->left_at + FREEZE_AFTER_US < next)) next = a->left_at + FREEZE_AFTER_US;
    }
    if (next) ink_svc_at(&AP.freeze_at, next, freeze_due, NULL);
    else ink_svc_cancel(&AP.freeze_at);
}

static void freeze_job(void *arg) { /* UI task */
    (void)arg;
    uint64_t now = osal_now_us();
    for (int i = 0; i < AP.n; i++) {
        app_t *a = &AP.a[i];
        if (a->state == INK_APP_BACKGROUND && !a->bg && now >= a->left_at + FREEZE_AFTER_US) freeze(a);
    }
    plan_freeze();
}

static void freeze_due(void *arg) { /* the timer task */
    (void)arg;
    ink_exec_post(INK_EXEC_UI, &AP.freeze_job, freeze_job, NULL);
}

/* What should be in the foreground now: the app of the page on top, unless the device is locked. UI task. */
static void sync_foreground(void) {
    app_t *want = AP.locked || !AP.top[0] ? NULL : find(AP.top);
    app_t *was = AP.fg >= 0 ? &AP.a[AP.fg] : NULL;
    if (want == was) return;
    uint64_t now = osal_now_us();
    if (was) {
        was->left_at = now;
        was->last_use = now;
        if (was->def->ops && was->def->ops->on_background) was->def->ops->on_background();
        set_state(was, INK_APP_BACKGROUND);
    }
    ink_svc_lock();
    AP.fg = want ? (int)(want - AP.a) : -1;
    ink_svc_unlock();
    if (want) {
        const ink_app_ops_t *ops = want->def->ops;
        if (want->state == INK_APP_STOPPED) {
            if (ops && ops->on_start) ops->on_start();
            restore(want);
        } else if (want->state == INK_APP_FROZEN && ops && ops->on_resume) ops->on_resume();
        if (ops && ops->on_foreground) ops->on_foreground();
        want->last_use = now;
        set_state(want, INK_APP_FOREGROUND);
    }
    ink_state_set_str(AP.h_foreground, want ? want->def->id : "");
    plan_freeze();
}

/* The UI's stack changed: before the new page is built. */
static void on_top(const char *app) {
    snprintf(AP.top, sizeof AP.top, "%s", app ? app : "");
    sync_foreground();
}

static void power_job(void *arg) { /* UI task */
    (void)arg;
    AP.locked = ink_power_state() == INK_PWR_LOCKED || !ink_member_space_open();
    sync_foreground();
}

static void on_power(void *ctx, ink_power_state_t from, ink_power_state_t to, uint64_t cause_us) { /* power task */
    (void)ctx;
    (void)cause_us;
    if (to == INK_PWR_LOCKED || from == INK_PWR_LOCKED) ink_exec_post(INK_EXEC_UI, &AP.power_job, power_job, NULL);
}

/* The one to go next, when memory is badly short: what is not playing first, then the longest unused. */
static app_t *next_to_stop(void) {
    app_t *best = NULL;
    for (int i = 0; i < AP.n; i++) {
        app_t *a = &AP.a[i];
        if (a->state != INK_APP_BACKGROUND && a->state != INK_APP_FROZEN) continue;
        if (a->def->keep) continue;
        bool playing = a->bg & INK_BG_PLAYBACK, best_playing = best && (best->bg & INK_BG_PLAYBACK);
        if (!best || (best_playing && !playing) || (best_playing == playing && a->last_use < best->last_use)) best = a;
    }
    return best;
}

static void pressure_job(void *arg) { /* UI task */
    (void)arg;
    ink_pressure_t level = ink_mem_pressure();
    if (level >= INK_PRESSURE_LOW) {
        for (int i = 0; i < AP.n; i++) {
            app_t *a = &AP.a[i];
            if (a->state == INK_APP_BACKGROUND && !(a->bg & INK_BG_PLAYBACK)) freeze(a);
        }
        plan_freeze();
    }
    while (ink_mem_check() >= INK_PRESSURE_CRITICAL) {
        app_t *a = next_to_stop();
        if (!a) break;
        stop(a);
    }
}

static void on_pressure(void *ctx, ink_pressure_t from, ink_pressure_t to) { /* whoever noticed */
    (void)ctx;
    (void)from;
    if (to >= INK_PRESSURE_LOW) ink_exec_post(INK_EXEC_UI, &AP.pressure_job, pressure_job, NULL);
}

/* Someone else: the pages close, all of it is saved and stopped under the old name, and home is built afresh under the new one. */
static void stop_for_member(void) { /* UI task */
    if (AP.fg >= 0) {
        app_t *a = &AP.a[AP.fg];
        if (a->def->ops && a->def->ops->on_background) a->def->ops->on_background();
        set_state(a, INK_APP_BACKGROUND);
        ink_svc_lock();
        AP.fg = -1;
        ink_svc_unlock();
    }
    for (int i = 0; i < AP.n; i++) stop(&AP.a[i]);
    snprintf(AP.member, sizeof AP.member, "%s", ink_member_current()->id);
    AP.generation = ink_member_generation(AP.member);
    plan_freeze();
}

static void member_job(void *arg) { /* UI task */
    (void)arg;
    AP.locked = ink_power_state() == INK_PWR_LOCKED || !ink_member_space_open();
    ink_ui_restart(stop_for_member);
}

static void on_member(void *ctx, int h, const char *id) {
    (void)ctx;
    (void)h;
    (void)id;
    if (strcmp(AP.member, ink_member_current()->id) || AP.generation != ink_member_generation(NULL))
        ink_exec_post(INK_EXEC_UI, &AP.member_job, member_job, NULL);
    else ink_exec_post(INK_EXEC_UI, &AP.power_job, power_job, NULL); /* the member gate keeps this person's page stack */
}

/* ---- the directory ---- */

int ink_app_register(const ink_app_def_t *def) {
    if (!def || !def->id || AP.n >= MAX_APPS || find(def->id) || !ink_layout_name_ok(def->id)) return -1;
    AP.a[AP.n++] = (app_t){.def = def}; /* at boot: nothing else looks yet */
    return 0;
}

int ink_app_count(void) { return AP.n; }

const ink_app_def_t *ink_app_at(int i) { return i >= 0 && i < AP.n ? AP.a[i].def : NULL; }

bool ink_app_visible(const ink_app_def_t *def) {
    if (ink_member_current()->role == INK_ROLE_CHILD && !def->child_ok) return false;
    for (const char *const *n = def->needs; n && *n; n++)
        if (!ink_cap_find(*n)) return false;
    return true;
}

ink_app_state_t ink_app_state_of(const char *id) {
    ink_svc_lock();
    const app_t *a = find(id);
    ink_app_state_t s = a ? a->state : INK_APP_STOPPED;
    ink_svc_unlock();
    return s;
}

int ink_app_open(const char *id, void *from, void *arg) {
    app_t *a = find(id);
    if (!a || !a->def->entry) return -1;
    if (!ink_app_visible(a->def)) return -2;
    if (ink_ui_bring_up(id)) return 0;
    if (ink_ui_full()) ink_ui_home();
    if (from) ink_ui_open(from, a->def->entry, arg);
    else ink_ui_push(a->def->entry, arg);
    return 0;
}

int ink_app_close(const char *id) {
    app_t *a = find(id);
    if (!a || a->def->keep) return -1;
    ink_ui_drop_app(id); /* its pages off the stack: the foreground moves on by itself */
    if (a->state != INK_APP_STOPPED && a->state != INK_APP_FROZEN && !save(a)) { /* the only copy of what it keeps is in it: it stays */
        plan_freeze();
        return -2;
    }
    stop_saved(a);
    plan_freeze();
    return 0;
}

int ink_app_background(const char *id, uint32_t kind, bool on) {
    ink_svc_lock();
    app_t *a = find(id);
    bool ok = a && (a->def->background & kind) == kind && kind;
    bool idle = false;
    if (ok) {
        if (on) a->bg |= kind;
        else a->bg &= ~kind;
        idle = !a->bg && a->state == INK_APP_BACKGROUND;
        if (idle) a->left_at = osal_now_us(); /* it finished: it has the same while as anyone */
    }
    ink_svc_unlock();
    if (idle) ink_exec_post(INK_EXEC_UI, &AP.freeze_job, freeze_job, NULL); /* it plans again */
    return ok ? 0 : -1;
}

ink_db_t *ink_app_db_open(const char *app, const ink_db_def_t *def) {
    size_t n = strlen(app);
    if (!def || !def->name || strncmp(def->name, app, n) || def->name[n] != '.') return NULL;
    return ink_db_open(def, NULL);
}

void ink_app_card_changed(const char *id) {
    (void)id;
    ink_state_set_int(AP.h_cards_rev, __atomic_add_fetch(&AP.cards_rev, 1, __ATOMIC_ACQ_REL));
}

bool ink_app_foreground_may(const char *permission) {
    if (!AP.ready || !permission) return true;
    bool may = false;
    ink_svc_lock();
    const app_t *a = AP.fg >= 0 ? &AP.a[AP.fg] : NULL;
    for (const char *const *p = a ? a->def->permissions : NULL; p && *p && !may; p++) may = !strcmp(*p, permission);
    ink_svc_unlock();
    return may;
}

/* ---- on the bus ---- */

static void fail(ink_call_t *c, ink_rc_t st, const char *why) {
    c->rc.status = st;
    snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
}

static void cap_list(ink_call_t *c) {
    char *out = osal_malloc(LIST_CAP, OSAL_MEM_EXTERNAL);
    if (!out) {
        fail(c, INK_RC_FAILED, "out of memory");
        return;
    }
    ink_jw_t w;
    ink_jw_init(&w, out, LIST_CAP);
    ink_jw_raw(&w, "[");
    bool first = true;
    for (int i = 0; i < AP.n; i++) {
        const ink_app_def_t *d = AP.a[i].def;
        if (!ink_app_visible(d) || !d->entry) continue;
        ink_jw_raw(&w, first ? "{\"id\":" : ",{\"id\":");
        first = false;
        ink_jw_str(&w, d->id);
        ink_jw_raw(&w, ",\"title\":");
        ink_jw_str(&w, d->title ? d->title : d->id);
        ink_jw_raw(&w, ",\"state\":");
        ink_jw_str(&w, ink_app_state_name(ink_app_state_of(d->id)));
        ink_jw_raw(&w, d->system_app ? ",\"system\":true}" : ",\"system\":false}");
    }
    ink_jw_raw(&w, "]");
    if (w.overflow) fail(c, INK_RC_FAILED, "too long");
    else ink_cap_result(c, out);
    osal_free(out);
}

static void cap_open(ink_call_t *c) { /* UI task */
    char id[32];
    ink_args_get_str(&c->args, "id", id, sizeof id);
    int r = ink_app_open(id, NULL, NULL);
    if (r == -1) fail(c, INK_RC_INVALID, "no such app");
    else if (r == -2) fail(c, INK_RC_REJECTED, "not available to this member or on this device");
    else {
        snprintf(c->rc.object, sizeof c->rc.object, "app:%s", id);
        c->rc.page_gen = ink_ui_content_version();
    }
}

static void cap_close(ink_call_t *c) { /* UI task */
    char id[32];
    ink_args_get_str(&c->args, "id", id, sizeof id);
    int r = ink_app_close(id);
    if (r == -2) fail(c, INK_RC_FAILED, "what it keeps could not be saved: it stays open");
    else if (r != 0) fail(c, INK_RC_INVALID, "no such app, or it cannot be closed");
}

static void cap_cards(ink_call_t *c) { /* UI task: apps have no other */
    char *out = osal_malloc(LIST_CAP, OSAL_MEM_EXTERNAL);
    ink_card_t *card = osal_malloc(sizeof *card, OSAL_MEM_EXTERNAL);
    if (!out || !card) {
        fail(c, INK_RC_FAILED, "out of memory");
    } else {
        ink_jw_t w;
        ink_jw_init(&w, out, LIST_CAP);
        ink_jw_raw(&w, "[");
        bool first = true;
        for (int i = 0; i < AP.n; i++) {
            const ink_app_def_t *d = AP.a[i].def;
            if (!d->card || !ink_app_visible(d)) continue;
            for (int k = 0; k < 8; k++) {
                memset(card, 0, sizeof *card);
                if (d->card(k, card) != 1) break;
                ink_jw_raw(&w, first ? "{\"app\":" : ",{\"app\":");
                first = false;
                ink_jw_str(&w, d->id);
                ink_jw_raw(&w, ",\"id\":");
                ink_jw_str(&w, card->id);
                ink_jw_raw(&w, ",\"title\":");
                ink_jw_str(&w, card->title);
                ink_jw_raw(&w, ",\"text\":");
                ink_jw_str(&w, card->text);
                if (card->action[0]) {
                    ink_jw_raw(&w, ",\"action\":");
                    ink_jw_str(&w, card->action);
                }
                if (card->has_value) {
                    ink_jw_raw(&w, ",\"value\":");
                    ink_jw_int(&w, card->value);
                }
                ink_jw_raw(&w, "}");
            }
        }
        ink_jw_raw(&w, "]");
        if (w.overflow) fail(c, INK_RC_FAILED, "too long");
        else ink_cap_result(c, out);
    }
    osal_free(card);
    osal_free(out);
}

#define ID_ARG "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":31}},\"required\":[\"id\"]}"

static const ink_cap_t APP_CAPS[] = {
    {.id = "app.list", .title = "这台设备上有哪些应用，各自的状态", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_list},
    {.id = "app.open", .title = "打开一个应用", .input = ID_ARG, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_open},
    {.id = "app.close", .title = "关掉一个应用（它会保存状态）", .input = ID_ARG, .effect = INK_FX_NAVIGATE, .child_ok = true, .run = cap_close},
    {.id = "app.cards", .title = "各应用的服务卡片（和主页看到的一样）", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .run = cap_cards},
    {.id = "app.state", .title = "一个应用的状态变了（foreground、background、frozen、stopped）", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

static const ink_state_def_t ST_FOREGROUND = {.id = "app.foreground", .type = INK_ST_STR, .title = "前台应用"};
static const ink_state_def_t ST_CARDS_REV = {.id = "app.cards_rev", .type = INK_ST_INT, .title = "服务卡片的版本（变了就重读）"};

int ink_apps_init(void) {
    AP.fg = -1;
    AP.locked = ink_power_state() == INK_PWR_LOCKED || !ink_member_space_open();
    snprintf(AP.member, sizeof AP.member, "%s", ink_member_current()->id);
    AP.generation = ink_member_generation(AP.member);
    AP.h_foreground = ink_state_define(&ST_FOREGROUND);
    AP.h_cards_rev = ink_state_define(&ST_CARDS_REV);
    ink_state_set_str(AP.h_foreground, "");
    ink_state_set_int(AP.h_cards_rev, 0);
    ink_ui_set_top_hook(on_top);
    if (ink_cap_register(APP_CAPS, (int)(sizeof APP_CAPS / sizeof APP_CAPS[0])) != 0 || ink_power_subscribe(on_power, NULL) != 0 || ink_mem_on_pressure(on_pressure, NULL) != 0 ||
        ink_state_watch("member.current", on_member, NULL, INK_EXEC_DIRECT) != 0 || ink_state_watch("member.authed", on_member, NULL, INK_EXEC_DIRECT) != 0)
        return -1;
    AP.ready = true;
    return 0;
}
