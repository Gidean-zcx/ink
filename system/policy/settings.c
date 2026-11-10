/*
 * Settings table (see ink/sys/settings.h): values, policy, the owning
 * service's apply hook, watchers, and the file they are kept in. Values read
 * from the file for settings this build does not define are kept and written
 * back, so an older and a newer firmware can share the file.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/json.h"
#include "ink/sys/members.h"
#include "ink/sys/confirm.h"
#include "ink/sys/settings.h"
#include "ink/sys/storage.h"
#include "watch.h"

#define SAVE_DELAY_US 1000000ull /* one write for a burst of changes (a volume slider) */
#define FILE_CAP 2048

typedef struct {
    const ink_setting_def_t *def;
    int64_t v;
    uint32_t revision;
    ink_exec_job_t apply;
} item_t;

typedef struct {
    char id[40];
    int64_t v;
} saved_t;

static struct {
    osal_mutex_t *lock;
    item_t it[INK_SETTINGS_MAX];
    int n;
    saved_t saved[INK_SETTINGS_MAX]; /* as read from the file */
    int nsaved;
    watch_list_t watch;
    osal_timer_t *save_timer;
    bool caps;
} SE INK_PSRAM_BSS;

static const char *id_of(int i) { return i >= 0 && i < SE.n ? SE.it[i].def->id : ""; }

static int find(const char *id) {
    for (int i = 0; i < SE.n; i++)
        if (!strcmp(SE.it[i].def->id, id)) return i;
    return -1;
}

static int nchoices(const ink_setting_def_t *d) {
    int n = 0;
    while (d->choices && d->choices[n]) n++;
    return n;
}

static bool valid(const ink_setting_def_t *d, int64_t v) {
    switch (d->type) {
    case INK_SET_BOOL: return v == 0 || v == 1;
    case INK_SET_INT: return v >= d->min && v <= d->max;
    case INK_SET_ENUM: return v >= 0 && v < nchoices(d);
    default: return false;
    }
}

/* ------------------------------------------------------------ the file */

static void load(void) {
    SE.nsaved = 0;
    char *buf = osal_malloc(FILE_CAP, OSAL_MEM_EXTERNAL);
    ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    int n = buf && j ? ink_storage_read(INK_SETTINGS_PATH, buf, FILE_CAP - 1) : -1;
    if (n > 0 && ink_json_parse(j, buf, (size_t)n) == 0 && j->tok[0].type == 1) {
        int t = 1;
        for (int k = 0; k < j->tok[0].size && t + 1 < j->ntok && SE.nsaved < INK_SETTINGS_MAX; k++) {
            saved_t *s = &SE.saved[SE.nsaved];
            const char *raw = j->text + j->tok[t + 1].start;
            int64_t v;
            bool ok = ink_json_str(j, t, s->id, sizeof s->id) > 0;
            if (ok && ink_json_int(j, t + 1, &v)) s->v = v;
            else if (ok && j->tok[t + 1].type == 8 && (raw[0] == 't' || raw[0] == 'f')) s->v = raw[0] == 't';
            else ok = false;
            if (ok) SE.nsaved++;
            else ink_probe_count("settings.file_bad_entry");
            t += 2; /* values are scalars; anything else is skipped as a bad entry */
            while (t < j->ntok && j->tok[t].start < j->tok[t - 1].end) t++;
        }
    } else if (n > 0) ink_probe_count("settings.file_unreadable");
    osal_free(j);
    osal_free(buf);
}

static void save_cb(void *arg) { /* timer task */
    (void)arg;
    char *buf = osal_malloc(FILE_CAP, OSAL_MEM_EXTERNAL);
    if (!buf) {
        ink_probe_count("settings.save_failed");
        return;
    }
    ink_jw_t w;
    ink_jw_init(&w, buf, FILE_CAP);
    ink_jw_raw(&w, "{");
    osal_mutex_lock(SE.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < SE.n; i++) {
        if (i) ink_jw_raw(&w, ",");
        ink_jw_str(&w, SE.it[i].def->id);
        ink_jw_raw(&w, ":");
        ink_jw_int(&w, SE.it[i].v);
    }
    for (int k = 0; k < SE.nsaved; k++) /* values of settings this build does not define */
        if (find(SE.saved[k].id) < 0) {
            if (SE.n || k) ink_jw_raw(&w, ",");
            ink_jw_str(&w, SE.saved[k].id);
            ink_jw_raw(&w, ":");
            ink_jw_int(&w, SE.saved[k].v);
        }
    osal_mutex_unlock(SE.lock);
    ink_jw_raw(&w, "}");
    if (w.overflow || ink_storage_write_atomic(INK_SETTINGS_PATH, buf, w.len) != 0) ink_probe_count("settings.save_failed");
    else ink_probe_count("settings.saved");
    osal_free(buf);
}

