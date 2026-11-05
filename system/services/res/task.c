/*
 * The list of what goes on in the background (ink/sys/task.h). A small table under the shared lock: what ended stays until there
 * is no room for something new, the oldest of it going first. Each member sees their own and what is for everyone.
 */
#include <stdio.h>
#include <string.h>

#include "ink/osal.h"
#include "ink/probe.h"
#include "ink/sys/cap.h"
#include "ink/sys/json.h"
#include "ink/sys/layout.h"
#include "ink/sys/members.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "ink/sys/task.h"
#include "ink/sys/time.h"

#define TASKS_MAX 16
#define LIST_MAX 6144 /* all of them with everything filled in */

typedef struct {
    int id;
    ink_task_state_t state;
    int percent;
    bool for_everyone;
    void (*cancel)(void *ctx);
    void *cancel_ctx;
    char kind[24], title[64], stage[16], result[96], object[48];
    char member[INK_MEMBER_ID_MAX];
    int64_t ended_s;
    uint32_t order;
} task_t;

static struct {
    task_t t[TASKS_MAX];
    int next_id;
    uint32_t next_order;
    int h_running;
    bool ready;
} T INK_PSRAM_BSS;

static const char *const STATE_NAME[] = {"running", "done", "failed", "cancelled"};

static void publish_running(void) {
    if (!T.ready) return; /* some test programs run without the list */
    int n = 0;
    ink_svc_lock();
    for (int i = 0; i < TASKS_MAX; i++) n += T.t[i].id && T.t[i].state == INK_TASK_RUNNING;
    ink_svc_unlock();
    ink_state_set_int(T.h_running, n);
}

static task_t *find(int id) { /* under the lock */
    for (int i = 0; id > 0 && i < TASKS_MAX; i++)
        if (T.t[i].id == id) return &T.t[i];
    return NULL;
}

int ink_task_begin(const char *kind, const char *title, bool for_everyone, void (*cancel)(void *ctx), void *ctx) {
    char member[INK_MEMBER_ID_MAX];
    snprintf(member, sizeof member, "%s", for_everyone ? "" : ink_member_current()->id);
    ink_svc_lock();
    task_t *slot = NULL, *oldest = NULL;
    for (int i = 0; i < TASKS_MAX && !slot; i++) {
        task_t *t = &T.t[i];
        if (!t->id) slot = t;
        else if (t->state != INK_TASK_RUNNING && (!oldest || t->order < oldest->order)) oldest = t;
    }
    if (!slot) slot = oldest;
    int id = 0;
    if (slot) {
        id = ++T.next_id;
        *slot = (task_t){.id = id, .state = INK_TASK_RUNNING, .percent = -1, .for_everyone = for_everyone, .cancel = cancel, .cancel_ctx = ctx, .order = ++T.next_order};
        snprintf(slot->kind, sizeof slot->kind, "%s", kind);
        snprintf(slot->title, sizeof slot->title, "%s", title);
        snprintf(slot->member, sizeof slot->member, "%s", member);
    }
    ink_svc_unlock();
    if (id) publish_running();
    else ink_probe_count("task.no_room");
    return id;
}

void ink_task_progress(int id, int percent, const char *stage) {
    ink_svc_lock();
    task_t *t = find(id);
    if (t && t->state == INK_TASK_RUNNING) {
        t->percent = percent;
        if (stage) snprintf(t->stage, sizeof t->stage, "%s", stage);
    }
    ink_svc_unlock();
}

void ink_task_end(int id, ink_task_state_t how, const char *result, const char *object) {
    ink_args_t *a = osal_malloc(sizeof *a, OSAL_MEM_EXTERNAL);
    if (a) ink_args_init(a);
    ink_svc_lock();
    task_t *t = find(id);
    bool ended = t && t->state == INK_TASK_RUNNING;
    if (ended) {
        t->state = how;
        t->cancel = NULL;
        t->ended_s = ink_time_now_s();
        if (how == INK_TASK_DONE) t->percent = 100;
        if (result) snprintf(t->result, sizeof t->result, "%s", result);
        if (object) snprintf(t->object, sizeof t->object, "%s", object);
        if (a) {
            ink_args_int(a, "id", id);
            ink_args_str(a, "kind", t->kind);
            ink_args_str(a, "title", t->title);
            ink_args_str(a, "state", STATE_NAME[how]);
            if (t->result[0]) ink_args_str(a, "result", t->result);
            if (t->object[0]) ink_args_str(a, "object", t->object);
            if (t->member[0]) ink_args_str(a, "member", t->member); /* told only while it is theirs who is here */
        }
    }
    ink_svc_unlock();
    if (ended) {
        publish_running();
        if (a) ink_cap_emit("job.finished", a);
    }
    osal_free(a);
}

