/* Pebble schedule logic, with ink member storage, time and capability adapters. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ink/osal.h"
#include "ink/sys/i18n.h"
#include "ink/sys/members.h"
#include "ink/sys/settings.h"
#include "ink/sys/state.h"
#include "ink/sys/svc.h"
#include "ink/sys/time.h"
#include "notify_internal.h"

enum { SLOTS = 5, REPLIES = 5, RECENT = 8, PREF_CAP = 1536 };
typedef struct { uint8_t from_hour, from_minute, to_hour, to_minute; } DoNotDisturbSchedule;
typedef struct { uint8_t used, enabled, days, legacy; DoNotDisturbSchedule schedule; } DoNotDisturbScheduleSlot;
struct pbl_tm { int tm_hour, tm_min, tm_wday; };
#define PBL_DAY_PER_WEEK 7
#define PBL_CRON_WDAY_ANY 127
#define PBL_MIN_PER_HOUR 60
#define do_not_disturb_schedule_slot_get_days(slot) ((slot)->days)
#include "vendor/pebble/schedule.inc"

typedef struct { DoNotDisturbScheduleSlot slots[SLOTS]; int calls, repeat, calendar; char replies[REPLIES][128], member[16]; } prefs_t;
static prefs_t P INK_PSRAM_BSS;
static struct { char number[24], member[16]; uint64_t when; bool used; } recent[RECENT] INK_PSRAM_BSS;
static unsigned recent_next;
static int h_active, h_reason, h_until;
static const ink_db_def_t PREFS = {.name = "notify.prefs", .title = "通知偏好", .scope = INK_DB_MEMBER, .schema = 1, .json = true, .ai_read = true};
static const char *const DEFAULTS[] = {TR_NOOP("好的"), TR_NOOP("是"), TR_NOOP("不是"), TR_NOOP("给我回个电话"), TR_NOOP("我晚点给你回电话")};
_Static_assert(sizeof DEFAULTS / sizeof *DEFAULTS == REPLIES, "reply count");
extern int ink_contacts_match(const char *query, bool exact, char *name, char *number);
extern bool ink_contacts_starred(const char *number);

static int64_t value(const ink_json_t *j, int obj, const char *key) {
    int64_t v = 0; ink_json_int(j, ink_json_find(j, obj, key), &v); return v;
}
static void load_prefs(void) { /* BG only. Record 1 remains the channel mask. */
    prefs_t p = {0}; snprintf(p.member, sizeof p.member, "%s", ink_member_current()->id);
    char *raw = osal_calloc(1, PREF_CAP, OSAL_MEM_EXTERNAL); ink_json_t *j = osal_malloc(sizeof *j, OSAL_MEM_EXTERNAL);
    ink_db_t *db = ink_db_open_member(&PREFS, p.member);
    if (db && raw && j && ink_db_get(db, 2, NULL, raw, PREF_CAP - 1) == INK_DB_OK && !ink_json_parse(j, raw, strlen(raw))) {
        p.calls = (int)value(j, 0, "calls"); p.repeat = (int)value(j, 0, "repeat"); p.calendar = (int)value(j, 0, "calendar");
        for (int i = 0; i < SLOTS; i++) {
            char key[24]; snprintf(key, sizeof key, "slots.%d", i); int t = ink_json_find(j, 0, key);
            int start = (int)value(j, t, "start"), end = (int)value(j, t, "end");
            p.slots[i] = (DoNotDisturbScheduleSlot){.used = 1, .enabled = 1, .days = (uint8_t)value(j, t, "days"), .schedule = {(uint8_t)(start / 60), (uint8_t)(start % 60), (uint8_t)(end / 60), (uint8_t)(end % 60)}};
        }
        for (int i = 0; i < REPLIES; i++) { char key[24]; snprintf(key, sizeof key, "replies.%d", i); ink_json_str(j, ink_json_find(j, 0, key), p.replies[i], sizeof p.replies[i]); }
    }
    ink_db_close(db); osal_free(raw); osal_free(j);
    ink_svc_lock(); P = p; ink_svc_unlock();
}
static void write_prefs(ink_jw_t *w, const prefs_t *p) {
    char b[112]; snprintf(b, sizeof b, "{\"calls\":%d,\"repeat\":%d,\"calendar\":%d,\"slots\":{", p->calls, p->repeat, p->calendar); ink_jw_raw(w, b);
    for (int i = 0; i < SLOTS; i++) {
        const DoNotDisturbSchedule *s = &p->slots[i].schedule;
        snprintf(b, sizeof b, "%s\"%d\":{\"days\":%u,\"start\":%d,\"end\":%d}", i ? "," : "", i, p->slots[i].days, prv_minutes(s->from_hour, s->from_minute), prv_minutes(s->to_hour, s->to_minute)); ink_jw_raw(w, b);
    }
    ink_jw_raw(w, "},\"replies\":{");
    for (int i = 0; i < REPLIES; i++) { snprintf(b, sizeof b, "%s\"%d\":", i ? "," : "", i); ink_jw_raw(w, b); ink_jw_str(w, p->replies[i][0] ? p->replies[i] : TR(DEFAULTS[i])); }
    ink_jw_raw(w, "}}");
}
void ink_notify_dnd_state(ink_notify_dnd_t *out) {
    *out = (ink_notify_dnd_t){.slot = -1, .reason = "off"};
    if (ink_setting_get("notify.dnd")) { out->active = true; out->reason = "manual"; return; }
    DoNotDisturbScheduleSlot slots[SLOTS]; char member[16]; int calendar;
    ink_svc_lock(); memcpy(slots, P.slots, sizeof slots); calendar = P.calendar; snprintf(member, sizeof member, "%s", P.member); ink_svc_unlock();
    int64_t now = ink_time_now_s();
    if (!now || strcmp(member, ink_member_current()->id)) return;
    ink_tm_t tm; ink_time_to_local(now, &tm); struct pbl_tm local = {tm.hour, tm.min, tm.wday};
    for (int i = 0; i < SLOTS; i++) if (prv_is_in_slot_period(&slots[i], &local)) {
        ink_tm_t end = tm; const DoNotDisturbSchedule *s = &slots[i].schedule;
        if (prv_minutes(tm.hour, tm.min) >= prv_minutes(s->to_hour, s->to_minute)) end.day++;
        end.hour = s->to_hour; end.min = s->to_minute; end.sec = 0;
        int64_t until = ink_time_from_local(&end);
        if (until > out->until_s) { out->active = true; out->reason = "schedule"; out->slot = i; out->until_s = until; }
    }
    if (calendar) {
        int cursor = 0; ink_alarm_t a;
        while (ink_alarm_next(&cursor, INK_REMINDER, &a) == 1) {
            int64_t start = ink_alarm_occurrence(&a, now);
            if (!a.all_day && start > 0 && start <= now && a.duration_s > 0 && now < start + a.duration_s && start + a.duration_s > out->until_s) {
                out->active = true; out->reason = "calendar"; out->slot = -1; out->until_s = start + a.duration_s;
            }
        }
    }
}
static void changed(void *ctx, int h, const char *id) {
    (void)ctx; (void)h;
    if (id && !strcmp(id, "member.current")) load_prefs();
    ink_notify_dnd_t d; ink_notify_dnd_state(&d);
    int64_t active = -1, until = -1; char reason[INK_STATE_STR] = "";
    if (id && strcmp(id, "member.current") && ink_state_get_int(h_active, &active) && ink_state_get_int(h_until, &until) &&
        ink_state_get_str(h_reason, reason, sizeof reason) >= 0 && active == d.active && until == d.until_s && !strcmp(reason, d.reason)) return;
    ink_state_set_int(h_until, d.until_s); ink_state_set_str(h_reason, d.reason); ink_state_set_bool(h_active, d.active);
    ink_cap_emit("notify.dnd_changed", NULL);
}
static void time_changed(void *ctx, const char *id, const ink_args_t *args) { (void)args; changed(ctx, 0, id); }
static void boot_model(void *ctx) { changed(ctx, 0, "member.current"); }
bool notify_allowed(const char *number) { /* Contact storage is touched only by the BG call job. */
    ink_svc_lock(); int calls = P.calls, repeats = P.repeat; ink_svc_unlock();
    bool repeat = false;
    if (number && number[0]) {
        uint64_t now = osal_now_us(); const char *member = ink_member_current()->id;
        for (int i = 0; i < RECENT; i++) if (recent[i].used && !strcmp(recent[i].member, member) && !strcmp(recent[i].number, number)) {
            repeat = repeat || now - recent[i].when <= UINT64_C(900000000);
        }
        unsigned at = recent_next++ % RECENT; recent[at].used = true; recent[at].when = now;
        snprintf(recent[at].number, sizeof recent[at].number, "%s", number); snprintf(recent[at].member, sizeof recent[at].member, "%s", member);
    }
    ink_notify_dnd_t d; ink_notify_dnd_state(&d);
    if (!d.active) return true;
    if (!number) return false;
    if ((repeats && repeat) || calls == INK_NOTIFY_CALL_ANY) return true;
    char name[97], matched[25];
    return (calls == INK_NOTIFY_CALL_CONTACTS && ink_contacts_match(number, true, name, matched) > 0)
        || (calls == INK_NOTIFY_CALL_STARRED && ink_contacts_starred(number));
}
void notify_actions_write(ink_jw_t *w, const ink_notify_action_t *actions, int count, const char *cap, const char *args) {
    ink_jw_raw(w, ",\"actions\":[");
    for (int i = 0; i < count; i++) {
        ink_jw_raw(w, i ? ",{\"label\":" : "{\"label\":"); ink_jw_str(w, actions[i].label);
        ink_jw_raw(w, ",\"cap\":"); ink_jw_str(w, actions[i].cap); ink_jw_raw(w, ",\"args\":"); ink_jw_raw(w, actions[i].args[0] ? actions[i].args : "{}"); ink_jw_raw(w, "}");
    }
    ink_jw_raw(w, "]");
    if (cap && cap[0]) { ink_jw_raw(w, ",\"reply\":{\"cap\":"); ink_jw_str(w, cap); ink_jw_raw(w, ",\"args\":"); ink_jw_raw(w, args && args[0] ? args : "{}"); ink_jw_raw(w, "}"); }
}
static void read_args(const ink_json_t *j, int t, char *out, size_t cap) {
    if (t >= 0) snprintf(out, cap, "%.*s", j->tok[t].end - j->tok[t].start, j->text + j->tok[t].start);
}
void notify_actions_read(const ink_json_t *j, ink_notify_view_t *v) {
    int arr = ink_json_find(j, 0, "actions");
    for (int t = arr + 1; arr >= 0 && t < j->ntok && j->tok[t].start < j->tok[arr].end && v->nactions < INK_NOTIFY_ACTIONS_MAX;) {
        ink_notify_action_t *a = &v->actions[v->nactions++];
        ink_json_str(j, ink_json_find(j, t, "label"), a->label, sizeof a->label); ink_json_str(j, ink_json_find(j, t, "cap"), a->cap, sizeof a->cap);
        read_args(j, ink_json_find(j, t, "args"), a->args, sizeof a->args);
        int end = j->tok[t++].end; while (t < j->ntok && j->tok[t].start < end) t++;
    }
    if (!v->nactions && v->action[0]) { v->nactions = 1; snprintf(v->actions[0].label, sizeof v->actions[0].label, "%s", TR("打开")); memcpy(v->actions[0].cap, v->action, sizeof v->action); memcpy(v->actions[0].args, v->action_args, sizeof v->action_args); }
    ink_json_str(j, ink_json_find(j, 0, "reply.cap"), v->reply_cap, sizeof v->reply_cap); read_args(j, ink_json_find(j, 0, "reply.args"), v->reply_args, sizeof v->reply_args);
}
static void forwarded(void *ctx, const ink_receipt_t *rc) {
    ink_call_t *c = ctx; c->rc = *rc; c->rc.result = NULL;
    if (rc->result) ink_cap_result(c, rc->result);
    ink_cap_done(c);
}
typedef struct { ink_exec_job_t job; ink_call_t *call; ink_args_t args; char cap[40]; } dispatch_t;
static void dispatch(void *ctx) { /* UI trampoline: BG targets start on their own stack. */
    dispatch_t *d = ctx; ink_call_t *c = d->call;
    ink_call_opts_t o = {.src = c->src, .member = ink_cap_call_member(c), .reply_exec = INK_EXEC_BG, .done = forwarded, .ctx = c};
    if (ink_cap_call(d->cap, &d->args, &o)) { c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
    osal_free(d);
}
static void perform(ink_call_t *c) {
    ink_notify_view_t *v = osal_malloc(sizeof *v, OSAL_MEM_EXTERNAL); dispatch_t *d = osal_calloc(1, sizeof *d, OSAL_MEM_EXTERNAL);
    int64_t id = 0, index = -1; ink_args_get_int(&c->args, "id", &id); ink_args_get_int(&c->args, "index", &index);
    ink_db_cursor_t cur = {0}; bool found = false; char text[256] = "", why[96], cap[40] = "";
    while (v && ink_notify_next(&cur, v) == 1) if (v->id == (uint32_t)id) { found = true; break; }
    bool reply = !strcmp(c->cap->id, "notify.reply");
    if (found && d) {
        const char *raw = NULL;
        if (reply && v->reply_cap[0]) {
            ink_svc_lock();
            if (index >= 1 && index <= REPLIES) snprintf(text, sizeof text, "%s", P.replies[index - 1][0] ? P.replies[index - 1] : TR(DEFAULTS[index - 1]));
            ink_svc_unlock();
            if (index == -1) ink_args_get_str(&c->args, "text", text, sizeof text);
            if (text[0]) { snprintf(cap, sizeof cap, "%s", v->reply_cap); raw = v->reply_args; }
        } else if (!reply && index >= 1 && index <= v->nactions) { snprintf(cap, sizeof cap, "%s", v->actions[index - 1].cap); raw = v->actions[index - 1].args; }
        if (raw && ink_args_from_json(&d->args, raw[0] ? raw : "{}", raw[0] ? strlen(raw) : 2, NULL, why, sizeof why) == 0 && (!reply || ink_args_str(&d->args, "text", text) == 0)) {
            d->call = c; snprintf(d->cap, sizeof d->cap, "%s", cap);
            ink_cap_defer(c);
            if (ink_exec_post(INK_EXEC_UI, &d->job, dispatch, d)) { osal_free(d); c->rc.status = INK_RC_FAILED; ink_cap_done(c); }
            osal_free(v); return;
        }
    }
    c->rc.status = INK_RC_INVALID; osal_free(v); osal_free(d);
}
static void preferences(ink_call_t *c) {
    load_prefs(); ink_svc_lock(); prefs_t p = P; ink_svc_unlock();
    bool set = !strcmp(c->cap->id, "notify.dnd_set") || !strcmp(c->cap->id, "notify.replies_set");
    int64_t slot = -1, days = 0, start = 0, end = 0, calls = p.calls; bool flag;
    if (set) {
        if (!strcmp(c->cap->id, "notify.replies_set")) {
            ink_args_get_int(&c->args, "index", &slot); int n = ink_args_get_str(&c->args, "text", slot >= 1 && slot <= REPLIES ? p.replies[slot - 1] : NULL, slot >= 1 && slot <= REPLIES ? sizeof p.replies[0] : 0);
            if (slot < 1 || slot > REPLIES || n <= 0 || n >= (int)sizeof p.replies[0]) { c->rc.status = INK_RC_INVALID; return; }
        } else {
            if (ink_args_get_int(&c->args, "slot", &slot)) {
                if (slot < 0 || slot >= SLOTS || !ink_args_get_int(&c->args, "days", &days) || !ink_args_get_int(&c->args, "start", &start) || !ink_args_get_int(&c->args, "end", &end) || days < 0 || days > 127 || start < 0 || start >= 1440 || end < 0 || end >= 1440) { c->rc.status = INK_RC_INVALID; return; }
                p.slots[slot] = (DoNotDisturbScheduleSlot){.used = 1, .enabled = 1, .days = (uint8_t)days, .schedule = {(uint8_t)(start / 60), (uint8_t)(start % 60), (uint8_t)(end / 60), (uint8_t)(end % 60)}};
            } else if (ink_args_find(&c->args, "days") || ink_args_find(&c->args, "start") || ink_args_find(&c->args, "end")) { c->rc.status = INK_RC_INVALID; return; }
            if (ink_args_get_int(&c->args, "calls", &calls)) p.calls = (int)calls;
            if (ink_args_get_bool(&c->args, "repeat", &flag)) p.repeat = flag;
            if (ink_args_get_bool(&c->args, "calendar", &flag)) p.calendar = flag;
        }
    }
    char *out = osal_malloc(PREF_CAP, OSAL_MEM_EXTERNAL);
    if (!out) { c->rc.status = INK_RC_FAILED; return; }
    ink_jw_t w; ink_jw_init(&w, out, PREF_CAP); write_prefs(&w, &p);
    if (set) {
        ink_db_t *db = ink_db_open_member(&PREFS, ink_cap_call_member(c)); ink_db_write_t rec = {.id = 2, .data = out, .len = w.len, .src = c->src};
        if (w.overflow || !db || ink_db_put(db, &rec, NULL) != INK_DB_OK) c->rc.status = INK_RC_FAILED;
        else { ink_svc_lock(); P = p; ink_svc_unlock(); changed(NULL, 0, NULL); }
        ink_db_close(db);
    } else {
        ink_notify_dnd_t d; ink_notify_dnd_state(&d); char b[160];
        if (w.len) w.len--;
        snprintf(b, sizeof b, ",\"active\":%s,\"manual\":%s,\"reason\":\"%s\",\"slot\":%d,\"until_s\":%lld}", d.active ? "true" : "false", ink_setting_get("notify.dnd") ? "true" : "false", d.reason, d.slot, (long long)d.until_s); ink_jw_raw(&w, b);
        if (!w.overflow) ink_cap_result(c, out); else c->rc.status = INK_RC_FAILED;
    }
    osal_free(out);
}
static const ink_cap_t CAPS[] = {
    {.id = "notify.act", .title = "执行通知动作（序号从 1 开始）", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1},\"index\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":3}},\"required\":[\"id\",\"index\"]}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .timeout_ms = 35000, .run = perform},
    {.id = "notify.reply", .title = "回复通知（快捷回复 index 1 至 5，或 text）", .input = "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"integer\",\"minimum\":1},\"index\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":5},\"text\":{\"type\":\"string\",\"maxLength\":63}},\"required\":[\"id\"]}", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .timeout_ms = 35000, .run = perform},
    {.id = "notify.dnd_get", .title = "勿扰状态、时段、例外和快捷回复", .effect = INK_FX_READ, .child_ok = true, .exec = INK_EXEC_BG, .run = preferences},
    {.id = "notify.dnd_set", .title = "修改自己的勿扰时段和例外（时段同时给 slot、days、start、end，分钟从零起）", .input = "{\"type\":\"object\",\"properties\":{\"slot\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":4},\"days\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":127},\"start\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":1439},\"end\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":1439},\"calls\":{\"type\":\"integer\",\"minimum\":0,\"maximum\":3},\"repeat\":{\"type\":\"boolean\"},\"calendar\":{\"type\":\"boolean\"}}}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = preferences},
    {.id = "notify.replies_set", .title = "修改自己的快捷回复", .input = "{\"type\":\"object\",\"properties\":{\"index\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":5},\"text\":{\"type\":\"string\",\"maxLength\":31}},\"required\":[\"index\",\"text\"]}", .effect = INK_FX_LOCAL_WRITE, .child_ok = true, .exec = INK_EXEC_BG, .run = preferences},
    {.id = "notify.dnd_changed", .title = "勿扰状态或偏好有变化", .form = INK_FORM_EVENT, .effect = INK_FX_READ, .child_ok = true},
};
int notify_model_init(void) {
    static const ink_state_def_t defs[] = {{.id = "notify.dnd_active", .type = INK_ST_BOOL, .title = "正在勿扰"}, {.id = "notify.dnd_reason", .type = INK_ST_STR, .title = "勿扰原因"}, {.id = "notify.dnd_until", .type = INK_ST_INT, .title = "勿扰结束时间"}};
    h_active = ink_state_define(&defs[0]); h_reason = ink_state_define(&defs[1]); h_until = ink_state_define(&defs[2]);
    if (h_active < 0 || h_reason < 0 || h_until < 0 || ink_cap_register(CAPS, sizeof CAPS / sizeof *CAPS)) return -1;
    static ink_exec_job_t boot;
    if (ink_exec_post(INK_EXEC_BG, &boot, boot_model, NULL)) return -1;
    return ink_state_watch("time.minute", changed, NULL, INK_EXEC_BG) || ink_state_watch("alarm.revision", changed, NULL, INK_EXEC_BG) || ink_state_watch("member.current", changed, NULL, INK_EXEC_BG) || ink_settings_watch("notify.dnd", changed, NULL, INK_EXEC_BG) || ink_cap_subscribe("time.changed", time_changed, NULL, INK_EXEC_BG) ? -1 : 0;
}