/* ------------------------------------------------------------ table */

static void register_caps(void);

int ink_settings_init(void) {
    if (!SE.lock) {
        SE.lock = osal_mutex_create();
        SE.save_timer = osal_timer_create("settings", save_cb, NULL);
        if (!SE.lock || !SE.save_timer || watch_list_init(&SE.watch, SE.lock, id_of, "settings.watch_dropped")) return -1;
    }
    load();
    if (!SE.caps) {
        register_caps();
        SE.caps = true;
    }
    return 0;
}

int ink_settings_define(const ink_setting_def_t *defs, int n) {
    if (!SE.lock) return -1;
    osal_mutex_lock(SE.lock, OSAL_WAIT_FOREVER);
    int r = 0;
    for (int i = 0; i < n; i++) {
        if (SE.n >= INK_SETTINGS_MAX || find(defs[i].id) >= 0) {
            r = -1;
            continue;
        }
        int64_t v = defs[i].def;
        for (int k = 0; k < SE.nsaved; k++)
            if (!strcmp(SE.saved[k].id, defs[i].id)) {
                if (valid(&defs[i], SE.saved[k].v)) v = SE.saved[k].v;
                else ink_probe_count("settings.saved_out_of_range");
                break;
            }
        SE.it[SE.n++] = (item_t){.def = &defs[i], .v = v};
    }
    osal_mutex_unlock(SE.lock);
    return r;
}

int64_t ink_setting_get(const char *id) {
    if (!SE.lock) return 0;
    osal_mutex_lock(SE.lock, OSAL_WAIT_FOREVER);
    int i = find(id);
    int64_t v = i >= 0 ? SE.it[i].v : 0;
    osal_mutex_unlock(SE.lock);
    return v;
}

static void apply_job(void *arg) { /* on the setting's apply executor */
    item_t *it = arg;
    osal_mutex_lock(SE.lock, OSAL_WAIT_FOREVER);
    int64_t v = it->v;
    osal_mutex_unlock(SE.lock);
    it->def->apply(it->def->ctx, v);
}

static void value_text(const ink_setting_def_t *d, int64_t v, char *out, size_t cap) {
    if (d->type == INK_SET_ENUM && v >= 0 && v < nchoices(d)) snprintf(out, cap, "%s", d->choices[v]);
    else if (d->type == INK_SET_BOOL) snprintf(out, cap, "%s", v ? "开" : "关");
    else snprintf(out, cap, "%lld", (long long)v);
}

static ink_rc_t refuse(ink_receipt_t *rc, ink_rc_t st, const char *why) {
    rc->status = st;
    snprintf(rc->detail, sizeof rc->detail, "%s", why);
    return st;
}

static ink_rc_t setting_set(const char *id, int64_t v, ink_source_t src, bool check_revision,
                            uint32_t revision, int64_t *before, ink_receipt_t *rc) {
    memset(rc, 0, sizeof *rc);
    if (!SE.lock) return refuse(rc, INK_RC_FAILED, "settings not started");
    osal_mutex_lock(SE.lock, OSAL_WAIT_FOREVER);
    int i = find(id);
    const ink_setting_def_t *d = i >= 0 ? SE.it[i].def : NULL;
    osal_mutex_unlock(SE.lock);
    if (!d) return refuse(rc, INK_RC_INVALID, "unknown setting");
    if (!valid(d, v)) return refuse(rc, INK_RC_INVALID, "value out of range");
    if (ink_member_current()->role == INK_ROLE_CHILD && !strcmp(id, "audio.volume") && v > ink_setting_get("audio.child_max"))
        v = ink_setting_get("audio.child_max");
    if (ink_member_current()->role == INK_ROLE_CHILD && (d->system || !d->child_may_change)) {
        ink_probe_count("policy.rejected");
        return refuse(rc, INK_RC_REJECTED, "children cannot change this setting");
    }
    if (d->ask_by_hand && src != INK_SRC_CONFIRMED && src != INK_SRC_SYSTEM) {
        ink_args_t args;
        ink_args_init(&args);
        ink_args_str(&args, "id", id);
        ink_args_int(&args, "value", v);
        if (check_revision) ink_args_int(&args, "expected_revision", revision);
        return ink_confirm_hold(ink_cap_find("settings.set"), &args, src, 0, rc);
    }
    if (d->apply && ink_exec_ready(d->apply_exec) != 0) return refuse(rc, INK_RC_FAILED, "cannot be applied now"); /* not stored either */
    osal_mutex_lock(SE.lock, OSAL_WAIT_FOREVER);
    if (check_revision && SE.it[i].revision != revision) {
        osal_mutex_unlock(SE.lock);
        return refuse(rc, INK_RC_STALE, "the setting changed since this operation");
    }
    if (before) *before = SE.it[i].v;
    bool changed = SE.it[i].v != v;
    SE.it[i].v = v;
    rc->revision = ++SE.it[i].revision;
    osal_mutex_unlock(SE.lock);
    rc->status = INK_RC_DONE;
    snprintf(rc->object, sizeof rc->object, "setting:%s", id);
    char vt[32];
    value_text(d, v, vt, sizeof vt);
    snprintf(rc->say, sizeof rc->say, "%s：%s", d->title, vt);
    /* also the same value again: its owner looks once more at what the hardware does (the job reads the newest value) */
    if (d->apply) ink_exec_post(d->apply_exec, &SE.it[i].apply, apply_job, &SE.it[i]);
    if (changed) {
        ink_probe_count("settings.changed");
        watch_changed(&SE.watch, i, id);
        osal_timer_start(SE.save_timer, SAVE_DELAY_US, 0); /* re-armed: one write for a burst */
    }
    return rc->status;
}