/* Theirs, or for everyone. */
static bool visible(const task_t *t, const char *member) { return t->id && (t->for_everyone || !strcmp(t->member, member)); }

/* Newest first: what runs, then what ended. */
static void cap_list(ink_call_t *c) {
    char *out = osal_malloc(LIST_MAX, OSAL_MEM_EXTERNAL);
    task_t *copy = osal_malloc(sizeof T.t, OSAL_MEM_EXTERNAL);
    if (!out || !copy) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "out of memory");
        osal_free(out);
        osal_free(copy);
        return;
    }
    ink_svc_lock();
    memcpy(copy, T.t, sizeof T.t);
    ink_svc_unlock();
    const char *member = ink_member_current()->id;
    ink_jw_t w;
    ink_jw_init(&w, out, LIST_MAX);
    ink_jw_raw(&w, "[");
    bool first = true;
    for (int running = 1; running >= 0; running--) {
        uint32_t below = UINT32_MAX;
        for (;;) {
            const task_t *t = NULL;
            for (int i = 0; i < TASKS_MAX; i++) {
                const task_t *x = &copy[i];
                if (visible(x, member) && (x->state == INK_TASK_RUNNING) == (running == 1) && x->order < below && (!t || x->order > t->order)) t = x;
            }
            if (!t) break;
            below = t->order;
            ink_jw_raw(&w, first ? "{\"id\":" : ",{\"id\":");
            first = false;
            ink_jw_int(&w, t->id);
            ink_jw_raw(&w, ",\"kind\":");
            ink_jw_str(&w, t->kind);
            ink_jw_raw(&w, ",\"title\":");
            ink_jw_str(&w, t->title);
            ink_jw_raw(&w, ",\"state\":");
            ink_jw_str(&w, STATE_NAME[t->state]);
            if (t->percent >= 0) {
                ink_jw_raw(&w, ",\"percent\":");
                ink_jw_int(&w, t->percent);
            }
            if (t->stage[0]) {
                ink_jw_raw(&w, ",\"stage\":");
                ink_jw_str(&w, t->stage);
            }
            if (t->cancel) ink_jw_raw(&w, ",\"can_cancel\":true");
            if (t->result[0]) {
                ink_jw_raw(&w, ",\"result\":");
                ink_jw_str(&w, t->result);
            }
            if (t->object[0]) {
                ink_jw_raw(&w, ",\"object\":");
                ink_jw_str(&w, t->object);
            }
            if (t->ended_s) {
                ink_jw_raw(&w, ",\"ended_s\":");
                ink_jw_int(&w, t->ended_s);
            }
            ink_jw_raw(&w, "}");
        }
    }
    ink_jw_raw(&w, "]");
    if (w.overflow) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "too long");
    } else ink_cap_result(c, out);
    osal_free(copy);
    osal_free(out);
}

static void cap_cancel(ink_call_t *c) {
    int64_t id = 0;
    ink_args_get_int(&c->args, "id", &id);
    const ink_member_t *m = ink_member_current();
    void (*cancel)(void *) = NULL;
    void *ctx = NULL;
    const char *why = NULL;
    ink_svc_lock();
    task_t *t = find((int)id);
    if (!t || !visible(t, m->id)) why = "no such task";
    else if (t->state != INK_TASK_RUNNING) why = "it is over";
    else if (!t->cancel) why = "this one cannot be stopped";
    else if (t->for_everyone && m->role != INK_ROLE_ADULT) why = "an adult can stop this one";
    else {
        cancel = t->cancel;
        ctx = t->cancel_ctx;
    }
    ink_svc_unlock();
    if (why) {
        c->rc.status = INK_RC_FAILED;
        snprintf(c->rc.detail, sizeof c->rc.detail, "%s", why);
        return;
    }
    cancel(ctx);
    snprintf(c->rc.say, sizeof c->rc.say, "正在停下");
}

static const ink_cap_t TASK_CAPS[] = {
    {.id = "job.list", .title = "后台在做什么、做到哪了、做完的结果", .output = "{\"type\":\"array\"}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_list},
    {.id = "job.cancel", .title = "停下一件后台在做的事",
     .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1}},\"required\":[\"id\"]}",
     .effect = INK_FX_NAVIGATE, .child_ok = true, .exec = INK_EXEC_DIRECT, .run = cap_cancel},
    {.id = "job.finished", .title = "一件后台的事结束了（做好了、失败了、停下了）", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};

static const ink_state_def_t ST_RUNNING = {.id = "job.running", .type = INK_ST_INT, .title = "后台有几件事在做"};

int ink_task_init(void) {
    T.h_running = ink_state_define(&ST_RUNNING);
    ink_state_set_int(T.h_running, 0);
    T.ready = true;
    return ink_cap_register(TASK_CAPS, (int)(sizeof TASK_CAPS / sizeof TASK_CAPS[0]));
}