ink_rc_t ink_setting_set(const char *id, int64_t v, ink_source_t src, ink_receipt_t *rc) {
    return setting_set(id, v, src, false, 0, NULL, rc);
}

int ink_settings_watch(const char *prefix, ink_state_fn fn, void *ctx, ink_exec_t exec) {
    return watch_add(&SE.watch, prefix, fn, ctx, exec, false) >= 0 ? 0 : -1;
}
int ink_settings_watch_handle(const char *prefix, ink_state_fn fn, void *ctx, ink_exec_t exec) {
    return exec == INK_EXEC_DIRECT ? -1 : watch_add(&SE.watch, prefix, fn, ctx, exec, true);
}
int ink_settings_unwatch(int handle) { return watch_remove(&SE.watch, handle); }

static const char *TYPE_NAME[] = {"", "bool", "int", "enum"};

const ink_setting_def_t *ink_setting_at(int index) {
    osal_mutex_lock(SE.lock, OSAL_WAIT_FOREVER);
    const ink_setting_def_t *d = index >= 0 && index < SE.n ? SE.it[index].def : NULL;
    osal_mutex_unlock(SE.lock);
    return d;
}

int ink_settings_json(char *out, size_t cap) {
    ink_jw_t w;
    ink_jw_init(&w, out, cap);
    ink_jw_raw(&w, "[");
    osal_mutex_lock(SE.lock, OSAL_WAIT_FOREVER);
    for (int i = 0; i < SE.n; i++) {
        const ink_setting_def_t *d = SE.it[i].def;
        ink_jw_raw(&w, i ? ",{\"id\":" : "{\"id\":");
        ink_jw_str(&w, d->id);
        ink_jw_raw(&w, ",\"title\":");
        ink_jw_str(&w, d->title);
        ink_jw_raw(&w, ",\"type\":");
        ink_jw_str(&w, TYPE_NAME[d->type]);
        ink_jw_raw(&w, ",\"value\":");
        ink_jw_int(&w, SE.it[i].v);
        if (d->type == INK_SET_INT) {
            ink_jw_raw(&w, ",\"min\":");
            ink_jw_int(&w, d->min);
            ink_jw_raw(&w, ",\"max\":");
            ink_jw_int(&w, d->max);
        }
        if (d->type == INK_SET_ENUM) {
            ink_jw_raw(&w, ",\"choices\":[");
            for (int k = 0; d->choices[k]; k++) {
                if (k) ink_jw_raw(&w, ",");
                ink_jw_str(&w, d->choices[k]);
            }
            ink_jw_raw(&w, "]");
        }
        if (d->system) ink_jw_raw(&w, ",\"system\":true");
        if (d->ask_by_hand) ink_jw_raw(&w, ",\"ask_by_hand\":true");
        if (d->via) {
            ink_jw_raw(&w, ",\"via\":");
            ink_jw_str(&w, d->via);
        }
        ink_jw_raw(&w, "}");
    }
    osal_mutex_unlock(SE.lock);
    ink_jw_raw(&w, "]");
    return w.overflow ? -1 : (int)w.len;
}

/* ------------------------------------------------------------ capabilities */

static void cap_get(ink_call_t *c) {
    char id[40];
    ink_args_get_str(&c->args, "id", id, sizeof id);
    osal_mutex_lock(SE.lock, OSAL_WAIT_FOREVER);
    int i = find(id);
    const ink_setting_def_t *d = i >= 0 ? SE.it[i].def : NULL;
    int64_t v = i >= 0 ? SE.it[i].v : 0;
    uint32_t revision = i >= 0 ? SE.it[i].revision : 0;
    osal_mutex_unlock(SE.lock);
    if (!d) {
        refuse(&c->rc, INK_RC_INVALID, "unknown setting");
        return;
    }
    char vt[32];
    value_text(d, v, vt, sizeof vt);
    snprintf(c->rc.object, sizeof c->rc.object, "setting:%s", id);
    c->rc.revision = revision;
    snprintf(c->rc.say, sizeof c->rc.say, "%s：%s", d->title, vt);
}

static void via_done(void *ctx, const ink_receipt_t *rc) { /* the setting's own capability answered */
    ink_call_t *c = ctx;
    c->rc = *rc;
    c->rc.result = NULL;
    ink_cap_done(c);
}

static void cap_set(ink_call_t *c) {
    char id[40], s[32];
    ink_args_get_str(&c->args, "id", id, sizeof id);
    int64_t v = 0;
    bool b;
    if (ink_args_get_int(&c->args, "value", &v)) {
        /* as given */
    } else if (ink_args_get_bool(&c->args, "value", &b)) {
        v = b;
    } else if (ink_args_get_str(&c->args, "value", s, sizeof s) >= 0) { /* an enum value by name */
        osal_mutex_lock(SE.lock, OSAL_WAIT_FOREVER);
        int i = find(id);
        v = -1;
        for (int k = 0; i >= 0 && SE.it[i].def->choices && SE.it[i].def->choices[k]; k++)
            if (!strcmp(SE.it[i].def->choices[k], s)) v = k;
        osal_mutex_unlock(SE.lock);
        if (v < 0) {
            refuse(&c->rc, INK_RC_INVALID, "not one of this setting's values");
            return;
        }
    } else {
        refuse(&c->rc, INK_RC_INVALID, "value required");
        return;
    }
    osal_mutex_lock(SE.lock, OSAL_WAIT_FOREVER);
    int i = find(id);
    const char *via = i >= 0 && (v == 0 || v == 1) ? SE.it[i].def->via : NULL;
    osal_mutex_unlock(SE.lock);
    if (via && c->src != INK_SRC_SYSTEM) { /* its own capability decides: past its gate, with its order and words */
        ink_args_t a;
        ink_args_init(&a);
        ink_args_bool(&a, "enabled", v != 0);
        ink_call_opts_t o = {.src = c->src, .page_gen = c->page_gen, .member = ink_cap_call_member(c),
                             .reply_exec = INK_EXEC_DIRECT, .done = via_done, .ctx = c};
        ink_cap_defer(c);
        if (ink_cap_call(via, &a, &o) != 0) {
            refuse(&c->rc, INK_RC_FAILED, "cannot be changed now");
            ink_cap_done(c);
        }
        return;
    }
    int64_t before = 0, revision = 0;
    bool check_revision = ink_args_get_int(&c->args, "expected_revision", &revision);
    if (setting_set(id, v, c->src, check_revision, (uint32_t)revision, &before, &c->rc) == INK_RC_DONE && before != v && !check_revision) {
        ink_args_t undo;
        ink_args_init(&undo);
        ink_args_str(&undo, "id", id);
        ink_args_int(&undo, "value", before);
        ink_args_int(&undo, "expected_revision", c->rc.revision);
        ink_cap_undoable(c, "settings.set", &undo, 10000);
    }
}

static const ink_cap_t SETTINGS_CAPS[] = {
    {.id = "settings.get",
     .title = "查看一项设置",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":39}},\"required\":[\"id\"]}",
     .effect = INK_FX_READ,
     .child_ok = true,
     .run = cap_get},
    {.id = "settings.set",
     .title = "改一项设置",
     .ask = "把设置 {id} 改成 {value}",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\",\"maxLength\":39},"
              "\"value\":{\"type\":[\"integer\",\"boolean\",\"string\"]},\"expected_revision\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":4294967295}},\"required\":[\"id\",\"value\"]}",
     .effect = INK_FX_LOCAL_WRITE,
     .child_ok = true, /* each setting says whether children may change it */
     .run = cap_set},
};

static void register_caps(void) { ink_cap_register(SETTINGS_CAPS, (int)(sizeof SETTINGS_CAPS / sizeof SETTINGS_CAPS[0])); }
